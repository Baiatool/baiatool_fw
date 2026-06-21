/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file main.c
 *
 * @brief Implements the scheduling management service for the workstation.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 22/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

int main(void)
{
/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "services/rfid/rfid.h"

LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(main_rfid_sub, 4);

int main(void)
{
	const struct zbus_channel *chan;

	zbus_chan_add_obs(&rfid_tag_chan, &main_rfid_sub, K_NO_WAIT);

	while (true) {
		if (zbus_sub_wait(&main_rfid_sub, &chan, K_FOREVER) != 0) {
			continue;
		}

		struct rfid_event ev;

		if (zbus_chan_read(chan, &ev, K_NO_WAIT) != 0) {
			continue;
		}

		LOG_INF("RFID UID (%u bytes):", ev.uid_len);
		for (uint8_t i = 0U; i < ev.uid_len; i++) {
			LOG_INF("  [%u] 0x%02X", i, ev.uid_bytes[i]);
		}
	}

	return 0;
}
