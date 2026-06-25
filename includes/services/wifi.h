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

#ifndef SERVICES_WIFI_H
#define SERVICES_WIFI_H

#include <zephyr/zbus/zbus.h>

/**
 * @brief Enumeration of the Wi-Fi commands that can be sent to the Wi-Fi service.
 *
 */
enum wifi_cmd_type {
	WIFI_CMD_CONNECT,     /**< Connect to a Wi-Fi network */
	WIFI_CMD_CONNECT_WPS, /**< Connect to a Wi-Fi network using WPS */
	WIFI_CMD_DISCONNECT,  /**< Disconnect from the current Wi-Fi network */
};

/**
 * @brief Structure of the credentials required to connect to a Wi-Fi network.
 *
 */
struct wifi_credentials {
	char ssid[CONFIG_WIFI_SSID_MAX_LEN +
		  1]; /**< SSID of the Wi-Fi network to connect (for WIFI_CMD_CONNECT) */
	char psk[CONFIG_WIFI_PSK_MAX_LEN +
		 1]; /**< PSK (password) of the Wi-Fi network to connect (for WIFI_CMD_CONNECT) */
};

/**
 * @brief Structure of the message sent to the Wi-Fi service to request a command execution.
 *
 */
struct wifi_cmd_msg {
	enum wifi_cmd_type type; /**< Type of the Wi-Fi command */
	struct wifi_credentials credentials;
	bool persist; /**< Whether to persist the connection settings */
};

/**
 * @brief Enumeration of the possible states of the Wi-Fi service for Baiatool.
 *
 */
enum wifi_baiatool_state {
	WIFI_BAIATOOL_STATE_DISCONNECTED = 0, /**< Wi-Fi is disconnected */
	WIFI_BAIATOOL_STATE_CONNECTING,       /**< Wi-Fi is connecting */
	WIFI_BAIATOOL_STATE_CONNECTED,        /**< Wi-Fi is connected */
	WIFI_BAIATOOL_STATE_FAILED,           /**< Wi-Fi connection failed */
};

/**
 * @brief Structure of the message published by the Wi-Fi service to report its current state.
 *
 */
struct wifi_state_msg {
	enum wifi_baiatool_state state; /**< Current state of the Wi-Fi service */
};

ZBUS_CHAN_DECLARE(wifi_cmd_chan);
ZBUS_CHAN_DECLARE(wifi_state_chan);

#endif /* SERVICES_WIFI_H */
