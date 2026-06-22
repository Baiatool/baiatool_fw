/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file storage_shell.c
 *
 * @brief Shell commands for the storage component (NVS read/write/delete).
 * @author Joao Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 22/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL

#include "components/storage.h"

#include <stdlib.h>
#include <string.h>
#include <zephyr/shell/shell.h>

#define STORAGE_SHELL_READ_BUF_SIZE 256

static int parse_nvs_id(const struct shell *sh, const char *str,
			 enum baiatool_storage_id *out_id)
{
	long val = strtol(str, NULL, 10);

	if (val <= BAIATOOL_NULL_NVS_ID || val >= BAIATOOL_NVS_ID_AMOUNT) {
		shell_error(sh, "Invalid id %ld. Valid range: 1..%d", val,
			    BAIATOOL_NVS_ID_AMOUNT - 1);
		return -EINVAL;
	}

	*out_id = (enum baiatool_storage_id)val;
	return 0;
}

static int cmd_storage_write(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_storage_id nvs_id;

	if (parse_nvs_id(sh, argv[1], &nvs_id) != 0) {
		return -EINVAL;
	}

	char *data = argv[2];
	size_t data_len = strlen(data) + 1;

	ssize_t ret = baiatool_storage_create(nvs_id, data, data_len);

	if (ret > 0) {
		shell_print(sh, "Wrote %zd bytes to id %d.", ret, nvs_id);
	} else if (ret == -ENOTEMPTY) {
		shell_error(sh, "Entry id %d already exists. Use 'storage update' to overwrite.",
			    nvs_id);
	} else {
		shell_error(sh, "Write failed (id %d): %zd", nvs_id, ret);
	}

	return ret > 0 ? 0 : ret;
}

static int cmd_storage_read(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_storage_id nvs_id;

	if (parse_nvs_id(sh, argv[1], &nvs_id) != 0) {
		return -EINVAL;
	}

	static uint8_t buf[STORAGE_SHELL_READ_BUF_SIZE];

	memset(buf, 0, sizeof(buf));

	ssize_t ret = baiatool_storage_read(nvs_id, buf, sizeof(buf) - 1);

	if (ret > 0) {
		shell_print(sh, "id %d (%zd bytes): %s", nvs_id, ret, (char *)buf);
	} else {
		shell_error(sh, "Read failed (id %d): %zd", nvs_id, ret);
	}

	return ret > 0 ? 0 : ret;
}

static int cmd_storage_update(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_storage_id nvs_id;

	if (parse_nvs_id(sh, argv[1], &nvs_id) != 0) {
		return -EINVAL;
	}

	char *data = argv[2];
	size_t data_len = strlen(data) + 1;

	ssize_t ret = baiatool_storage_update(nvs_id, data, data_len);

	if (ret == 0) {
		shell_print(sh, "Updated id %d.", nvs_id);
	} else {
		shell_error(sh, "Update failed (id %d): %zd", nvs_id, ret);
	}

	return (int)ret;
}

static int cmd_storage_delete(const struct shell *sh, size_t argc, char **argv)
{
	enum baiatool_storage_id nvs_id;

	if (parse_nvs_id(sh, argv[1], &nvs_id) != 0) {
		return -EINVAL;
	}

	int ret = baiatool_storage_delete(nvs_id);

	if (ret == 0) {
		shell_print(sh, "Deleted id %d.", nvs_id);
	} else {
		shell_error(sh, "Delete failed (id %d): %d", nvs_id, ret);
	}

	return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(storage_cmds,
	SHELL_CMD_ARG(write, NULL, "<id> <data>", cmd_storage_write, 3, 0),
	SHELL_CMD_ARG(read, NULL, "<id>", cmd_storage_read, 2, 0),
	SHELL_CMD_ARG(update, NULL, "<id> <data>", cmd_storage_update, 3, 0),
	SHELL_CMD_ARG(delete, NULL, "<id>", cmd_storage_delete, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(storage, &storage_cmds, "NVS storage commands.", NULL);

#endif /* CONFIG_SHELL */