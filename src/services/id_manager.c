/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file id_manager.c
 *
 * @brief Orchestrates access control: turns RFID taps and schedule state
 *        changes into HTTP calls and schedule_cmd_chan commands.
 *
 * Runs on its own thread (zbus subscriber, not listener) because it performs
 * blocking HTTP I/O that must not stall the RFID polling thread.
 *
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 01/07/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "endpoints/checkout.h"
#include "endpoints/confirm_schedule.h"
#include "endpoints/schedule.h"
#include "services/rfid.h"
#include "services/schedule.h"
#include "services/sntp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(id_manager, CONFIG_ID_MANAGER_LOG_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(id_manager_sub, CONFIG_ID_MANAGER_SUB_QUEUE_SIZE);

ZBUS_CHAN_ADD_OBS(rfid_tag_chan, id_manager_sub, 2);
ZBUS_CHAN_ADD_OBS(schedule_state_chan, id_manager_sub, 2);

static struct {
	bool tap_pending;
	bool confirm_pending;
	int64_t confirm_deadline;
	int64_t long_tap_deadline;
	int64_t next_fetch_at;
} self;

/* TODO @Jota: implement this in a separate file (server.c component, or something like that)*/
static int connect_to_server(void)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res;
	int sock;
	int ret;

	ret = zsock_getaddrinfo(CONFIG_SERVER_HOST, CONFIG_SERVER_PORT, &hints, &res);
	if (ret != 0) {
		LOG_ERR("DNS lookup failed for %s: %d", CONFIG_SERVER_HOST, ret);
		return -ENOENT;
	}

	sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		ret = -errno;
		LOG_ERR("Socket creation failed: %d", ret);
		zsock_freeaddrinfo(res);
		return ret;
	}

	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	zsock_freeaddrinfo(res);

	if (ret < 0) {
		ret = -errno;
		LOG_ERR("Connect to %s:%s failed: %d", CONFIG_SERVER_HOST, CONFIG_SERVER_PORT, ret);
		(void)zsock_close(sock);
		return ret;
	}

	return sock;
}

static void do_fetch(void)
{
	int sock;
	int ret;

	sock = connect_to_server();
	if (sock < 0) {
		return;
	}

	ret = schedule_endpoint_fetch(sock, CONFIG_SERVER_HOST, CONFIG_SERVER_PORT);
	(void)zsock_close(sock);

	if (ret < 0) {
		LOG_WRN("periodic schedule fetch failed: %d", ret);
	}
}

static void do_confirm(void)
{
	struct baiatool_schedule_state state;
	struct baiatool_schedule_cmd cmd = {.cmd_id = SCHEDULE_CMD_FIRST_USE};
	int sock;
	int ret;

	if (zbus_chan_read(&schedule_state_chan, &state, K_MSEC(200)) != 0) {
		LOG_ERR("failed to read schedule state before confirm");
		return;
	}

	sock = connect_to_server();
	if (sock < 0) {
		return;
	}

	ret = confirm_schedule_endpoint_post(sock, CONFIG_SERVER_HOST, CONFIG_SERVER_PORT);
	(void)zsock_close(sock);
	if (ret < 0) {
		LOG_ERR("confirm_schedule failed: %d", ret);
		return;
	}

	memcpy(cmd.user_id, state.user_id, CONFIG_MAX_ID_LENGTH);
	(void)zbus_chan_pub(&schedule_cmd_chan, &cmd, K_MSEC(300));
}

static void do_extend(void)
{
	struct baiatool_schedule_state state;
	struct baiatool_schedule_cmd cmd = {.cmd_id = SCHEDULE_CMD_EXTEND_TIME};

	if (zbus_chan_read(&schedule_state_chan, &state, K_MSEC(200)) != 0) {
		LOG_ERR("failed to read schedule state before extend");
		return;
	}

	/* TODO @Jota: replace with a real extend_schedule_endpoint_post() call once
	 * the /api/schedule/extend endpoint is implemented in a follow-up branch.
	 * For now, publish EXTEND_TIME directly so the rest of the flow can be
	 * exercised end-to-end.
	 */
	LOG_WRN("extend endpoint not implemented yet; publishing EXTEND_TIME without HTTP call");

	memcpy(cmd.user_id, state.user_id, CONFIG_MAX_ID_LENGTH);
	(void)zbus_chan_pub(&schedule_cmd_chan, &cmd, K_MSEC(300));
}

static void do_checkout(void)
{
	struct baiatool_schedule_state state;
	struct baiatool_schedule_cmd cmd = {.cmd_id = SCHEDULE_CMD_END_USE};
	int sock;
	int ret;

	if (zbus_chan_read(&schedule_state_chan, &state, K_MSEC(200)) != 0) {
		LOG_ERR("failed to read schedule state before checkout");
		return;
	}

	sock = connect_to_server();
	if (sock < 0) {
		return;
	}

	ret = checkout_endpoint_post(sock, CONFIG_SERVER_HOST, CONFIG_SERVER_PORT);
	(void)zsock_close(sock);
	if (ret < 0) {
		LOG_ERR("checkout failed: %d", ret);
		return;
	}

	memcpy(cmd.user_id, state.user_id, CONFIG_MAX_ID_LENGTH);
	(void)zbus_chan_pub(&schedule_cmd_chan, &cmd, K_MSEC(300));
}

