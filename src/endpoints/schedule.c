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
 * @brief HTTP endpoint — fetches the pending schedule from the server
 *        and loads it into the schedule service.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 26/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "endpoints/http_request.h"
#include "endpoints/schedule.h"
#include "services/schedule.h"

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(schedule_endpoint, CONFIG_SCHEDULE_ENDPOINT_LOG_LEVEL);

#define RECV_BUF_SIZE 512
#define BODY_BUF_SIZE 256

struct schedule_server_resp {
	const char *user_id;
	const char *last_cmd;
	int32_t start_time;
	int32_t end_time;
};

static const struct json_obj_descr resp_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct schedule_server_resp, user_id, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct schedule_server_resp, last_cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct schedule_server_resp, start_time, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct schedule_server_resp, end_time, JSON_TOK_NUMBER),
};

static struct {
	uint8_t recv_buf[RECV_BUF_SIZE];
	char body_buf[BODY_BUF_SIZE];
	size_t body_len;
	uint16_t s_http_status;
} data_response;

static int response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	ARG_UNUSED(user_data);

	if (rsp->body_frag_len > 0) {
		size_t space = sizeof(data_response.body_buf) - data_response.body_len - 1;
		size_t copy_len = MIN(rsp->body_frag_len, space);

		memcpy(&data_response.body_buf[data_response.body_len], rsp->body_frag_start,
		       copy_len);
		data_response.body_len += copy_len;
	}

	if (final_data == HTTP_DATA_FINAL) {
		data_response.s_http_status = rsp->http_status_code;
	}

	return 0;
}

int schedule_endpoint_fetch(int sock, const char *host, const char *port)
{
	int ret;
	int parsed;
	struct schedule_server_resp resp = {NULL, NULL, 0, 0};
	struct baiatool_schedule_cmd cmd;

	data_response.body_len = 0;
	data_response.s_http_status = 0;
	memset(data_response.recv_buf, 0, sizeof(data_response.recv_buf));
	memset(data_response.body_buf, 0, sizeof(data_response.body_buf));

	ret = HTTP_REQ(HTTP_GET, sock, host, port, "/api/schedule", NULL, NULL, 0, response_cb,
		       data_response.recv_buf, sizeof(data_response.recv_buf), 5000, NULL);
	if (ret < 0) {
		LOG_ERR("GET /api/schedule failed: %d", ret);
		return ret;
	}

	if (data_response.s_http_status != 200) {
		LOG_INF("no schedule available (HTTP %u)", data_response.s_http_status);
		return 0;
	}

	if (data_response.body_len == 0) {
		LOG_WRN("200 response with empty body");
		return -ENODATA;
	}

	parsed = json_obj_parse(data_response.body_buf, data_response.body_len, resp_descr,
				ARRAY_SIZE(resp_descr), &resp);

	if (parsed < 0) {
		LOG_ERR("JSON parse error: %d", parsed);
		return -EINVAL;
	}

	/* Bitmask: bits 0-3 map to the four descriptor fields */
	if ((parsed & 0xF) != 0xF) {
		LOG_ERR("JSON missing required fields, mask: 0x%x", parsed);
		return -EINVAL;
	}

	if (resp.user_id == NULL || resp.user_id[0] == '\0') {
		LOG_ERR("empty user_id in response");
		return -EINVAL;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = SCHEDULE_CMD_LOAD;
	cmd.start_time = (time_t)resp.start_time;
	cmd.end_time = (time_t)resp.end_time;
	memcpy(cmd.user_id, resp.user_id, CONFIG_MAX_ID_LENGTH - 1);

	ret = zbus_chan_pub(&schedule_cmd_chan, &cmd, K_MSEC(300));
	if (ret != 0) {
		LOG_ERR("failed to publish LOAD command: %d", ret);
		return ret;
	}

	LOG_INF("schedule LOAD published for user %s", cmd.user_id);
	return 0;
}
