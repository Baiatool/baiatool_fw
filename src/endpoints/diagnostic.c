/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file diagnostic.c
 *
 * @brief HTTP endpoint — sends a device diagnostic snapshot (settings)
 *        to the server. The Wi-Fi PSK is never included.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 01/07/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "components/settings.h"
#include "endpoints/diagnostic.h"
#include "endpoints/http_request.h"

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>

LOG_MODULE_REGISTER(diagnostic_endpoint, CONFIG_DIAGNOSTIC_ENDPOINT_LOG_LEVEL);

struct diag_payload {
	const char *ssid;
	const char *user_id;
	int32_t last_cmd;
	int32_t start_time;
	int32_t end_time;
};

static const struct json_obj_descr diag_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct diag_payload, ssid, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct diag_payload, user_id, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct diag_payload, last_cmd, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct diag_payload, start_time, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct diag_payload, end_time, JSON_TOK_NUMBER),
};

static struct {
	uint8_t recv_buf[CONFIG_HTTP_RECV_BUF_SIZE];
	char body_buf[CONFIG_HTTP_BODY_BUF_SIZE];
	uint16_t s_http_status;
} data_response;

static int response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	ARG_UNUSED(user_data);

	if (final_data == HTTP_DATA_FINAL) {
		data_response.s_http_status = rsp->http_status_code;
	}

	return 0;
}

int diagnostic_endpoint_post(int sock, const char *host, const char *port)
{
	int ret;
	struct baiatool_settings settings;
	struct diag_payload payload;

	ret = baiatool_settings_get(&settings);
	if (ret < 0) {
		LOG_ERR("failed to read settings: %d", ret);
		return ret;
	}

	payload.ssid = settings.wifi_credentials.ssid;
	payload.user_id = (const char *)settings.schedule_state.user_id;
	payload.last_cmd = (int32_t)settings.schedule_state.last_cmd;
	payload.start_time = (int32_t)settings.schedule_state.start_time;
	payload.end_time = (int32_t)settings.schedule_state.end_time;

	memset(data_response.body_buf, 0, sizeof(data_response.body_buf));

	ret = json_obj_encode_buf(diag_descr, ARRAY_SIZE(diag_descr), &payload,
				  data_response.body_buf, sizeof(data_response.body_buf));
	if (ret < 0) {
		LOG_ERR("JSON encode failed: %d", ret);
		return ret;
	}

	data_response.s_http_status = 0;
	memset(data_response.recv_buf, 0, sizeof(data_response.recv_buf));

	ret = HTTP_REQ(HTTP_POST, sock, host, port, "/api/diagnostic", "application/json",
		       data_response.body_buf, strlen(data_response.body_buf), response_cb,
		       data_response.recv_buf, sizeof(data_response.recv_buf), 5000, NULL);
	if (ret < 0) {
		LOG_ERR("POST /api/diagnostic failed: %d", ret);
		return ret;
	}

	if (data_response.s_http_status < 200 || data_response.s_http_status >= 300) {
		LOG_ERR("unexpected HTTP status: %d", data_response.s_http_status);
		return -EIO;
	}

	LOG_INF("diagnostic sent (HTTP %d)", data_response.s_http_status);
	return 0;
}
