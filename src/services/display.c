/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file display.c
 *
 * @brief Implements the TFT display management service.
 *
 * Three screens:
 *   LOGO     — splash shown at boot
 *   WARNING  — red screen for errors / alerts
 *   SCHEDULE — live schedule status (idle, upcoming, in-use)
 *
 * The service subscribes to both display_cmd_chan and
 * schedule_state_chan via a single ZBUS_SUBSCRIBER. All LVGL calls
 * happen inside the display thread to avoid concurrency issues.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#include "assets/logo_baiatool.h"
#include "services/display.h"
#include "services/schedule.h"

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(display_service, CONFIG_DISPLAY_SERVICE_LOG_LEVEL);

ZBUS_CHAN_DEFINE(display_cmd_chan, struct display_cmd_msg, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.screen = DISPLAY_SCREEN_LOGO, .warning_msg = {0}));

ZBUS_SUBSCRIBER_DEFINE(display_sub, CONFIG_DISPLAY_SERVICE_CMD_SUB_QUEUE_SIZE);

ZBUS_CHAN_ADD_OBS(display_cmd_chan, display_sub, 2);
ZBUS_CHAN_ADD_OBS(schedule_state_chan, display_sub, 2);

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static lv_obj_t *logo_screen;
static lv_obj_t *warning_screen;
static lv_obj_t *schedule_screen;

/* Warning screen widgets */
static lv_obj_t *warn_title_label;
static lv_obj_t *warn_msg_label;

/* Schedule screen widgets */
static lv_obj_t *sched_title_label;
static lv_obj_t *sched_user_label;
static lv_obj_t *sched_start_label;
static lv_obj_t *sched_end_label;
static lv_obj_t *sched_status_label;

static void build_logo_screen(void)
{
	logo_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(logo_screen, lv_color_hex(0x000000), LV_PART_MAIN);

	lv_obj_t *img = lv_image_create(logo_screen);
	lv_image_set_src(img, &logo_baiatool);

	/* Scale image to fit display width (240px). Logo is 301×240.
	 * scale = (240 / 301) * 256 ≈ 204 */
	lv_image_set_scale(img, 204);
	lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}

