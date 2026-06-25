/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file settings.c
 *
 * @brief Implementation of settings component, responsible for managing device settings.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "components/settings.h"
#include "components/storage.h"

#include <zephyr/logging/log.h>
#include <sys/errno.h>

LOG_MODULE_REGISTER(settings_component);

int baiatool_settings_get(struct baiatool_settings *settings)
{
	if (settings == NULL) {
		return -EINVAL;
	}

	ssize_t ret;

	ret = baiatool_storage_read(BAIATOOL_SCHEDULE_NVS_ID, &settings->schedule_state,
				    sizeof(settings->schedule_state));
	if (ret < 0) {
		LOG_ERR("Failed to read schedule state: %d", (int)ret);
		return (int)ret;
	}

	ret = baiatool_storage_read(BAIATOOL_NET_SETTINGS_NVS_ID, &settings->wifi_settings,
				    sizeof(settings->wifi_settings));
	if (ret < 0) {
		LOG_ERR("Failed to read wifi settings: %d", (int)ret);
		return (int)ret;
	}

	return 0;
}