static void handle_short_tap(const uint8_t *uid, uint8_t uid_len)
{
	struct baiatool_schedule_state state;
	struct sntp_service_time_msg now_msg;

	if (zbus_chan_read(&schedule_state_chan, &state, K_MSEC(200)) != 0) {
		LOG_ERR("failed to read schedule state for tap decision");
		return;
	}

	if (memcmp(uid, state.user_id, MIN(uid_len, CONFIG_MAX_ID_LENGTH)) != 0) {
		LOG_WRN("tap uid does not match current schedule user, ignoring");
		return;
	}

	switch (state.last_cmd) {
	case SCHEDULE_CMD_END_USE:
	case SCHEDULE_CMD_LOAD:
		do_confirm();
		break;

	case SCHEDULE_CMD_FIRST_USE:
		if (zbus_chan_read(&sntp_time_chan, &now_msg, K_MSEC(100)) != 0 || !now_msg.valid) {
			LOG_WRN("cannot evaluate extend guard: SNTP time unavailable");
			break;
		}

		if ((time_t)now_msg.unix_time_s - state.start_time <
		    (CONFIG_ID_MANAGER_MIN_USE_BEFORE_EXTEND_MIN * 60)) {
			LOG_WRN("extend rejected: less than %d min since first use",
				CONFIG_ID_MANAGER_MIN_USE_BEFORE_EXTEND_MIN);
			break;
		}

		do_extend();
		break;

	case SCHEDULE_CMD_EXTEND_TIME:
		LOG_WRN("extend already used once for this session, ignoring tap");
		break;

	default:
		break;
	}
}

static void handle_rfid_event(void)
{
	struct rfid_event ev;

	if (zbus_chan_read(&rfid_tag_chan, &ev, K_MSEC(50)) != 0) {
		return;
	}

	if (ev.type == RFID_EVT_PRESENT) {
		self.tap_pending = true;
		self.long_tap_deadline = k_uptime_get() + CONFIG_ID_MANAGER_LONG_TAP_MS;
		return;
	}

	/* RFID_EVT_REMOVED */
	if (!self.tap_pending) {
		return; /* already handled as a long tap, or a spurious removal */
	}
	self.tap_pending = false;

	if (ev.duration_ms >= CONFIG_ID_MANAGER_LONG_TAP_MS) {
		do_checkout();
		return;
	}

	handle_short_tap(ev.uid_bytes, ev.uid_len);
}

static void handle_schedule_state_change(void)
{
	struct baiatool_schedule_state state;

	if (zbus_chan_read(&schedule_state_chan, &state, K_MSEC(50)) != 0) {
		return;
	}

	if (state.last_cmd == SCHEDULE_CMD_LOAD) {
		self.confirm_pending = true;
		self.confirm_deadline =
			k_uptime_get() + (CONFIG_ID_MANAGER_CONFIRM_TIMEOUT_MIN * 60 * 1000);
	} else {
		self.confirm_pending = false;
	}
}

static void id_manager_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	self.next_fetch_at = k_uptime_get();

	while (true) {
		int64_t now = k_uptime_get();
		int64_t soonest = self.next_fetch_at;
		int64_t wait_ms;
		int ret;

		if (self.tap_pending && self.long_tap_deadline < soonest) {
			soonest = self.long_tap_deadline;
		}
		if (self.confirm_pending && self.confirm_deadline < soonest) {
			soonest = self.confirm_deadline;
		}

		wait_ms = soonest - now;
		if (wait_ms < 0) {
			wait_ms = 0;
		}

		ret = zbus_sub_wait(&id_manager_sub, &chan, K_MSEC(wait_ms));

		if (ret == 0) {
			if (chan == &rfid_tag_chan) {
				handle_rfid_event();
			} else if (chan == &schedule_state_chan) {
				handle_schedule_state_change();
			}
			continue;
		}

		now = k_uptime_get();

		if (self.tap_pending && now >= self.long_tap_deadline) {
			self.tap_pending = false;
			do_checkout();
		}

		if (self.confirm_pending && now >= self.confirm_deadline) {
			self.confirm_pending = false;
			do_checkout();
		}

		if (now >= self.next_fetch_at) {
			do_fetch();
			self.next_fetch_at = now + (CONFIG_ID_MANAGER_FETCH_INTERVAL_SEC * 1000);
		}
	}
}

K_THREAD_DEFINE(id_manager_thread, CONFIG_ID_MANAGER_THREAD_STACK_SIZE, id_manager_thread_fn, NULL,
		NULL, NULL, CONFIG_ID_MANAGER_THREAD_PRIORITY, 0, 0);
