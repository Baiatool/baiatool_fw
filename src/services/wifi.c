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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wps.h"

#include "components/storage.h"
#include "services/wifi.h"

struct wifi_credentials {
	char ssid[CONFIG_WIFI_SSID_MAX_LEN + 1];
	char psk[CONFIG_WIFI_PSK_MAX_LEN + 1];
};

/*
 * Static pool for z_strdup — the WPS/wpa_supplicant library requires this
 * symbol but the Zephyr ESP32 HAL port (zephyr/port/wifi/wpa_supplicant/
 * os_xtensa.c) does not define it.  A fixed-size ring buffer satisfies the
 * no-dynamic-allocation rule; STRDUP_SLOTS concurrent live copies are
 * supported before the oldest entry is silently recycled.
 */
#define STRDUP_SLOTS     8
#define STRDUP_SLOT_SIZE 128

static char strdup_pool[STRDUP_SLOTS][STRDUP_SLOT_SIZE];
static uint8_t strdup_idx;

char *z_strdup(const char *s)
{
	char *p = strdup_pool[strdup_idx % STRDUP_SLOTS];

	strdup_idx++;
	strncpy(p, s, STRDUP_SLOT_SIZE - 1);
	p[STRDUP_SLOT_SIZE - 1] = '\0';
	return p;
}

int z_strcasecmp(const char *s1, const char *s2)
{
	return z_strcasecmp(s1, s2);
}

LOG_MODULE_REGISTER(wifi, CONFIG_WIFI_LOG_LEVEL);

#define WIFI_EVENT_MASK (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

ZBUS_SUBSCRIBER_DEFINE(wifi_cmd_sub, CONFIG_WIFI_CMD_SUB_QUEUE_SIZE);

ZBUS_CHAN_DEFINE(wifi_cmd_chan, struct wifi_cmd_msg, NULL, NULL, ZBUS_OBSERVERS(wifi_cmd_sub),
		 ZBUS_MSG_INIT(.type = WIFI_CMD_DISCONNECT));

ZBUS_CHAN_DEFINE(wifi_state_chan, struct wifi_state_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = WIFI_BAIATOOL_STATE_DISCONNECTED));

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_evt_cb;
static enum wifi_baiatool_state current_state = WIFI_BAIATOOL_STATE_DISCONNECTED;

/*
 * @note: Set when WPS starts WiFi directly via the HAL (ESP32 driver does not
 * implement the net_mgmt wps_config op).  Cleared in wifi_do_connect after
 * stopping WiFi so that the driver can restart it cleanly through net_mgmt.
 */
static bool wps_pending;

static inline void publish_state(enum wifi_baiatool_state state)
{
	struct wifi_state_msg msg = {.state = state};

	current_state = state;
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
		if (current_state == WIFI_BAIATOOL_STATE_CONNECTING) {
			LOG_ERR("Connection failed: invalid SSID or password");
			publish_state(WIFI_BAIATOOL_STATE_FAILED);
		} else {
			LOG_INF("Disconnected");
			publish_state(WIFI_BAIATOOL_STATE_DISCONNECTED);
		}
		break;

	default:
		break;
	}
}

static inline void wifi_do_connect(const char *ssid, const char *psk);

static void wifi_save_credentials(enum baiatool_storage_id id, const char *ssid, const char *psk)
{
	struct wifi_credentials creds;

	strncpy(creds.ssid, ssid, CONFIG_WIFI_SSID_MAX_LEN);
	creds.ssid[CONFIG_WIFI_SSID_MAX_LEN] = '\0';
	strncpy(creds.psk, psk, CONFIG_WIFI_PSK_MAX_LEN);
	creds.psk[CONFIG_WIFI_PSK_MAX_LEN] = '\0';

	if (baiatool_storage_update(id, &creds, sizeof(creds)) < 0) {
		LOG_ERR("Failed to save credentials (id=%u)", id);
	}
}

static void wifi_auto_connect(void)
{
	struct wifi_credentials creds;
	ssize_t ret;

	ret = baiatool_storage_read(BAIATOOL_PROVISIONING_NVS_ID, &creds, sizeof(creds));
	if (ret > 0) {
		LOG_INF("WPS credentials found, connecting to %s", creds.ssid);
		wifi_do_connect(creds.ssid, creds.psk);
		return;
	}

	ret = baiatool_storage_read(BAIATOOL_NET_SETTINGS_NVS_ID, &creds, sizeof(creds));
	if (ret > 0) {
		LOG_INF("Stored credentials found, connecting to %s", creds.ssid);
		wifi_do_connect(creds.ssid, creds.psk);
		return;
	}

	LOG_WRN("No wifi connection data available");
}

