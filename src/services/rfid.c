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

static void rfid_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!device_is_ready(s_rfid)) {
		LOG_ERR("mfrc522 not ready");
		return;
	}

	while (true) {
		int ret = sensor_sample_fetch(s_rfid);

		if (ret == 0) {
			struct sensor_value present;

			ret = sensor_channel_get(s_rfid,
						 (enum sensor_channel)SENSOR_CHAN_MFRC522_PRESENT,
						 &present);
			if (ret != 0) {
				LOG_WRN("channel_get(PRESENT) failed: %d", ret);
				k_msleep(200);
				continue;
			}

			if (present.val1) {
				struct sensor_value uid_val;

				ret = sensor_channel_get(s_rfid,
							 (enum sensor_channel)SENSOR_CHAN_MFRC522_UID,
							 &uid_val);
				if (ret != 0) {
					LOG_WRN("channel_get(UID) failed: %d", ret);
					k_msleep(200);
					continue;
				}

				uint8_t copied = MIN((uint8_t)uid_val.val1, 4U);
				struct rfid_event ev = {
					.uid_len = copied,
				};

				memcpy(ev.uid_bytes, &uid_val.val2, copied);

				ret = zbus_chan_pub(&rfid_tag_chan, &ev, K_MSEC(10));
				if (ret != 0) {
					LOG_WRN("zbus_chan_pub failed: %d", ret);
				} else {
					LOG_INF("RFID tag: len=%u uid=%02X:%02X:%02X:%02X",
						ev.uid_len,
						ev.uid_bytes[0], ev.uid_bytes[1],
						ev.uid_bytes[2], ev.uid_bytes[3]);
				}
			}
		}

		k_msleep(200);
	}
}

K_THREAD_DEFINE(rfid_thread, 2048, rfid_thread_fn, NULL, NULL, NULL, 5, 0, 0);
