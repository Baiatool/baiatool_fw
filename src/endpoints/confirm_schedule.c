/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file confirm_schedule.c
 *
 * @brief HTTP endpoint — notifies the server that the user confirmed
 *        the pending schedule by tapping their RFID/NFC tag.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "endpoints/confirm_schedule.h"
#include "endpoints/http_request.h"
#include "services/schedule.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(confirm_schedule_endpoint, CONFIG_CONFIRM_SCHEDULE_ENDPOINT_LOG_LEVEL);

static struct {
	uint8_t recv_buf[CONFIG_HTTP_RECV_BUF_SIZE];
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

int confirm_schedule_endpoint_post(int sock, const char *host, const char *port)
{
	int ret;
	struct baiatool_schedule_state state;
	char body[CONFIG_MAX_ID_LENGTH + 14]; /* {"user_id":"..."} */

	ret = zbus_chan_read(&schedule_state_chan, &state, K_MSEC(300));
	if (ret != 0) {
		LOG_ERR("failed to read schedule state: %d", ret);
		return ret;
	}

	ret = snprintf(body, sizeof(body), "{\"user_id\":\"%s\"}", state.user_id);
	if (ret < 0 || ret >= (int)sizeof(body)) {
		LOG_ERR("body buffer too small");
		return -ENOMEM;
	}

	data_response.s_http_status = 0;
	memset(data_response.recv_buf, 0, sizeof(data_response.recv_buf));

	ret = HTTP_REQ(HTTP_POST, sock, host, port, "/api/schedule/confirm",
		       "application/json", body, strlen(body),
		       response_cb, data_response.recv_buf, sizeof(data_response.recv_buf), 5000,
		       NULL);
	if (ret < 0) {
		LOG_ERR("POST /api/schedule/confirm failed: %d", ret);
		return ret;
	}

	if (data_response.s_http_status < 200 || data_response.s_http_status >= 300) {
		LOG_ERR("unexpected HTTP status: %d", data_response.s_http_status);
		return -EIO;
	}

	LOG_INF("schedule confirmed for user %s (HTTP %d)", state.user_id,
		data_response.s_http_status);
	return 0;
}
