#ifndef DRIVER_MFRC522_H_
#define DRIVER_MFRC522_H_

#include <zephyr/drivers/sensor.h>

/**
 * @brief Sensor channel definitions for MFRC522 RFID reader.
 *
 */
enum sensor_channel_mfrc522 {
	SENSOR_CHAN_MFRC522_UID = SENSOR_CHAN_PRIV_START, /**< Request the UID channel */
	SENSOR_CHAN_MFRC522_SAK,                          /**< Request the SAK channel */
	SENSOR_CHAN_MFRC522_PRESENT,                      /**< Request the present channel */
};

#endif /* DRIVER_MFRC522_H_ */
