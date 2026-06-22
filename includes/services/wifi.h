/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file wifi.h
 *
 * @brief Defines the Wi-Fi management service interface.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 22/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef WIFI_H
#define WIFI_H

#include <zephyr/zbus/zbus.h>

enum wifi_cmd_type {
	WIFI_CMD_CONNECT,
	WIFI_CMD_CONNECT_WPS,
	WIFI_CMD_DISCONNECT,
};

struct wifi_cmd_msg {
	enum wifi_cmd_type type;
	char ssid[CONFIG_WIFI_SSID_MAX_LEN + 1];
	char psk[CONFIG_WIFI_PSK_MAX_LEN + 1];
};

enum wifi_baiatool_state {
	WIFI_BAIATOOL_STATE_DISCONNECTED = 0,
	WIFI_BAIATOOL_STATE_CONNECTING,
	WIFI_BAIATOOL_STATE_CONNECTED,
	WIFI_BAIATOOL_STATE_FAILED,
};

struct wifi_state_msg {
	enum wifi_baiatool_state state;
};

ZBUS_CHAN_DECLARE(wifi_cmd_chan);
ZBUS_CHAN_DECLARE(wifi_state_chan);

#endif /* WIFI_H */