static inline void wifi_do_connect(const char *ssid, const char *psk)
{
	struct wifi_connect_req_params params = {0};
	int ret;

	if (wps_pending) {
		/* WiFi was started directly via the HAL for WPS, so the Zephyr
		 * driver's esp_wifi_start() guard would reject the next connect
		 * request.  Stop WiFi first to reset driver state. */
		esp_wifi_stop();
		wps_pending = false;
	}

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
	LOG_INF("Connecting to SSID: %s", ssid);

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params,
		       sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Connect request rejected: %d", ret);
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
	}
}

/*
 * @note: esp_event_post intercepts WPS events posted by the wpa_supplicant layer.
 * The Zephyr ESP32 port does not compile esp_event.c, so this shim provides
 * the symbol and routes WPS outcomes back through the Zephyr stack.
 *
 * On success: credentials extracted from the WPS result are published as a
 * WIFI_CMD_CONNECT message so the wifi thread completes the association via
 * net_mgmt (the normal, driver-managed path).
 *
 * On failure/timeout: WiFi is stopped to reset Zephyr driver state so that
 * subsequent net_mgmt connect requests succeed.
 */
esp_err_t esp_event_post(esp_event_base_t event_base, int32_t event_id, const void *event_data,
			 size_t event_data_size, uint32_t ticks_to_wait)
{
	ARG_UNUSED(ticks_to_wait);

	if (event_base != WIFI_EVENT) {
		return ESP_OK;
	}

	switch (event_id) {
	case WIFI_EVENT_STA_WPS_ER_SUCCESS: {
		const wifi_event_sta_wps_er_success_t *wps = event_data;
		struct wifi_cmd_msg cmd = {.type = WIFI_CMD_CONNECT};

		if (wps && event_data_size >= sizeof(*wps)) {
			strncpy(cmd.ssid, (const char *)wps->ap_cred[0].ssid,
				CONFIG_WIFI_SSID_MAX_LEN);
			cmd.ssid[CONFIG_WIFI_SSID_MAX_LEN] = '\0';
			strncpy(cmd.psk, (const char *)wps->ap_cred[0].passphrase,
				CONFIG_WIFI_PSK_MAX_LEN);
			cmd.psk[CONFIG_WIFI_PSK_MAX_LEN] = '\0';
			wifi_save_credentials(BAIATOOL_PROVISIONING_NVS_ID, cmd.ssid, cmd.psk);
		}
		esp_wifi_wps_disable();
		wps_pending = true;
		zbus_chan_pub(&wifi_cmd_chan, &cmd, K_MSEC(500));
		LOG_INF("WPS success, connecting to %s", cmd.ssid);
		break;
	}
	case WIFI_EVENT_STA_WPS_ER_FAILED:
		LOG_ERR("WPS failed");
		esp_wifi_wps_disable();
		esp_wifi_stop();
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
		break;
	case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
		LOG_ERR("WPS timeout");
		esp_wifi_wps_disable();
		esp_wifi_stop();
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
		break;
	default:
		break;
	}

	return ESP_OK;
}

static inline void wifi_do_connect_wps(void)
{
	/* WPS_CONFIG_INIT_DEFAULT needs CONFIG_IDF_TARGET (absent in Zephyr); zero
	 * factory_info fields use ESP-IDF defaults per struct field documentation. */
	esp_wps_config_t config = {.wps_type = WPS_TYPE_PBC};
	int ret;

	publish_state(WIFI_BAIATOOL_STATE_CONNECTING);

	/* @note: net_mgmt has no WPS support on ESP32: the driver does not implement
	 * the wps_config op (NET_REQUEST_WIFI_WPS_CONFIG returns -ENOTSUP).
	 * WiFi must be started via the HAL before enabling WPS; the credential
	 * exchange is handled by esp_event_post above, which re-routes the
	 * association back through net_mgmt once credentials are obtained. */
	esp_wifi_set_mode(ESP32_WIFI_MODE_STA);
	ret = esp_wifi_start();
	if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
		LOG_ERR("WPS: WiFi start failed: %d", ret);
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
		return;
	}

	ret = esp_wifi_wps_enable(&config);
	if (ret != ESP_OK) {
		LOG_ERR("WPS enable failed: %d", ret);
		publish_state(WIFI_BAIATOOL_STATE_FAILED);
		return;
	}

	ret = esp_wifi_wps_start(0);
	if (ret != ESP_OK) {
		LOG_ERR("WPS start failed: %d", ret);
		esp_wifi_wps_disable();
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
	wifi_auto_connect();

	while (!zbus_sub_wait(&wifi_cmd_sub, &chan, K_FOREVER)) {
		struct wifi_cmd_msg cmd;

		if (zbus_chan_read(chan, &cmd, K_MSEC(100))) {
			continue;
		}

		switch (cmd.type) {
		case WIFI_CMD_CONNECT:
			if (cmd.persist) {
				wifi_save_credentials(BAIATOOL_NET_SETTINGS_NVS_ID, cmd.ssid,
						      cmd.psk);
			}
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
