/*
 * Unit tests for the SNTP service.
 * Platform: native_sim (sntp_simple is wrapped — no real network requests).
 *
 * The service thread starts automatically via K_THREAD_DEFINE in sntp.c.
 * Tests publish to wifi_state_chan and observe sntp_time_chan.
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "services/wifi.h"
#include "services/sntp.h"

/* Declared in stubs.c */
void stub_sntp_configure(int result, uint64_t seconds);
void stub_sntp_reset(void);
int stub_sntp_get_call_count(void);

/* Time to wait after publishing to wifi_state_chan for the service thread to
 * process the event and publish to sntp_time_chan. The stub is instant, so
 * 300 ms is a large safety margin. */
#define SYNC_WAIT_MS 300

static void publish_wifi_state(enum wifi_baiatool_state state)
{
	struct wifi_state_msg msg = {.state = state};
	int ret;

	ret = zbus_chan_pub(&wifi_state_chan, &msg, K_MSEC(100));
	zassert_ok(ret, "wifi_state_chan pub failed: %d", ret);
}

static void reset_sntp_time_chan(void)
{
	struct sntp_service_time_msg reset = {.unix_time_s = 0U, .valid = false};
	int ret;

	ret = zbus_chan_pub(&sntp_time_chan, &reset, K_MSEC(100));
	zassert_ok(ret, "sntp_time_chan reset failed: %d", ret);
}

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Publishing DISCONNECTED breaks any ongoing retry loop: the retry loop
	 * reads wifi_state_chan on each iteration and exits when not CONNECTED. */
	publish_wifi_state(WIFI_BAIATOOL_STATE_DISCONNECTED);

	/* Wait long enough for the retry loop (100 ms interval) to notice the
	 * DISCONNECTED state and for the service thread to return to sub_wait. */
	k_msleep(250);

	/* Reset after the wait so any stub calls during cleanup are not counted. */
	stub_sntp_reset();
	reset_sntp_time_chan();
}

ZTEST_SUITE(sntp_service, NULL, NULL, before_each, NULL, NULL);

ZTEST(sntp_service, test_connected_publishes_valid_time)
{
	struct sntp_service_time_msg msg;

	stub_sntp_configure(0, 1750000000ULL);
	publish_wifi_state(WIFI_BAIATOOL_STATE_CONNECTED);
	k_msleep(SYNC_WAIT_MS);

	zassert_ok(zbus_chan_read(&sntp_time_chan, &msg, K_NO_WAIT),
		   "sntp_time_chan read failed");
	zassert_true(msg.valid, "expected valid=true after successful SNTP sync");
	zassert_equal(msg.unix_time_s, 1750000000ULL,
		      "unix_time_s mismatch: got %llu", (unsigned long long)msg.unix_time_s);
}

ZTEST(sntp_service, test_connected_calls_sntp_simple_once)
{
	stub_sntp_configure(0, 1750000000ULL);
	publish_wifi_state(WIFI_BAIATOOL_STATE_CONNECTED);
	k_msleep(SYNC_WAIT_MS);

	zassert_equal(stub_sntp_get_call_count(), 1,
		      "expected 1 sntp_simple call, got %d", stub_sntp_get_call_count());
}

ZTEST(sntp_service, test_disconnected_does_not_trigger_sync)
{
	struct sntp_service_time_msg msg;

	publish_wifi_state(WIFI_BAIATOOL_STATE_DISCONNECTED);
	k_msleep(SYNC_WAIT_MS);

	zassert_equal(stub_sntp_get_call_count(), 0,
		      "expected 0 sntp_simple calls on disconnect, got %d",
		      stub_sntp_get_call_count());

	zassert_ok(zbus_chan_read(&sntp_time_chan, &msg, K_NO_WAIT),
		   "sntp_time_chan read failed");
	zassert_false(msg.valid, "expected valid=false: no sync should occur on disconnect");
}

ZTEST(sntp_service, test_sntp_failure_publishes_invalid_time)
{
	struct sntp_service_time_msg msg;

	stub_sntp_configure(-EIO, 0ULL);
	publish_wifi_state(WIFI_BAIATOOL_STATE_CONNECTED);
	k_msleep(SYNC_WAIT_MS);

	zassert_ok(zbus_chan_read(&sntp_time_chan, &msg, K_NO_WAIT),
		   "sntp_time_chan read failed");
	zassert_false(msg.valid, "expected valid=false after SNTP request failure");
}

ZTEST(sntp_service, test_reconnect_triggers_second_sync)
{
	stub_sntp_configure(0, 1750000000ULL);

	publish_wifi_state(WIFI_BAIATOOL_STATE_CONNECTED);
	k_msleep(SYNC_WAIT_MS);

	publish_wifi_state(WIFI_BAIATOOL_STATE_DISCONNECTED);
	k_msleep(50);

	publish_wifi_state(WIFI_BAIATOOL_STATE_CONNECTED);
	k_msleep(SYNC_WAIT_MS);

	zassert_equal(stub_sntp_get_call_count(), 2,
		      "expected 2 sntp_simple calls across two connections, got %d",
		      stub_sntp_get_call_count());
}
