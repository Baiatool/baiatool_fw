/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MFRC522_CHAN_H_
#define MFRC522_CHAN_H_

#include <zephyr/drivers/sensor.h>

#define MFRC522_UID_MAX_LEN 10U

enum sensor_channel_mfrc522 {
	SENSOR_CHAN_MFRC522_UID = SENSOR_CHAN_PRIV_START,
	SENSOR_CHAN_MFRC522_SAK,
	SENSOR_CHAN_MFRC522_PRESENT,
};

/*
 * Layout of struct sensor_value when chan == SENSOR_CHAN_MFRC522_UID:
 *   val1 = uid length (bytes)
 *   val2 = first 4 uid bytes packed as uint32_t (little-endian)
 */

#endif /* MFRC522_CHAN_H_ */
