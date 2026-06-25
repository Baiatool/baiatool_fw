/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file settings.h
 *
 * @brief Implementation of settings component, responsible for managing device settings.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef COMPONENTS_SETTINGS_H
#define COMPONENTS_SETTINGS_H

#include "services/schedule.h"

/**
 * @brief Helper structure to store wifi settings
 *
 */
struct wifi_settings {
	char wifi_ssid[CONFIG_WIFI_SSID_MAX_LEN + 1]; /**< WiFi SSID currently on flash */
	char wifi_psk[CONFIG_WIFI_PSK_MAX_LEN + 1];   /**< WiFi PSK currently on flash */
};

/**
 * @brief Structure to store baiatool settings
 *
 */
struct baiatool_settings {
	struct baiatool_schedule_state schedule_state; /**< Current schedule state */
	struct wifi_settings wifi_settings;            /**< WiFi settings */

	/* TODO @Jota: Add provisioning settings */
};

/**
 * @brief Gets the baiatool settings from storage
 *
 * @param[out] settings Pointer to the baiatool settings structure
 * @return int 0 if Success, -ERRNO otherwise
 */
int baiatool_settings_get(struct baiatool_settings *settings);

#endif /* COMPONENTS_SETTINGS_H */