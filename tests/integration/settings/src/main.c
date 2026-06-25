#include <string.h>
#include <zephyr/ztest.h>

#include "components/settings.h"
#include "components/storage.h"
#include "services/wifi.h"

#define TEST_USER_ID "test_user"
#define TEST_SSID    "TestNetwork"
#define TEST_PSK     "s3cr3t_pass"

static void store_schedule(void)
{
	struct baiatool_schedule_state s = {
		.last_cmd = SCHEDULE_CMD_FIRST_USE,
		.start_time = 1000,
		.end_time = 7200,
	};

	memcpy(s.user_id, TEST_USER_ID, strlen(TEST_USER_ID) + 1U);
	baiatool_storage_create(BAIATOOL_SCHEDULE_NVS_ID, &s, sizeof(s));
}

static void store_wifi(void)
{
	struct wifi_credentials w;

	strncpy(w.ssid, TEST_SSID, sizeof(w.ssid) - 1U);
	w.ssid[sizeof(w.ssid) - 1U] = '\0';
	strncpy(w.psk, TEST_PSK, sizeof(w.psk) - 1U);
	w.psk[sizeof(w.psk) - 1U] = '\0';
	baiatool_storage_create(BAIATOOL_NET_SETTINGS_NVS_ID, &w, sizeof(w));
}

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	baiatool_storage_delete(BAIATOOL_SCHEDULE_NVS_ID);
	baiatool_storage_delete(BAIATOOL_NET_SETTINGS_NVS_ID);
}

ZTEST_SUITE(settings, NULL, NULL, before_each, NULL, NULL);

ZTEST(settings, test_get_null_returns_einval)
{
	int ret = baiatool_settings_get(NULL);

	zassert_equal(ret, -EINVAL, "expected -EINVAL for NULL, got %d", ret);
}

ZTEST(settings, test_get_empty_flash_returns_error)
{
	struct baiatool_settings s;

	int ret = baiatool_settings_get(&s);

	zassert_true(ret < 0, "expected error on empty flash, got %d", ret);
}

ZTEST(settings, test_get_fails_when_wifi_missing)
{
	struct baiatool_settings s;

	store_schedule();
	int ret = baiatool_settings_get(&s);

	zassert_true(ret < 0, "expected error when wifi not stored, got %d", ret);
}

ZTEST(settings, test_get_schedule_data_matches)
{
	struct baiatool_settings s;

	store_schedule();
	store_wifi();
	int ret = baiatool_settings_get(&s);

	zassert_equal(ret, 0, "settings_get failed: %d", ret);
	zassert_equal(s.schedule_state.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "last_cmd mismatch: got %d", s.schedule_state.last_cmd);
	zassert_mem_equal(s.schedule_state.user_id, TEST_USER_ID, strlen(TEST_USER_ID),
			  "user_id mismatch");
	zassert_equal(s.schedule_state.start_time, 1000, "start_time mismatch: got %lld",
		      (long long)s.schedule_state.start_time);
	zassert_equal(s.schedule_state.end_time, 7200, "end_time mismatch: got %lld",
		      (long long)s.schedule_state.end_time);
}

ZTEST(settings, test_get_wifi_data_matches)
{
	struct baiatool_settings s;

	store_schedule();
	store_wifi();
	int ret = baiatool_settings_get(&s);

	zassert_equal(ret, 0, "settings_get failed: %d", ret);
	zassert_str_equal(s.wifi_credentials.ssid, TEST_SSID, "ssid mismatch");
	zassert_str_equal(s.wifi_credentials.psk, TEST_PSK, "psk mismatch");
}

ZTEST(settings, test_get_updated_schedule_reflects_change)
{
	struct baiatool_settings s;

	store_schedule();
	store_wifi();

	struct baiatool_schedule_state updated = {
		.last_cmd = SCHEDULE_CMD_END_USE,
		.start_time = 0,
		.end_time = 0,
	};

	baiatool_storage_update(BAIATOOL_SCHEDULE_NVS_ID, &updated, sizeof(updated));

	int ret = baiatool_settings_get(&s);

	zassert_equal(ret, 0, "settings_get failed after update: %d", ret);
	zassert_equal(s.schedule_state.last_cmd, SCHEDULE_CMD_END_USE,
		      "last_cmd should reflect update, got %d", s.schedule_state.last_cmd);
}
