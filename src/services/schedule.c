/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file schedule.c
 *
 * @brief Implements the scheduling management service for the workstation.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 24/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "components/storage.h"
#include "services/schedule.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(schedule, CONFIG_SCHEDULE_LOG_LEVEL);

static void schedule_chan_listener(const struct zbus_channel *chan);
static void schedule_storage_update(const struct zbus_channel *chan);

ZBUS_CHAN_DEFINE(schedule_chan_cmd, struct baiatool_schedule_cmd, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.cmd_id = SCHEDULE_CMD_END_USE, .user_id = {0}));

ZBUS_CHAN_DEFINE(schedule_chan_state, struct baiatool_schedule_state, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.start_time = 0, .end_time = 0, .user_id = {0},
			       .last_cmd = SCHEDULE_CMD_END_USE));

ZBUS_LISTENER_DEFINE(schedule_listener, schedule_chan_listener);
ZBUS_LISTENER_DEFINE(schedule_state_cb, schedule_storage_update);

ZBUS_CHAN_ADD_OBS(schedule_chan_cmd, schedule_listener, 1);
ZBUS_CHAN_ADD_OBS(schedule_chan_state, schedule_state_cb, 1);

static void schedule_chan_listener(const struct zbus_channel *chan)
{
	int err;
	bool changed = false;
	uint8_t empty_id[CONFIG_MAX_ID_LENGTH] = {0};
	const struct baiatool_schedule_cmd *msg;
	struct baiatool_schedule_state *state;

	msg = zbus_chan_const_msg(chan);

	err = zbus_chan_claim(&schedule_chan_state, K_MSEC(200));
	if (err < 0) {
		LOG_ERR("Failed to claim channel: %d", err);
		return;
	}

	state = zbus_chan_msg(&schedule_chan_state);

	switch (msg->cmd_id) {

	case SCHEDULE_CMD_FIRST_USE:
		if (memcmp(msg->user_id, empty_id, CONFIG_MAX_ID_LENGTH) == 0) {
			LOG_ERR("User ID cannot be NULL");
			goto finish;
		}

		if (state->last_cmd != SCHEDULE_CMD_END_USE) {
			LOG_ERR("Last command is not END_USE: %d", state->last_cmd);

			goto finish;
		}

		LOG_INF("Received FIRST_USE command from user ID: %s", msg->user_id);
		state->start_time =
			0; /* TODO @Jota: Pegar a hora atual no serviço de data e hora */
		state->end_time =
			CONFIG_SCHEDULE_FIRST_USE_TIMEOUT_MINUTES; /* TODO @Jota: Pegar a hora final
								      n o serviço de data e hora */
		state->last_cmd = SCHEDULE_CMD_FIRST_USE;

		memcpy(state->user_id, msg->user_id, CONFIG_MAX_ID_LENGTH);
		changed = true;

		goto finish;

	case SCHEDULE_CMD_EXTEND_TIME:
		if (state->last_cmd == SCHEDULE_CMD_END_USE) {
			LOG_ERR("Last command is not FIRST_USE or EXTEND_TIME: %d",
				state->last_cmd);

			goto finish;
		}

		LOG_INF("Received EXTEND_TIME command from user ID: %s", msg->user_id);

		if (memcmp(state->user_id, msg->user_id, CONFIG_MAX_ID_LENGTH) != 0) {
			LOG_ERR("User ID mismatch: command from %s but schedule is for %s",
				msg->user_id, state->user_id);
			break;
		}

		state->end_time +=
			CONFIG_SCHEDULE_EXTEND_TIMEOUT_MINUTES; /* TODO @Jota: Pegar a hora final no
								   serviço de data e hora */
		state->last_cmd = SCHEDULE_CMD_EXTEND_TIME;
		changed = true;

		goto finish;

	case SCHEDULE_CMD_END_USE:
		if (memcmp(msg->user_id, empty_id, CONFIG_MAX_ID_LENGTH) == 0) {
			LOG_ERR("User ID cannot be NULL");
			goto finish;
		}

		if (state->last_cmd == SCHEDULE_CMD_END_USE) {
			LOG_ERR("Last command is already END_USE: %d", state->last_cmd);

			goto finish;
		}

		LOG_INF("Received END_USE command from user ID: %s", msg->user_id);

		if (memcmp(state->user_id, msg->user_id, CONFIG_MAX_ID_LENGTH) != 0) {
			LOG_ERR("User ID mismatch: command from %s but schedule is for %s",
				msg->user_id, state->user_id);
			break;
		}

		memset(state->user_id, 0, CONFIG_MAX_ID_LENGTH);
		LOG_INF("ID after clearing: %s", state->user_id);
		state->start_time = 0;
		state->end_time = 0;
		state->last_cmd = SCHEDULE_CMD_END_USE;
		changed = true;

		goto finish;

	default:
		LOG_WRN("Received unknown command ID: %d from user ID: %s", msg->cmd_id,
			msg->user_id);

		goto finish;
	}

finish:
	err = zbus_chan_finish(&schedule_chan_state);
	if (err < 0) {
		LOG_ERR("Failed to finish channel: %d", err);
		return;
	}

	if (!changed) {
		return;
	}

	err = zbus_chan_notify(&schedule_chan_state, K_MSEC(300));
	if (err < 0) {
		LOG_ERR("Failed to notify channel: %d", err);
		return;
	}
}

static void schedule_storage_update(const struct zbus_channel *chan)
{
	int ret;
	struct baiatool_schedule_state *state;

	state = zbus_chan_msg(chan);

	ret = baiatool_storage_update(BAIATOOL_SCHEDULE_NVS_ID, state,
				      sizeof(struct baiatool_schedule_state));
	if (ret < 0) {
		LOG_ERR("Failed to update schedule storage: %d", ret);
		return;
	}

	LOG_DBG("Current schedule saved in Flash");
}