/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file button.c
 *
 * @brief Implements the WPS button action.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 25/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "services/wifi.h"

LOG_MODULE_REGISTER(button, CONFIG_BUTTON_LOG_LEVEL);

#define BTN_WPS_NODE DT_ALIAS(btn_wps)

static const struct gpio_dt_spec btn_wps = GPIO_DT_SPEC_GET(BTN_WPS_NODE, gpios);
static struct gpio_callback btn_cb;
static struct k_work wps_work;

static void wps_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	struct wifi_cmd_msg cmd = {.type = WIFI_CMD_CONNECT_WPS};

	int ret = zbus_chan_pub(&wifi_cmd_chan, &cmd, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish WPS cmd: %d", ret);
	}
}

static void btn_wps_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

    LOG_DBG("WPS button pressed, starting WPS process");

	k_work_submit(&wps_work);
}

static int button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&btn_wps)) {
		LOG_ERR("WPS button GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&btn_wps, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure WPS button: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&btn_wps, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure WPS button interrupt: %d", ret);
		return ret;
	}

	(void)gpio_init_callback(&btn_cb, btn_wps_pressed, BIT(btn_wps.pin));
	(void)gpio_add_callback(btn_wps.port, &btn_cb);

	k_work_init(&wps_work, wps_work_handler);

	LOG_INF("WPS button ready");
	return 0;
}

SYS_INIT(button_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
