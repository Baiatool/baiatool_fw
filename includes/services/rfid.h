/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file rfid.h
 *
 * @brief Define the RFID management service for the workstation.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 24/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef SERVICE_RFID_H_
#define SERVICE_RFID_H_

#include <stdint.h>
#include <zephyr/zbus/zbus.h>
#include "drivers/mfrc522/mfrc522_chan.h"

/**
 * @brief RFID event message structure.
 * 
 */
struct rfid_event {
	uint8_t uid_bytes[MFRC522_UID_MAX_LEN]; /**< UID bytes */
	uint8_t uid_len; /**< Length of the UID in bytes */
};

/**
 * @brief Construct a new zbus chan declare object
 * 
 */
ZBUS_CHAN_DECLARE(rfid_tag_chan);

#endif /* SERVICE_RFID_H_ */
