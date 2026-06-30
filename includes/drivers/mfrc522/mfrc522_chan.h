/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MFRC522_CHAN_H_
#define MFRC522_CHAN_H_

#include <zephyr/drivers/sensor.h>

#define MFRC522_UID_MAX_LEN 10U

/**
 * @brief Sensor channel definitions for MFRC522 RFID reader.
 * 
 */
enum sensor_channel_mfrc522 {
	SENSOR_CHAN_MFRC522_UID = SENSOR_CHAN_PRIV_START,
	SENSOR_CHAN_MFRC522_SAK,
	SENSOR_CHAN_MFRC522_PRESENT,
};

#endif /* MFRC522_CHAN_H_ */
