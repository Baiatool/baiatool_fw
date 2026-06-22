/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file storage.h
 *
 * @brief Definition of storage component, responsible for store data in non-volatile memory.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 03/12/2025
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#ifndef COMPONENTS_STORAGE_H_
#define COMPONENTS_STORAGE_H_

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

/**
 * @brief Enumeration of the IDs available in the NVS.
 *
 */
enum baiatool_storage_id {
    BAIATOOL_NULL_NVS_ID = 0,       /**< ID invalid for NVS */
    BAIATOOL_AUTHENTICATION_NVS_ID, /**< ID of the NVS for authentication data */
    BAIATOOL_SCHEDULE_NVS_ID,       /**< ID of the NVS for the current schedule data */
    BAIATOOL_PROVISIONING_NVS_ID,   /**< ID of the NVS for the provisioning data */
    BAIATOOL_NET_SETTINGS_NVS_ID,   /**< ID of the NVS for the network settings */
    BAIATOOL_NVS_ID_AMOUNT          /**< Amount of NVS IDs */
};

/**
 * @brief Function to add a new object to the flash memory.
 *
 * @param nvs_id ID in the NVS of the object to be added.
 * @param[in] data Data to be stored in flash memory.
 * @param[in] data_len Size of the data to be stored.
 * @retval Number of bytes stored.
 * @retval -ERROR in case of error.
 */
ssize_t baiatool_storage_create(enum baiatool_storage_id nvs_id, void *data, size_t data_len);

/**
 * @brief Function to read an object from flash memory.
 *
 * @param nvs_id ID in the NVS of the object to be read.
 * @param[out] data Buffer to store the data read from flash memory.
 * @retval Number of bytes read.
 * @retval -ERROR in case of error.
 */
ssize_t baiatool_storage_read(enum baiatool_storage_id nvs_id, void *data, size_t data_len);

/**
 * @brief Function to update a flash object.
 *
 * @param nvs_id ID in the NVS of the object to be updated.
 * @param[in] data Data to be updated in the flash.
 * @retval Returns 0 on success.
 * @retval -ERROR on error.
 */
ssize_t baiatool_storage_update(enum baiatool_storage_id nvs_id, void *data, size_t data_len);

/**
 * @brief Function to delete an object from flash memory.
 *
 * @param nvs_id ID in the NVS of the object to be deleted from flash memory.
 * @retval Returns 0 on success.
 * @retval -ERROR on error.
 */
int baiatool_storage_delete(enum baiatool_storage_id nvs_id);

#endif /* COMPONENTS_STORAGE_H_ */