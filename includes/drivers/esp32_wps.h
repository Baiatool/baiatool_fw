/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file esp32_wps.h
 *
 * @brief Thin driver wrapping ESP32 HAL WPS — isolates <esp_wifi.h>
 *        and <esp_wps.h> from the application service layer.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef DRIVERS_ESP32_WPS_H_
#define DRIVERS_ESP32_WPS_H_

/**
 * @brief Callback invoked by the WPS driver when the exchange completes.
 *
 * @param ssid  NUL-terminated SSID obtained via WPS, or empty string on error.
 * @param psk   NUL-terminated passphrase obtained via WPS, or empty string on error.
 * @param err   0 on success, negative errno on failure or timeout.
 */
typedef void (*esp32_wps_result_cb_t)(const char *ssid, const char *psk, int err);

/**
 * @brief Start WPS PBC (Push-Button Configuration).
 *
 * Starts the ESP32 WiFi HAL in STA mode and triggers WPS negotiation.
 * The result callback is invoked from the ESP32 event context once
 * credentials are obtained or the exchange fails/times out.
 *
 * @param cb  Callback to invoke on completion. Must not be NULL.
 *
 * @retval 0        WPS started successfully.
 * @retval -EINVAL  cb is NULL.
 * @retval -EIO     HAL returned an unexpected error.
 */
int esp32_wps_start(esp32_wps_result_cb_t cb);

/**
 * @brief Stop WPS and release HAL WiFi state.
 *
 * Must be called before issuing a net_mgmt connect request if WPS was
 * previously started, because the Zephyr ESP32 driver rejects
 * esp_wifi_start() when WiFi is already running via the HAL.
 *
 * Safe to call even if WPS was never started (no-op in that case).
 */
void esp32_wps_stop(void);

#endif /* DRIVERS_ESP32_WPS_H_ */
