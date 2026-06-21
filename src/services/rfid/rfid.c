/* SPDX-License-Identifier: Apache-2.0 */
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "drivers/mfrc522/mfrc522_chan.h"
#include "services/rfid/rfid.h"

LOG_MODULE_REGISTER(rfid_svc, CONFIG_LOG_DEFAULT_LEVEL);

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

			sensor_channel_get(s_rfid,
					   (enum sensor_channel)SENSOR_CHAN_MFRC522_PRESENT,
					   &present);

			if (present.val1) {
				struct sensor_value uid_val;

				sensor_channel_get(s_rfid,
						   (enum sensor_channel)SENSOR_CHAN_MFRC522_UID,
						   &uid_val);

				struct rfid_event ev = {
					.uid_len = (uint8_t)uid_val.val1,
				};

				memcpy(ev.uid_bytes, &uid_val.val2,
				       MIN((uint8_t)uid_val.val1, 4U));

				zbus_chan_pub(&rfid_tag_chan, &ev, K_MSEC(10));
				LOG_INF("RFID tag: len=%u", ev.uid_len);
			}
		}

		k_msleep(200);
	}
}

K_THREAD_DEFINE(rfid_thread, 1024, rfid_thread_fn, NULL, NULL, NULL, 5, 0, 0);
