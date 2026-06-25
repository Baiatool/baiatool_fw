#include <zephyr/kernel.h>
#include <zephyr/net/sntp.h>
#include <zephyr/zbus/zbus.h>

#include "services/wifi.h"
#include "services/sntp.h"

ZBUS_OBS_DECLARE(sntp_wifi_state_sub);

ZBUS_CHAN_DEFINE(wifi_state_chan, struct wifi_state_msg, NULL, NULL,
		 ZBUS_OBSERVERS(sntp_wifi_state_sub),
		 ZBUS_MSG_INIT(.state = WIFI_BAIATOOL_STATE_DISCONNECTED));

ZBUS_CHAN_DEFINE(wifi_cmd_chan, struct wifi_cmd_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.type = WIFI_CMD_DISCONNECT));


static int stub_result;
static uint64_t stub_seconds;
static int stub_call_count;

void stub_sntp_configure(int result, uint64_t seconds)
{
	stub_result = result;
	stub_seconds = seconds;
}

void stub_sntp_reset(void)
{
	stub_result = 0;
	stub_seconds = 1750000000ULL;
	stub_call_count = 0;
}

int stub_sntp_get_call_count(void)
{
	return stub_call_count;
}

int sntp_simple(const char *server, uint32_t timeout, struct sntp_time *ts)
{
	ARG_UNUSED(server);
	ARG_UNUSED(timeout);

	stub_call_count++;

	if (stub_result < 0) {
		return stub_result;
	}

	ts->seconds = stub_seconds;
	ts->fraction = 0U;
	ts->rsp_delay_us = 0U;

	return 0;
}
