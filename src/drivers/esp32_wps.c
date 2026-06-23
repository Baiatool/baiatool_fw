/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file esp32_wps.c
 *
 * @brief Thin driver wrapping ESP32 HAL WPS. All <esp_wifi.h> and
 *        <esp_wps.h> usage is confined here so the service layer
 *        stays free of HAL dependencies.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <strings.h>

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_wps.h>

#include "drivers/esp32_wps.h"

LOG_MODULE_REGISTER(esp32_wps, CONFIG_WIFI_LOG_LEVEL);

static esp32_wps_result_cb_t wps_result_cb;

/*
 * True when esp_wifi_start() was called by this driver for WPS.
 * Cleared by esp32_wps_stop(); checked internally before calling
 * esp_wifi_stop() to avoid double-stopping.
 */
static bool wps_wifi_started;

/*
 * Wraps esp_event_post (--wrap=esp_event_post) to intercept WPS events
 * before forwarding to the HAL stub (__real_esp_event_post → esp_wifi_event_handler).
 */
extern esp_err_t __real_esp_event_post(esp_event_base_t event_base, int32_t event_id,
				       const void *event_data, size_t event_data_size,
				       uint32_t ticks_to_wait);

esp_err_t __wrap_esp_event_post(esp_event_base_t event_base, int32_t event_id,
				const void *event_data, size_t event_data_size,
				uint32_t ticks_to_wait)
{
	if (event_base != WIFI_EVENT) {
		return __real_esp_event_post(event_base, event_id, event_data, event_data_size,
					     ticks_to_wait);
	}

	switch (event_id) {
	case WIFI_EVENT_STA_WPS_ER_SUCCESS: {
		const wifi_event_sta_wps_er_success_t *wps = event_data;
		char ssid[33] = {0};
		char psk[65] = {0};

		if (wps && event_data_size >= sizeof(*wps)) {
			strncpy(ssid, (const char *)wps->ap_cred[0].ssid, sizeof(ssid) - 1);
			strncpy(psk, (const char *)wps->ap_cred[0].passphrase, sizeof(psk) - 1);
		}

		esp_wifi_wps_disable();
		LOG_INF("WPS success: SSID=%s", ssid);

		if (wps_result_cb) {
			wps_result_cb(ssid, psk, 0);
		}
		break;
	}
	case WIFI_EVENT_STA_WPS_ER_FAILED:
		LOG_ERR("WPS failed");
		esp_wifi_wps_disable();
		esp32_wps_stop();
		if (wps_result_cb) {
			wps_result_cb("", "", -EIO);
		}
		break;
	case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
		LOG_ERR("WPS timeout");
		esp_wifi_wps_disable();
		esp32_wps_stop();
		if (wps_result_cb) {
			wps_result_cb("", "", -ETIMEDOUT);
		}
		break;
	default:
		return __real_esp_event_post(event_base, event_id, event_data, event_data_size,
					     ticks_to_wait);
	}

	return ESP_OK;
}

int esp32_wps_start(esp32_wps_result_cb_t cb)
{
	/* WPS_CONFIG_INIT_DEFAULT needs CONFIG_IDF_TARGET (absent in Zephyr);
	 * zero factory_info fields use ESP-IDF defaults per struct docs. */
	esp_wps_config_t config = {.wps_type = WPS_TYPE_PBC};
	int ret;

	if (!cb) {
		return -EINVAL;
	}

	wps_result_cb = cb;

	esp_wifi_set_mode(ESP32_WIFI_MODE_STA);
	ret = esp_wifi_start();
	if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
		LOG_ERR("WPS: WiFi HAL start failed: %d", ret);
		return -EIO;
	}
	wps_wifi_started = true;

	ret = esp_wifi_wps_enable(&config);
	if (ret != ESP_OK) {
		LOG_ERR("WPS enable failed: %d", ret);
		esp32_wps_stop();
		return -EIO;
	}

	ret = esp_wifi_wps_start();
	if (ret != ESP_OK) {
		LOG_ERR("WPS start failed: %d", ret);
		esp_wifi_wps_disable();
		esp32_wps_stop();
		return -EIO;
	}

	return 0;
}

void esp32_wps_stop(void)
{
	if (!wps_wifi_started) {
		return;
	}

	esp_wifi_stop();
	wps_wifi_started = false;
}