static void build_warning_screen(void)
{
	warning_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(warning_screen, lv_color_hex(0xCC0000), LV_PART_MAIN);

	warn_title_label = lv_label_create(warning_screen);
	lv_label_set_text(warn_title_label, "! AVISO");
	lv_obj_set_style_text_color(warn_title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_align(warn_title_label, LV_ALIGN_TOP_MID, 0, 20);

	warn_msg_label = lv_label_create(warning_screen);
	lv_label_set_text(warn_msg_label, "");
	lv_label_set_long_mode(warn_msg_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(warn_msg_label, 220);
	lv_obj_set_style_text_color(warn_msg_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_align(warn_msg_label, LV_ALIGN_CENTER, 0, 20);
}

static void build_schedule_screen(void)
{
	schedule_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(schedule_screen, lv_color_hex(0x0D1B2A), LV_PART_MAIN);

	sched_title_label = lv_label_create(schedule_screen);
	lv_obj_set_style_text_color(sched_title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_align(sched_title_label, LV_ALIGN_TOP_MID, 0, 16);

	sched_user_label = lv_label_create(schedule_screen);
	lv_obj_set_style_text_color(sched_user_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
	lv_label_set_long_mode(sched_user_label, LV_LABEL_LONG_DOT);
	lv_obj_set_width(sched_user_label, 220);
	lv_obj_align(sched_user_label, LV_ALIGN_TOP_MID, 0, 60);

	sched_start_label = lv_label_create(schedule_screen);
	lv_obj_set_style_text_color(sched_start_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
	lv_obj_align(sched_start_label, LV_ALIGN_TOP_MID, 0, 100);

	sched_end_label = lv_label_create(schedule_screen);
	lv_obj_set_style_text_color(sched_end_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
	lv_obj_align(sched_end_label, LV_ALIGN_TOP_MID, 0, 130);

	sched_status_label = lv_label_create(schedule_screen);
	lv_obj_set_style_text_color(sched_status_label, lv_color_hex(0x00FF88), LV_PART_MAIN);
	lv_obj_align(sched_status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void format_time(char *buf, size_t len, time_t t)
{
	if (t <= 0) {
		snprintf(buf, len, "---");
		return;
	}
	struct tm tm_info;
	gmtime_r(&t, &tm_info);
	strftime(buf, len, "%H:%M", &tm_info);
}

static void update_schedule_screen(const struct baiatool_schedule_state *state)
{
	char start_buf[16];
	char end_buf[16];
	char user_buf[CONFIG_MAX_ID_LENGTH + 8];

	format_time(start_buf, sizeof(start_buf), state->start_time);
	format_time(end_buf, sizeof(end_buf), state->end_time);
	snprintf(user_buf, sizeof(user_buf), "ID: %s", state->user_id);

	switch (state->last_cmd) {
	case SCHEDULE_CMD_END_USE:
		lv_label_set_text(sched_title_label, "LIVRE");
		lv_obj_set_style_text_color(sched_title_label, lv_color_hex(0x00CC66),
					    LV_PART_MAIN);
		lv_label_set_text(sched_user_label, "Sem agendamento ativo");
		lv_label_set_text(sched_start_label, "");
		lv_label_set_text(sched_end_label, "");
		lv_label_set_text(sched_status_label, "");
		break;

	case SCHEDULE_CMD_LOAD:
		lv_label_set_text(sched_title_label, "PROXIMO AGENDAMENTO");
		lv_obj_set_style_text_color(sched_title_label, lv_color_hex(0xFFFFFF),
					    LV_PART_MAIN);
		lv_label_set_text(sched_user_label, user_buf);
		lv_label_set_text_fmt(sched_start_label, "Inicio: %s", start_buf);
		lv_label_set_text_fmt(sched_end_label, "Fim:    %s", end_buf);
		lv_label_set_text(sched_status_label, "Aguardando...");
		lv_obj_set_style_text_color(sched_status_label, lv_color_hex(0xFFCC00),
					    LV_PART_MAIN);
		break;

	case SCHEDULE_CMD_FIRST_USE:
	case SCHEDULE_CMD_EXTEND_TIME:
		lv_label_set_text(sched_title_label, "EM USO");
		lv_obj_set_style_text_color(sched_title_label, lv_color_hex(0x00FF88),
					    LV_PART_MAIN);
		lv_label_set_text(sched_user_label, user_buf);
		lv_label_set_text_fmt(sched_start_label, "Inicio: %s", start_buf);
		lv_label_set_text_fmt(sched_end_label, "Fim:    %s", end_buf);
		lv_label_set_text(sched_status_label, "Estacao ocupada");
		lv_obj_set_style_text_color(sched_status_label, lv_color_hex(0x00FF88),
					    LV_PART_MAIN);
		break;

	default:
		break;
	}
}

static void handle_display_cmd(const struct display_cmd_msg *msg)
{
	switch (msg->screen) {
	case DISPLAY_SCREEN_LOGO:
		lv_scr_load(logo_screen);
		break;

	case DISPLAY_SCREEN_WARNING:
		lv_label_set_text(warn_msg_label, msg->warning_msg);
		lv_scr_load(warning_screen);
		break;

	case DISPLAY_SCREEN_SCHEDULE:
		lv_scr_load(schedule_screen);
		break;

	default:
		LOG_WRN("Unknown screen id: %d", msg->screen);
		break;
	}
}

static void display_service_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if(!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return;
	}

	LOG_INF("Display service initializing");

	build_logo_screen();
	build_warning_screen();
	build_schedule_screen();

	/* st7789v_init leaves display in DISPOFF — send DISPON now */
	display_blanking_off(display_dev);

	lv_scr_load(logo_screen);

	const struct zbus_channel *chan;

	while (true) {
		int err = zbus_sub_wait(&display_sub, &chan,
					K_MSEC(CONFIG_DISPLAY_SERVICE_LVGL_PERIOD_MS));

		if (err == 0) {
			if (chan == &display_cmd_chan) {
				struct display_cmd_msg msg;

				if (zbus_chan_read(chan, &msg, K_MSEC(50)) == 0) {
					handle_display_cmd(&msg);
				}
			} else if (chan == &schedule_state_chan) {
				struct baiatool_schedule_state state;

				if (zbus_chan_read(chan, &state, K_MSEC(50)) == 0) {
					update_schedule_screen(&state);
					lv_scr_load(schedule_screen);
				}
			}
		}

		lv_timer_handler();
	}
}

K_THREAD_DEFINE(display_thread_id, CONFIG_DISPLAY_SERVICE_THREAD_STACK_SIZE,
		display_service_thread, NULL, NULL, NULL,
		CONFIG_DISPLAY_SERVICE_THREAD_PRIORITY, 0, 0);
