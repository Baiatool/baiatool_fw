/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file led.c
 *
 * @brief Implements the LED management service.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "services/led.h"

#define VERIFY_FUNC_AND_RETURN(func)                                                               \
	do {                                                                                       \
		int ret = func;                                                                    \
		if (ret < 0) {                                                                     \
			LOG_ERR("Error %d: %s failed", ret, #func);                                \
			return;                                                                    \
		}                                                                                  \
	} while (0)

LOG_MODULE_REGISTER(led, CONFIG_LED_LOG_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(led_cmd_sub, CONFIG_LED_CMD_SUB_QUEUE_SIZE);

ZBUS_CHAN_DEFINE(led_cmd_chan, struct led_cmd_msg, NULL, NULL, ZBUS_OBSERVERS(led_cmd_sub),
		 ZBUS_MSG_INIT(.color = LED_COLOR_DEFAULT, .pattern = LED_CMD_BLINK_OFF));

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(BAIATOOL_RED_LED_NODE, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(BAIATOOL_GREEN_LED_NODE, gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BAIATOOL_BLUE_LED_NODE, gpios);

static struct led_cmd_msg current_led_state = {.color = LED_COLOR_DEFAULT,
					       .pattern = LED_CMD_BLINK_OFF};

static struct k_timer blink_timer;
static struct k_work blink_work;
static bool blink_state;

static inline void led_off(void)
{
	VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&red_led, 0));
	VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&green_led, 0));
	VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&blue_led, 0));
}

static void baiatool_led_set_basic(bool state)
{
    led_off();
    
	switch (current_led_state.color) {
	case LED_COLOR_RED:
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&red_led, state));
		break;
	case LED_COLOR_GREEN:
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&green_led, state));
		break;
	case LED_COLOR_BLUE:
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&blue_led, state));
		break;
	case LED_COLOR_WHITE:
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&red_led, state));
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&green_led, state));
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&blue_led, state));
		break;
	case LED_COLOR_DEFAULT:
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&red_led, false));
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&green_led, false));
		VERIFY_FUNC_AND_RETURN(gpio_pin_set_dt(&blue_led, false));
		break;
	default:
		LOG_ERR("Invalid LED color");
		return;
	}
}

static void blink_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	blink_state = !blink_state;
	baiatool_led_set_basic(blink_state);
}

static void blink_timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	k_work_submit(&blink_work);
}

static void baiatool_led_set_off(enum baiatool_led_colors color)
{
	ARG_UNUSED(color);
	k_timer_stop(&blink_timer);
	baiatool_led_set_basic(false);
}

static void baiatool_led_set_solid(enum baiatool_led_colors color)
{
	ARG_UNUSED(color);
	k_timer_stop(&blink_timer);
	baiatool_led_set_basic(true);
}

static void baiatool_led_set_slow(enum baiatool_led_colors color)
{
	ARG_UNUSED(color);
	blink_state = false;
	k_timer_start(&blink_timer, K_MSEC(CONFIG_LED_BLINK_TIME_SLOW_MS),
		      K_MSEC(CONFIG_LED_BLINK_TIME_SLOW_MS));
}

static void baiatool_led_set_fast(enum baiatool_led_colors color)
{
	ARG_UNUSED(color);
	blink_state = false;
	k_timer_start(&blink_timer, K_MSEC(CONFIG_LED_BLINK_TIME_FAST_MS),
		      K_MSEC(CONFIG_LED_BLINK_TIME_FAST_MS));
}

static led_cmd led_commands[LED_CMD_BLINK_AMOUNT] = {
	[LED_CMD_BLINK_OFF] = {.execute = baiatool_led_set_off},
	[LED_CMD_BLINK_SOLID] = {.execute = baiatool_led_set_solid},
	[LED_CMD_BLINK_SLOW] = {.execute = baiatool_led_set_slow},
	[LED_CMD_BLINK_FAST] = {.execute = baiatool_led_set_fast},
};

static void led_execute_command(void)
{
	led_commands[current_led_state.pattern].execute(current_led_state.color);
}

int led_init(void)
{
	int ret = 0;

	if (!device_is_ready(red_led.port)) {
		printk("Red LED device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Error %d: failed to configure Red LED pin", ret);
		return 0;
	}

	if (!device_is_ready(green_led.port)) {
		LOG_ERR("Green LED device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to configure Green LED pin", ret);
		return 0;
	}

	if (!device_is_ready(blue_led.port)) {
		LOG_ERR("Blue LED device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to configure Blue LED pin", ret);
		return 0;
	}

	k_timer_init(&blink_timer, blink_timer_expiry, NULL);
	k_work_init(&blink_work, blink_work_handler);

	return 0;
}

static void led_service_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&led_cmd_sub, &chan, K_FOREVER)) {
		if (zbus_chan_read(chan, &current_led_state, K_MSEC(100))) {
			continue;
		}

		LOG_DBG("Received LED command: color=%d, pattern=%d", current_led_state.color,
			current_led_state.pattern);

		led_execute_command();
	}
}

K_THREAD_DEFINE(led_service_thread_id, CONFIG_LED_THREAD_STACK_SIZE, led_service_thread, NULL, NULL,
		NULL, CONFIG_LED_THREAD_PRIORITY, 0, 0);

SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);