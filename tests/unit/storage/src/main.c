/*
 * Unit tests for baiatool storage component.
 * Platform: mps2/an385 (simulated, FLASH_SIMULATOR).
 *
 * storage_init() runs via SYS_INIT before tests execute.
 * before_each() wipes all NVS IDs so each test starts clean.
 */

#include "components/storage.h"
#include <zephyr/ztest.h>

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	baiatool_storage_delete(BAIATOOL_SCHEDULE_NVS_ID);
	baiatool_storage_delete(BAIATOOL_PROVISIONING_NVS_ID);
	baiatool_storage_delete(BAIATOOL_NET_SETTINGS_NVS_ID);
}

ZTEST_SUITE(storage, NULL, NULL, before_each, NULL, NULL);

ZTEST(storage, test_create_duplicate_returns_enotempty)
{
	uint32_t data = 0x12345678U;

	baiatool_storage_create(BAIATOOL_SCHEDULE_NVS_ID, &data, sizeof(data));
	ssize_t ret = baiatool_storage_create(BAIATOOL_SCHEDULE_NVS_ID, &data, sizeof(data));

	zassert_equal(ret, -ENOTEMPTY, "expected -ENOTEMPTY on duplicate, got %d", ret);
}

ZTEST(storage, test_delete_existing_returns_zero)
{
	uint32_t data = 0xABCDABCDU;

	baiatool_storage_create(BAIATOOL_PROVISIONING_NVS_ID, &data, sizeof(data));
	int ret = baiatool_storage_delete(BAIATOOL_PROVISIONING_NVS_ID);

	zassert_equal(ret, 0, "delete returned %d, expected 0", ret);
}

ZTEST(storage, test_delete_then_read_returns_error)
{
	uint32_t data = 0x11223344U;
	uint32_t buf = 0U;

	baiatool_storage_create(BAIATOOL_PROVISIONING_NVS_ID, &data, sizeof(data));
	baiatool_storage_delete(BAIATOOL_PROVISIONING_NVS_ID);
	ssize_t ret = baiatool_storage_read(BAIATOOL_PROVISIONING_NVS_ID, &buf, sizeof(buf));

	zassert_true(ret < 0, "expected error reading deleted ID, got %d", ret);
}

ZTEST(storage, test_delete_nonexistent_returns_zero)
{
	int ret = baiatool_storage_delete(BAIATOOL_NET_SETTINGS_NVS_ID);

	zassert_equal(ret, 0, "expected 0 for nonexistent delete, got %d", ret);
}

ZTEST(storage, test_update_changes_stored_data)
{
	uint32_t original = 0xAAAAAAAAU;
	uint32_t updated = 0xBBBBBBBBU;
	uint32_t read_back = 0U;

	baiatool_storage_create(BAIATOOL_SCHEDULE_NVS_ID, &original, sizeof(original));
	ssize_t ret = baiatool_storage_update(BAIATOOL_SCHEDULE_NVS_ID, &updated, sizeof(updated));

	zassert_equal(ret, 0, "update returned %d, expected 0", ret);

	ret = baiatool_storage_read(BAIATOOL_SCHEDULE_NVS_ID, &read_back, sizeof(read_back));
	zassert_true(ret > 0, "read after update failed: %d", ret);
	zassert_equal(read_back, updated, "data mismatch after update: got 0x%08X, want 0x%08X",
		      read_back, updated);
}
