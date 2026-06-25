/*
 * Unit tests for the baiatool schedule service state machine.
 * Platform: mps2/an385 (simulated, FLASH_SIMULATOR).
 *
 * storage_init() runs via SYS_INIT before tests execute.
 * before_each() resets schedule_chan_state to idle so each test starts clean.
 * schedule_storage_update fires as a side effect of state changes; NVS errors
 * on the first write of a test are benign — baiatool_storage_delete returns 0
 * for non-existent keys, so the subsequent create always succeeds.
 */

#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "services/schedule.h"

#define USER_A "ana_001"
#define USER_B "bob_002"

static const struct baiatool_schedule_state k_idle = {
	.user_id = {0},
	.last_cmd = SCHEDULE_CMD_END_USE,
	.start_time = 0,
	.end_time = 0,
};

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	zbus_chan_pub(&schedule_chan_state, &k_idle, K_MSEC(100));
}

ZTEST_SUITE(schedule, NULL, NULL, before_each, NULL, NULL);

static void pub_cmd(enum schedule_cmd_id id, const char *user_id)
{
	struct baiatool_schedule_cmd cmd;
	size_t id_len;

	cmd.cmd_id = id;
	memset(cmd.user_id, 0, sizeof(cmd.user_id));

	if (user_id != NULL) {
		id_len = strlen(user_id);
		memcpy(cmd.user_id, user_id, MIN(id_len, sizeof(cmd.user_id) - 1U));
	}

	zbus_chan_pub(&schedule_chan_cmd, &cmd, K_MSEC(100));
}

static void get_state(struct baiatool_schedule_state *out)
{
	zbus_chan_read(&schedule_chan_state, out, K_MSEC(100));
}

/* ── FIRST_USE ───────────────────────────────────────────────────── */

ZTEST(schedule, test_first_use_sets_state)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE, "expected FIRST_USE, got %d", s.last_cmd);
	zassert_mem_equal(s.user_id, USER_A, strlen(USER_A), "user_id mismatch after FIRST_USE");
	zassert_equal(s.end_time, CONFIG_SCHEDULE_FIRST_USE_TIMEOUT_MINUTES,
		      "end_time should equal FIRST_USE timeout, got %d", (int)s.end_time);
}

ZTEST(schedule, test_first_use_rejects_empty_user_id)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, NULL);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_END_USE, "state should remain idle, got last_cmd=%d",
		      s.last_cmd);
}

ZTEST(schedule, test_first_use_rejects_when_session_active)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_B);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "second FIRST_USE should be rejected, got last_cmd=%d", s.last_cmd);
	zassert_mem_equal(s.user_id, USER_A, strlen(USER_A),
			  "user_id should still belong to USER_A after rejected FIRST_USE");
}

/* ── EXTEND_TIME ─────────────────────────────────────────────────── */

ZTEST(schedule, test_extend_time_increases_end_time)
{
	struct baiatool_schedule_state s;
	time_t end_after_first;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	get_state(&s);
	end_after_first = s.end_time;

	pub_cmd(SCHEDULE_CMD_EXTEND_TIME, USER_A);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_EXTEND_TIME, "expected EXTEND_TIME, got %d",
		      s.last_cmd);
	zassert_equal(s.end_time, end_after_first + CONFIG_SCHEDULE_EXTEND_TIMEOUT_MINUTES,
		      "end_time not extended correctly: got %d, want %d", (int)s.end_time,
		      (int)(end_after_first + CONFIG_SCHEDULE_EXTEND_TIMEOUT_MINUTES));
}

ZTEST(schedule, test_extend_time_rejects_wrong_user)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	pub_cmd(SCHEDULE_CMD_EXTEND_TIME, USER_B);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "wrong-user extend should be rejected, got last_cmd=%d", s.last_cmd);
	zassert_mem_equal(s.user_id, USER_A, strlen(USER_A),
			  "user_id should be unchanged after rejected extend");
}

ZTEST(schedule, test_extend_time_rejects_without_session)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_EXTEND_TIME, USER_A);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_END_USE,
		      "extend without session should be rejected, got last_cmd=%d", s.last_cmd);
}

/* ── END_USE ─────────────────────────────────────────────────────── */

ZTEST(schedule, test_end_use_clears_state)
{
	struct baiatool_schedule_state s;
	uint8_t empty_id[CONFIG_MAX_ID_LENGTH] = {0};

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	pub_cmd(SCHEDULE_CMD_END_USE, USER_A);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_END_USE,
		      "expected END_USE after ending session, got %d", s.last_cmd);
	zassert_mem_equal(s.user_id, empty_id, CONFIG_MAX_ID_LENGTH,
			  "user_id should be zeroed after END_USE");
	zassert_equal(s.start_time, 0, "start_time should be 0 after END_USE");
	zassert_equal(s.end_time, 0, "end_time should be 0 after END_USE");
}

ZTEST(schedule, test_end_use_rejects_empty_user_id)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	pub_cmd(SCHEDULE_CMD_END_USE, NULL);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "session should remain active after END_USE with empty id, got %d",
		      s.last_cmd);
}

ZTEST(schedule, test_end_use_rejects_wrong_user)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	pub_cmd(SCHEDULE_CMD_END_USE, USER_B);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "wrong-user END_USE should be rejected, got last_cmd=%d", s.last_cmd);
	zassert_mem_equal(s.user_id, USER_A, strlen(USER_A),
			  "user_id should be unchanged after rejected END_USE");
}

ZTEST(schedule, test_end_use_rejects_when_idle)
{
	struct baiatool_schedule_state s;

	pub_cmd(SCHEDULE_CMD_END_USE, USER_A);
	get_state(&s);

	zassert_equal(s.last_cmd, SCHEDULE_CMD_END_USE,
		      "END_USE on idle state should be rejected, got last_cmd=%d", s.last_cmd);
}

/* ── Full lifecycle ──────────────────────────────────────────────── */

ZTEST(schedule, test_full_lifecycle)
{
	struct baiatool_schedule_state s;
	uint8_t empty_id[CONFIG_MAX_ID_LENGTH] = {0};

	pub_cmd(SCHEDULE_CMD_FIRST_USE, USER_A);
	get_state(&s);
	zassert_equal(s.last_cmd, SCHEDULE_CMD_FIRST_USE,
		      "lifecycle step 1 (FIRST_USE) failed, got %d", s.last_cmd);

	pub_cmd(SCHEDULE_CMD_EXTEND_TIME, USER_A);
	get_state(&s);
	zassert_equal(s.last_cmd, SCHEDULE_CMD_EXTEND_TIME,
		      "lifecycle step 2 (EXTEND_TIME) failed, got %d", s.last_cmd);

	pub_cmd(SCHEDULE_CMD_END_USE, USER_A);
	get_state(&s);
	zassert_equal(s.last_cmd, SCHEDULE_CMD_END_USE, "lifecycle step 3 (END_USE) failed, got %d",
		      s.last_cmd);
	zassert_mem_equal(s.user_id, empty_id, CONFIG_MAX_ID_LENGTH,
			  "user_id not cleared at end of full lifecycle");
}
