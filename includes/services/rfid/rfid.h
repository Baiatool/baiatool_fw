/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RFID_H_
#define RFID_H_

#include <stdint.h>
#include <zephyr/zbus/zbus.h>
#include "drivers/mfrc522/mfrc522_chan.h"

struct rfid_event {
	uint8_t uid_bytes[MFRC522_UID_MAX_LEN];
	uint8_t uid_len;
};

ZBUS_CHAN_DECLARE(rfid_tag_chan);

#endif /* RFID_H_ */
