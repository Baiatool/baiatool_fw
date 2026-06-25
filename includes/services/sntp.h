/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file sntp.h
 *
 * @brief Defines the SNTP time synchronization service interface.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef SERVICES_SNTP_H
#define SERVICES_SNTP_H

#include <zephyr/zbus/zbus.h>

/**
 * @brief Message published on sntp_time_chan after each synchronization attempt.
 *
 */
struct sntp_service_time_msg {
	uint64_t unix_time_s; /**< Unix timestamp in seconds (valid == true only) */
	bool valid;           /**< true when unix_time_s carries a good SNTP value */
};

/**
 * @brief Logs the given Unix timestamp in seconds as a human-readable date and time.
 * 
 * @param unix_s Unix timestamp in seconds since the epoch (January 1, 1970).
 */
void log_sntp_datetime(uint64_t unix_s);

ZBUS_CHAN_DECLARE(sntp_time_chan);

#endif /* SERVICES_SNTP_H */
