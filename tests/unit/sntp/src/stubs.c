/*
 * Test stubs for the SNTP service unit test.
 *
 * Provides:
 *   - wifi_state_chan / wifi_cmd_chan definitions (replaces wifi.c)
 *   - __wrap_sntp_simple (via -Wl,--wrap=sntp_simple) for controlled responses
 */

#include <zephyr/kernel.h>
#include <zephyr/net/sntp.h>
#include <zephyr/zbus/zbus.h>

#include "services/wifi.h"
#include "services/sntp.h"

/* sntp_wifi_state_sub is defined in sntp.c; forward-declare so wifi_state_chan
 * can list it as an observer without depending on wifi.c. */
ZBUS_OBS_DECLARE(sntp_wifi_state_sub);

ZBUS_CHAN_DEFINE(wifi_state_chan, struct wifi_state_msg, NULL, NULL,
		 ZBUS_OBSERVERS(sntp_wifi_state_sub),
		 ZBUS_MSG_INIT(.state = WIFI_BAIATOOL_STATE_DISCONNECTED));

ZBUS_CHAN_DEFINE(wifi_cmd_chan, struct wifi_cmd_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.type = WIFI_CMD_DISCONNECT));

/* ---- sntp_simple stub -------------------------------------------------- */

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

/* CONFIG_SNTP=n disables Zephyr's sntp_simple.c — this is the only definition. */
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
