/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file display_shell.c
 *
 * @brief Shell commands to switch display screens manually.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL_DISPLAY

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/zbus/zbus.h>

#include "services/display.h"

static int pub_display_cmd(const struct shell *sh, const struct display_cmd_msg *msg)
{
	int ret = zbus_chan_pub(&display_cmd_chan, msg, K_MSEC(500));

	if (ret < 0) {
		shell_error(sh, "Failed to publish display command: %d", ret);
	}
	return ret;
}

static int cmd_display_logo(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct display_cmd_msg msg = {.screen = DISPLAY_SCREEN_LOGO};
	return pub_display_cmd(sh, &msg);
}

static int cmd_display_warn(const struct shell *sh, size_t argc, char **argv)
{
	struct display_cmd_msg msg = {.screen = DISPLAY_SCREEN_WARNING};

	if (argc > 1) {
		strncpy(msg.warning_msg, argv[1], sizeof(msg.warning_msg) - 1);
		msg.warning_msg[sizeof(msg.warning_msg) - 1] = '\0';
	}

	return pub_display_cmd(sh, &msg);
}

static int cmd_display_schedule(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct display_cmd_msg msg = {.screen = DISPLAY_SCREEN_SCHEDULE};
	return pub_display_cmd(sh, &msg);
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	display_cmds,
	SHELL_CMD_ARG(logo, NULL, "show logo screen", cmd_display_logo, 1, 0),
	SHELL_CMD_ARG(warn, NULL, "[message] show warning screen", cmd_display_warn, 1, 1),
	SHELL_CMD_ARG(schedule, NULL, "show schedule screen", cmd_display_schedule, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(display, &display_cmds, "Display screen control", NULL);

#endif /* CONFIG_SHELL_DISPLAY */
