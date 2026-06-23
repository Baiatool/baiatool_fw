/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file led.h
 *
 * @brief Defines the LED management service interface.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef SERVICES_LED_H
#define SERVICES_LED_H

#include <zephyr/zbus/zbus.h>

/**
 * @brief Device tree alias for the red LED.
 *
 */
#define BAIATOOL_RED_LED_NODE DT_ALIAS(red_led)

/**
 * @brief Device tree alias for the green LED.
 *
 */
#define BAIATOOL_GREEN_LED_NODE DT_ALIAS(green_led)

/**
 * @brief Device tree alias for the blue LED.
 *
 */
#define BAIATOOL_BLUE_LED_NODE DT_ALIAS(blue_led)

/**
 * @brief All colors that led can be set to.
 *
 */
enum baiatool_led_colors {
	LED_COLOR_DEFAULT = 0, /**< Default LED color (no color) */
	LED_COLOR_RED,         /**< Red LED */
	LED_COLOR_GREEN,       /**< Green LED */
	LED_COLOR_BLUE,        /**< Blue LED */
	LED_COLOR_WHITE,       /**< White LED (Red + Green + Blue) */
	LED_COLOR_AMOUNT       /**< Amount of LED colors */
};

/**
 * @brief Command patterns for LED control.
 *
 */
enum led_patterns {
	LED_CMD_BLINK_OFF = 0, /**< No blinking */
	LED_CMD_BLINK_SOLID,   /**< Solid on */
	LED_CMD_BLINK_SLOW,    /**< Slow blinking */
	LED_CMD_BLINK_FAST,    /**< Fast blinking */
	LED_CMD_BLINK_AMOUNT,  /**< Amount of blinking patterns */
};

struct led_cmd_msg {
	enum baiatool_led_colors color; /**< Color to set the LED */
	enum led_patterns pattern;      /**< Blinking pattern for the LED */
};

/**
 * @brief Structure representing a command to control the LED.
 *
 * @return int Return 0 if the command was executed with success, or retVal if there was an error.
 *
 * @retval -EIO I/O error when accessing an external GPIO chip.
 * @retval -EWOULDBLOCK if operation would block.
 * @retval -EFAULT Invalid command provided.
 */
typedef struct {
	void (*execute)(enum baiatool_led_colors); /** < Function pointer to execute led' command */
} led_cmd;

ZBUS_CHAN_DECLARE(led_cmd_chan);

#endif /* SERVICES_LED_H */