/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file wifi.c
 *
 * @brief Implements the wifi management service.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 22/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "services/wifi.h"

LOG_MODULE_REGISTER(wifi, CONFIG_WIFI_LOG_LEVEL);

#define WIFI_EVENT_MASK (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

ZBUS_SUBSCRIBER_DEFINE(wifi_cmd_sub, 4);

ZBUS_CHAN_DEFINE(wifi_cmd_chan, struct wifi_cmd_msg, NULL, NULL, ZBUS_OBSERVERS(wifi_cmd_sub),
		 ZBUS_MSG_INIT(.type = WIFI_CMD_DISCONNECT));

ZBUS_CHAN_DEFINE(wifi_state_chan, struct wifi_state_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = WIFI_BAIATOOL_STATE_DISCONNECTED));

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_evt_cb;

static inline void publish_state(enum wifi_baiatool_state state)
{
	struct wifi_state_msg msg = {.state = state};

	zbus_chan_pub(&wifi_state_chan, &msg, K_MSEC(100));
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
#ifdef CONFIG_NET_MGMT_EVENT_INFO
	{
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status) {
			LOG_ERR("Connect failed: %d", status->status);
			publish_state(WIFI_BAIATOOL_STATE_FAILED);
			break;
		}
	}
#endif
		LOG_INF("Connected");
		publish_state(WIFI_BAIATOOL_STATE_CONNECTED);
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Disconnected");
		publish_state(WIFI_BAIATOOL_STATE_DISCONNECTED);
		break;

	default:
		break;
	}
}

/**
 * TODO: When the storage service is implemented, this function should be modified to retrieve the
 * SSID and PSK, or credentials, from the storage instead of receiving them as parameters. @JoaoMatheusND
 *
 */
static inline void wifi_do_connect(const char *ssid, const char *psk)
{
	struct wifi_connect_req_params params = {0};
	int ret;

	params.ssid = (const uint8_t *)ssid;
	params.ssid_length = (uint8_t)strlen(ssid);
	params.channel = WIFI_CHANNEL_ANY;
	params.band = WIFI_FREQ_BAND_UNKNOWN;
	params.mfp = WIFI_MFP_OPTIONAL;
	params.timeout = SYS_FOREVER_MS;

	if (psk[0] != '\0') {
		params.psk = (const uint8_t *)psk;
		params.psk_length = (uint8_t)strlen(psk);
		params.security = WIFI_SECURITY_TYPE_PSK;
	} else {
		params.security = WIFI_SECURITY_TYPE_NONE;
	}

	publish_state(WIFI_BAIATOOL_STATE_CONNECTING);

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params,
		       sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Connect request failed: %d", ret);
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
	}
}

static inline void wifi_do_connect_wps(void)
{
	struct wifi_wps_config_params params = {.oper = WIFI_WPS_PBC};
	int ret;

	publish_state(WIFI_BAIATOOL_STATE_CONNECTING);

	ret = net_mgmt(NET_REQUEST_WIFI_WPS_CONFIG, wifi_iface, &params,
		       sizeof(struct wifi_wps_config_params));
	if (ret) {
		LOG_ERR("WPS PBC request failed: %d", ret);
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
		return;
	}

	LOG_INF("WPS PBC started — press router button");
}

static inline void wifi_do_disconnect(void)
{
	int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);

	if (ret) {
		LOG_ERR("Disconnect request failed: %d", ret);
	}
}

static void wifi_cmd_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	/* Wait for WiFi STA interface to be ready */
	do {
		wifi_iface = net_if_get_wifi_sta();
		if (!wifi_iface) {
			k_sleep(K_MSEC(500));
		}
	} while (!wifi_iface);

	net_mgmt_init_event_callback(&wifi_evt_cb, wifi_event_handler, WIFI_EVENT_MASK);
	net_mgmt_add_event_callback(&wifi_evt_cb);

	LOG_INF("WiFi service ready");

	while (!zbus_sub_wait(&wifi_cmd_sub, &chan, K_FOREVER)) {
		struct wifi_cmd_msg cmd;

		if (zbus_chan_read(chan, &cmd, K_MSEC(100))) {
			continue;
		}

		switch (cmd.type) {
		case WIFI_CMD_CONNECT:
			wifi_do_connect(cmd.ssid, cmd.psk);
			break;
		case WIFI_CMD_CONNECT_WPS:
			wifi_do_connect_wps();
			break;
		case WIFI_CMD_DISCONNECT:
			wifi_do_disconnect();
			break;
		default:
			LOG_WRN("Unknown cmd: %d", cmd.type);
			break;
		}
	}
}

K_THREAD_DEFINE(wifi_baiatool_tid, CONFIG_WIFI_THREAD_STACK_SIZE, wifi_cmd_thread_fn, NULL, NULL,
		NULL, CONFIG_WIFI_THREAD_PRIORITY, 0, 0);
