/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file wifi_shell.c
 *
 * @brief Implements shell commands to test wifi functionality.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 22/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL_WIFI

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/zbus/zbus.h>

#include "services/wifi.h"

static const char *state_to_str(enum wifi_baiatool_state state)
{
	switch (state) {
	case WIFI_BAIATOOL_STATE_DISCONNECTED:
		return "disconnected";
	case WIFI_BAIATOOL_STATE_CONNECTING:
		return "connecting";
	case WIFI_BAIATOOL_STATE_CONNECTED:
		return "connected";
	case WIFI_BAIATOOL_STATE_FAILED:
		return "failed";
	default:
		return "unknown";
	}
}

static int cmd_wifi_connect(const struct shell *sh, size_t argc, char **argv)
{
	struct wifi_cmd_msg cmd = {.type = WIFI_CMD_CONNECT, .persist = true};

	strncpy(cmd.credentials.ssid, argv[1], CONFIG_WIFI_SSID_MAX_LEN);
	cmd.credentials.ssid[CONFIG_WIFI_SSID_MAX_LEN] = '\0';

	if (argc == 3) {
		strncpy(cmd.credentials.psk, argv[2], CONFIG_WIFI_PSK_MAX_LEN);
		cmd.credentials.psk[CONFIG_WIFI_PSK_MAX_LEN] = '\0';
	}

	shell_print(sh, "Connecting to '%s'...", cmd.credentials.ssid);
	return zbus_chan_pub(&wifi_cmd_chan, &cmd, K_MSEC(500));
}

static int cmd_wifi_wps(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct wifi_cmd_msg cmd = {.type = WIFI_CMD_CONNECT_WPS};

	shell_print(sh, "Starting WPS PBC...");
	return zbus_chan_pub(&wifi_cmd_chan, &cmd, K_MSEC(500));
}

static int cmd_wifi_disconnect(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct wifi_cmd_msg cmd = {.type = WIFI_CMD_DISCONNECT};

	return zbus_chan_pub(&wifi_cmd_chan, &cmd, K_MSEC(500));
}

static int cmd_wifi_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct wifi_state_msg state;

	if (zbus_chan_read(&wifi_state_chan, &state, K_MSEC(100))) {
		shell_print(sh, "WiFi: unavailable");
		return -EIO;
	}

	shell_print(sh, "WiFi: %s", state_to_str(state.state));
	return 0;
}

static int cmd_wifi_monitor(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct wifi_state_msg state;
	enum wifi_baiatool_state last = (enum wifi_baiatool_state) - 1;

	shell_print(sh, "Monitoring WiFi state (60 s)...");

	for (int i = 0; i < CONFIG_WIFI_SHELL_MONITOR_TIMEOUT; i++) {
		if (zbus_chan_read(&wifi_state_chan, &state, K_MSEC(100))) {
			k_sleep(K_MSEC(CONFIG_WIFI_SHELL_MONITOR_POLL_MS));
			continue;
		}

		if (state.state != last) {
			last = state.state;
			shell_print(sh, "WiFi: %s", state_to_str(state.state));

			if (state.state == WIFI_BAIATOOL_STATE_CONNECTED ||
			    state.state == WIFI_BAIATOOL_STATE_FAILED) {
				break;
			}
		}

		k_sleep(K_MSEC(CONFIG_WIFI_SHELL_MONITOR_POLL_MS));
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	wifi_cmds, SHELL_CMD_ARG(connect, NULL, "<ssid> [password]", cmd_wifi_connect, 2, 1),
	SHELL_CMD_ARG(wps, NULL, "connect via WPS PBC", cmd_wifi_wps, 1, 0),
	SHELL_CMD_ARG(disconnect, NULL, "disconnect from AP", cmd_wifi_disconnect, 1, 0),
	SHELL_CMD_ARG(status, NULL, "show connection state", cmd_wifi_status, 1, 0),
	SHELL_CMD_ARG(monitor, NULL, "watch state changes (60 s)", cmd_wifi_monitor, 1, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(wifi, &wifi_cmds, "WiFi management", NULL);

#endif /* CONFIG_SHELL_WIFI */
