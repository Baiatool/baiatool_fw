/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file led_shell.c
 *
 * @brief Implements shell commands to test LED service functionality.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/zbus/zbus.h>

#include "services/led.h"

static int color_from_str(const char *str, enum baiatool_led_colors *color)
{
	if (strcmp(str, "red") == 0) {
		*color = LED_COLOR_RED;
	} else if (strcmp(str, "green") == 0) {
		*color = LED_COLOR_GREEN;
	} else if (strcmp(str, "blue") == 0) {
		*color = LED_COLOR_BLUE;
	} else if (strcmp(str, "default") == 0) {
		*color = LED_COLOR_DEFAULT;
	} else {
		return -EINVAL;
	}
	return 0;
}

static int pub_led_cmd(const struct shell *sh, enum baiatool_led_colors color,
		       enum led_patterns pattern)
{
	struct led_cmd_msg msg = {.color = color, .pattern = pattern};
	int ret = zbus_chan_pub(&led_cmd_chan, &msg, K_MSEC(500));

	if (ret < 0) {
		shell_error(sh, "Failed to publish LED command: %d", ret);
	}
	return ret;
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pub_led_cmd(sh, LED_COLOR_DEFAULT, LED_CMD_BLINK_OFF);
}

static int cmd_led_solid(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_led_colors color;

	if (color_from_str(argv[1], &color) < 0) {
		shell_error(sh, "Unknown color '%s'. Use: red green blue white default", argv[1]);
		return -EINVAL;
	}

	return pub_led_cmd(sh, color, LED_CMD_BLINK_SOLID);
}

static int cmd_led_slow(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_led_colors color;

	if (color_from_str(argv[1], &color) < 0) {
		shell_error(sh, "Unknown color '%s'. Use: red green blue white default", argv[1]);
		return -EINVAL;
	}

	return pub_led_cmd(sh, color, LED_CMD_BLINK_SLOW);
}

static int cmd_led_fast(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_led_colors color;

	if (color_from_str(argv[1], &color) < 0) {
		shell_error(sh, "Unknown color '%s'. Use: red green blue white default", argv[1]);
		return -EINVAL;
	}

	return pub_led_cmd(sh, color, LED_CMD_BLINK_FAST);
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	led_cmds,
	SHELL_CMD_ARG(off, NULL, "turn off all LEDs", cmd_led_off, 1, 0),
	SHELL_CMD_ARG(solid, NULL, "<color: red|green|blue|white|default>",
		      cmd_led_solid, 2, 0),
	SHELL_CMD_ARG(slow, NULL, "<color: red|green|blue|white|default>",
		      cmd_led_slow, 2, 0),
	SHELL_CMD_ARG(fast, NULL, "<color: red|green|blue|white|default>",
		      cmd_led_fast, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(led, &led_cmds, "LED management", NULL);

#endif /* CONFIG_SHELL */