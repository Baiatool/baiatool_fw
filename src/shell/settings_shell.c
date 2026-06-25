/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file settings_shell.c
 *
 * @brief Shell commands for the settings component.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL

#include "components/settings.h"

#include <zephyr/shell/shell.h>

static const char *schedule_cmd_to_str(enum schedule_cmd_id cmd)
{
	switch (cmd) {
	case SCHEDULE_CMD_FIRST_USE:
		return "first_use";
	case SCHEDULE_CMD_EXTEND_TIME:
		return "extend_time";
	case SCHEDULE_CMD_END_USE:
		return "end_use";
	default:
		return "unknown";
	}
}

static int cmd_settings_get(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct baiatool_settings settings;
	int ret = baiatool_settings_get(&settings);

	if (ret < 0) {
		shell_error(sh, "Failed to get settings: %d", ret);
		return ret;
	}

	shell_print(sh, "Schedule:");
	shell_print(sh, "  user_id : %s", settings.schedule_state.user_id);
	shell_print(sh, "  last_cmd: %s", schedule_cmd_to_str(settings.schedule_state.last_cmd));
	shell_print(sh, "  start   : %lld", (long long)settings.schedule_state.start_time);
	shell_print(sh, "  end     : %lld", (long long)settings.schedule_state.end_time);
	shell_print(sh, "WiFi:");
	shell_print(sh, "  ssid    : %s", settings.wifi_credentials.ssid);
	shell_print(sh, "  psk     : %s",
		    settings.wifi_credentials.psk[0] != '\0' ? "****" : "(not set)");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(settings_cmds,
			       SHELL_CMD_ARG(get, NULL, "print all settings from flash",
					     cmd_settings_get, 1, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(settings, &settings_cmds, "Settings commands.", NULL);

#endif /* CONFIG_SHELL */
