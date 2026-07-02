/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file display.h
 *
 * @brief Defines the TFT display management service interface.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef SERVICES_DISPLAY_H
#define SERVICES_DISPLAY_H

#include <zephyr/zbus/zbus.h>

/**
 * @brief Display screen identifiers.
 */
enum display_screen_id {
	DISPLAY_SCREEN_LOGO = 0, /**< Logo splash screen */
	DISPLAY_SCREEN_WARNING,  /**< Warning / error screen */
	DISPLAY_SCREEN_SCHEDULE, /**< Schedule status screen */
	DISPLAY_SCREEN_AMOUNT,
};

/**
 * @brief Command message for the display service.
 *
 * Send on display_cmd_chan to switch screens or update warning text.
 * For DISPLAY_SCREEN_SCHEDULE, the service reads schedule_state_chan
 * directly — warning_msg is ignored.
 */
struct display_cmd_msg {
	enum display_screen_id screen;
	char warning_msg[CONFIG_DISPLAY_SERVICE_WARNING_MSG_MAX_LEN];
};

/**
 * @brief Declares the command channel for the display service.
 */
ZBUS_CHAN_DECLARE(display_cmd_chan);

#endif /* SERVICES_DISPLAY_H */
