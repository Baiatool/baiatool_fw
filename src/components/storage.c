/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file storage.c
 *
 * @brief Implementation of storage component, responsible for store data in non-volatile memory.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @author Joao Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 21/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "components/storage.h"

#include <stdint.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <sys/errno.h>

#define NVS_PARTITION        storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

LOG_MODULE_REGISTER(storage_component, CONFIG_STORAGE_LOG_LEVEL);

static struct nvs_fs storage_fs;

ssize_t baiatool_storage_read(enum baiatool_storage_id nvs_id, void *data, size_t data_len)
{
	__ASSERT(nvs_id != 0, "Invalid NVS_ID received");

	if (data == NULL) {
		LOG_ERR("Invalid pointer received");
		return -EINVAL;
	}

	ssize_t ret;

	ret = nvs_read(&storage_fs, nvs_id, data, data_len);
	if (ret > 0) {
		LOG_DBG("An object with ID = %u was read.", nvs_id);
		return ret;
	}

	LOG_ERR("Failed to read object with id %u: %d", nvs_id, ret);

	return -ENODATA;
}

ssize_t baiatool_storage_create(enum baiatool_storage_id nvs_id, void *data, size_t data_len)
{
	__ASSERT(nvs_id != 0, "Invalid NVS_ID received");
	__ASSERT(data != NULL, "Invalid pointer received");

	ssize_t ret;
	uint8_t *aux;

	ret = baiatool_storage_read(nvs_id, &aux, data_len);
	if (ret > 0) {
		LOG_WRN("An object with %u already exists.", nvs_id);
		return -ENOTEMPTY;
	}

	ret = nvs_write(&storage_fs, nvs_id, data, data_len);
	if (ret <= 0) {
		LOG_ERR("Failed to add a new object added to storage with id %u: %d", nvs_id, ret);
		return ret;
	}

	LOG_DBG("New object added to storage with %d bytes", ret);
	return ret;
}

int baiatool_storage_delete(enum baiatool_storage_id nvs_id)
{
	__ASSERT(nvs_id != 0, "Invalid NVS_ID received");

	int ret;
	uint8_t *aux;

	ret = baiatool_storage_read(nvs_id, &aux, sizeof(aux));
	if (ret < 0) {
		LOG_ERR("There's no object with ID = %u", nvs_id);
		return 0;
	}

	ret = nvs_delete(&storage_fs, nvs_id);
	if (ret < 0) {
		LOG_ERR("Failed to delete object with id %u: %d", nvs_id, ret);
		return ret;
	}

	LOG_DBG("Object with id: %u deleted", nvs_id);

	return 0;
}

ssize_t baiatool_storage_update(enum baiatool_storage_id nvs_id, void *data, size_t data_len)
{
	__ASSERT(nvs_id != 0, "Invalid NVS_ID received");
	__ASSERT(data != NULL, "Invalid pointer received");

	ssize_t ret;

	ret = baiatool_storage_delete(nvs_id);
	if (ret < 0) {
		LOG_ERR("Failed to delete object with id: %u", nvs_id);
		return ret;
	}

	ret = baiatool_storage_create(nvs_id, data, data_len);
	if (ret > 0) {
		LOG_DBG("Object with id %u updated", nvs_id);
		return 0;
	}

	LOG_ERR("Failed to update object with id: %u", nvs_id);
	return ret;
}

int storage_init(void)
{
	int ret;
	struct flash_pages_info page_info;

	storage_fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(storage_fs.flash_device)) {
		LOG_ERR("Flash device not ready");
		return 0;
	}

	storage_fs.offset = NVS_PARTITION_OFFSET;
	ret = flash_get_page_info_by_offs(storage_fs.flash_device, storage_fs.offset, &page_info);
	if (ret) {
		LOG_ERR("Unable to get page info: %d", ret);
		return 0;
	}

	storage_fs.sector_size = page_info.size;
	storage_fs.sector_count = 2U;

	ret = nvs_mount(&storage_fs);
	if (ret) {
		LOG_ERR("NVS mount failed: %d", ret);
		return 0;
	}

	return 0;
}

SYS_INIT(storage_init, APPLICATION, CONFIG_FLASH_INIT_PRIORITY);