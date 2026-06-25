/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file sntp.c
 *
 * @brief Implements the SNTP time synchronization service.
 *
 * Subscribes to wifi_state_chan. On each CONNECTED event, queries
 * CONFIG_SNTP_SERVICE_SERVER and publishes the result on sntp_time_chan.
 *
 * Note: the ESP32 DevKit C has no battery-backed external RTC, so time is
 * lost on every power cycle and SNTP must run on every boot after WiFi connects.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

 #include <zephyr/kernel.h>
 #include <zephyr/logging/log.h>
 #include <zephyr/net/sntp.h>
 #include <time.h>

#if defined(CONFIG_POSIX_TIMERS)
#include <zephyr/posix/time.h>
#endif

#include "services/sntp.h"
#include "services/wifi.h"

LOG_MODULE_REGISTER(sntp_service, CONFIG_SNTP_SERVICE_LOG_LEVEL);

static void on_sntp_time_update(const struct zbus_channel *chan);

ZBUS_SUBSCRIBER_DEFINE(sntp_wifi_state_sub, CONFIG_SNTP_SERVICE_WIFI_SUB_QUEUE_SIZE);

ZBUS_CHAN_DEFINE(sntp_time_chan, struct sntp_service_time_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.unix_time_s = 0, .valid = false));

ZBUS_LISTENER_DEFINE(main_sntp_listener, on_sntp_time_update);

ZBUS_CHAN_ADD_OBS(sntp_time_chan, main_sntp_listener, 0);

static void publish_time(uint64_t unix_time_s, bool valid)
{
	struct sntp_service_time_msg msg = {
		.unix_time_s = unix_time_s,
		.valid = valid,
	};
	int ret;

	ret = zbus_chan_pub(&sntp_time_chan, &msg, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish time: %d", ret);
	}
}

static int do_sntp_sync(void)
{
	struct sntp_time ts;
	int ret;

	LOG_INF("Syncing with %s", CONFIG_SNTP_SERVICE_SERVER);

	ret = sntp_simple(CONFIG_SNTP_SERVICE_SERVER, CONFIG_SNTP_SERVICE_TIMEOUT_MS, &ts);
	if (ret < 0) {
		LOG_ERR("SNTP request failed: %d", ret);
		publish_time(0, false);
		return ret;
	}

#if defined(CONFIG_POSIX_TIMERS)
	struct timespec tspec = {
		.tv_sec = (time_t)ts.seconds,
		.tv_nsec = 0,
	};

	if (clock_settime(CLOCK_REALTIME, &tspec) < 0) {
		LOG_WRN("clock_settime failed");
	}
#endif

	struct tm t;
	time_t epoch = (time_t)ts.seconds + CONFIG_SNTP_SERVICE_UTC_OFFSET;

	gmtime_r(&epoch, &t);
	LOG_INF("Time synced: %04d-%02d-%02d %02d:%02d:%02d BRT", t.tm_year + 1900, t.tm_mon + 1,
		t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	publish_time(ts.seconds, true);

	return 0;
}

static void sntp_service_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&sntp_wifi_state_sub, &chan, K_FOREVER)) {
		struct wifi_state_msg state;

		if (zbus_chan_read(chan, &state, K_MSEC(100)) < 0) {
			continue;
		}

		if (state.state != WIFI_BAIATOOL_STATE_CONNECTED) {
			continue;
		}

		/* Retry while connected until sync succeeds */
		do {
			if (do_sntp_sync() == 0) {
				break;
			}

			k_sleep(K_MSEC(CONFIG_SNTP_SERVICE_RETRY_INTERVAL_MS));

			if (zbus_chan_read(&wifi_state_chan, &state, K_MSEC(100)) < 0) {
				break;
			}
		} while (state.state == WIFI_BAIATOOL_STATE_CONNECTED);
	}
}

void log_sntp_datetime(uint64_t unix_s)
{
	struct tm t;
	time_t ts = (time_t)unix_s + CONFIG_SNTP_SERVICE_UTC_OFFSET;

	gmtime_r(&ts, &t);

	LOG_INF("SNTP updated: %04d-%02d-%02d %02d:%02d:%02d BRT", t.tm_year + 1900, t.tm_mon + 1,
		t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

static void on_sntp_time_update(const struct zbus_channel *chan)
{
	struct sntp_service_time_msg msg;

	if (zbus_chan_read(chan, &msg, K_NO_WAIT) < 0 || !msg.valid) {
		return;
	}

	log_sntp_datetime(msg.unix_time_s);
}

K_THREAD_DEFINE(sntp_service_tid, CONFIG_SNTP_SERVICE_THREAD_STACK_SIZE, sntp_service_thread, NULL,
		NULL, NULL, CONFIG_SNTP_SERVICE_THREAD_PRIORITY, 0, 0);
