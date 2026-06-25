/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file schedule.h
 *
 * @brief Define the scheduling management service for the workstation.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 28/11/2025
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#ifndef SERVICE_SCHEDULE_H
#define SERVICE_SCHEDULE_H

#include <time.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

/**
 * @brief Declares the channel for the scheduling service status.
 *
 */
ZBUS_CHAN_DECLARE(schedule_chan_state);

/**
 * @brief Declares the channel for the scheduling service commands.
 *
 */
ZBUS_CHAN_DECLARE(schedule_chan_cmd);

/**
 * @brief Enumeration of command identifiers for the scheduling service.
 *
 */
enum schedule_cmd_id {
	SCHEDULE_CMD_FIRST_USE = 0, /**< Command to start using the workstation. */
	SCHEDULE_CMD_EXTEND_TIME,   /**< Command to extend workstation usage time. */
	SCHEDULE_CMD_END_USE,       /**< Command to end workstation use. */
	SCHEDULE_CMD_AMOUNT         /**< Amount of commands available. */
};

/**
 * @brief Structure representing the current state of the workstation.
 *
 */
struct baiatool_schedule_state {
	uint8_t user_id[CONFIG_MAX_ID_LENGTH]; /**< User ID of the user currently using the workstation. */
	enum schedule_cmd_id last_cmd;         /**< Last command sent by the user. */
	time_t start_time;                     /**< Start time for using the workstation. */
	time_t end_time;                       /**< Time when the workstation can no longer be used. */
};

/**
 * @brief Structure that represents a command for the scheduling service.
 *
 */
struct baiatool_schedule_cmd {
	enum schedule_cmd_id cmd_id;           /**< ID of the command */
	uint8_t user_id[CONFIG_MAX_ID_LENGTH]; /**< ID from the user who sent the command */
};

#endif /* SERVICE_SCHEDULE_H */