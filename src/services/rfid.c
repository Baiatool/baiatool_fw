#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "drivers/mfrc522.h"
#include "services/rfid.h"

LOG_MODULE_REGISTER(rfid, CONFIG_RFID_SERVICE_LOG_LEVEL);

static const struct device *s_rfid = DEVICE_DT_GET_ONE(nxp_mfrc522);

ZBUS_CHAN_DEFINE(rfid_tag_chan, struct rfid_event, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

static void rfid_publish(enum rfid_tap_event type, const uint8_t *uid_bytes, uint8_t uid_len,
			 uint32_t duration_ms)
{
	int ret;
	struct rfid_event ev = {
		.type = type,
		.uid_len = uid_len,
		.duration_ms = duration_ms,
	};

	memcpy(ev.uid_bytes, uid_bytes, uid_len);

	ret = zbus_chan_pub(&rfid_tag_chan, &ev, K_MSEC(10));
	if (ret != 0) {
		LOG_WRN("zbus_chan_pub failed: %d", ret);
		return;
	}

	LOG_INF("RFID %s: len=%u uid=%02X:%02X:%02X:%02X duration_ms=%u",
		type == RFID_EVT_PRESENT ? "present" : "removed", ev.uid_len, ev.uid_bytes[0],
		ev.uid_bytes[1], ev.uid_bytes[2], ev.uid_bytes[3], duration_ms);
}

static void rfid_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	bool was_present = false;
	int64_t detected_at = 0;
	uint8_t last_uid[CONFIG_MFRC522_UID_MAX_LEN] = {0};
	uint8_t last_uid_len = 0;

	if (!device_is_ready(s_rfid)) {
		LOG_ERR("mfrc522 not ready");
		return;
	}

	while (true) {
		int ret = sensor_sample_fetch(s_rfid);
		bool now_present = false;

		if (ret == 0) {
			struct sensor_value present;

			ret = sensor_channel_get(
				s_rfid, (enum sensor_channel)SENSOR_CHAN_MFRC522_PRESENT, &present);
			if (ret != 0) {
				LOG_WRN("channel_get(PRESENT) failed: %d", ret);
				k_msleep(CONFIG_RFID_POLL_INTERVAL_MS);
				continue;
			}

			now_present = present.val1 != 0;
		}

		if (now_present && !was_present) {
			struct sensor_value uid_val;

			ret = sensor_channel_get(
				s_rfid, (enum sensor_channel)SENSOR_CHAN_MFRC522_UID, &uid_val);
			if (ret != 0) {
				LOG_WRN("channel_get(UID) failed: %d", ret);
				k_msleep(CONFIG_RFID_POLL_INTERVAL_MS);
				continue;
			}

			last_uid_len = MIN((uint8_t)uid_val.val1, 4U);
			memcpy(last_uid, &uid_val.val2, last_uid_len);

			detected_at = k_uptime_get();
			rfid_publish(RFID_EVT_PRESENT, last_uid, last_uid_len, 0);
		} else if (!now_present && was_present) {
			uint32_t duration_ms = (uint32_t)(k_uptime_get() - detected_at);

			rfid_publish(RFID_EVT_REMOVED, last_uid, last_uid_len, duration_ms);
		}

		was_present = now_present;
		k_msleep(CONFIG_RFID_POLL_INTERVAL_MS);
	}
}

K_THREAD_DEFINE(rfid_thread, 2048, rfid_thread_fn, NULL, NULL, NULL, 5, 0, 0);
