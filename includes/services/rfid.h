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

/**
 * @brief Enumeration of RFID tap edge types.
 *
 */
enum rfid_tap_event {
	RFID_EVT_PRESENT = 0, /**< Tag detected (rising edge). */
	RFID_EVT_REMOVED,     /**< Tag removed (falling edge). */
};

/**
 * @brief RFID event message structure.
 *
 */
struct rfid_event {
	uint8_t uid_bytes[CONFIG_MFRC522_UID_MAX_LEN]; /**< UID bytes */
	uint8_t uid_len;                               /**< Length of the UID in bytes */
	enum rfid_tap_event type;                      /**< Edge type for this event */
	uint32_t duration_ms; /**< Time the tag stayed present; valid only on RFID_EVT_REMOVED */
};

/**
 * @brief Declares the RFID channel.
 *
 */
ZBUS_CHAN_DECLARE(rfid_tag_chan);

#endif /* SERVICE_RFID_H_ */
