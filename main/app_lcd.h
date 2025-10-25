
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "stdbool.h"
#include "string.h"  
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"

#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
/*********************************************************************************************/

void init_lcd_lvgl();
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void example_lvgl_update_cb(lv_disp_drv_t *drv);
static void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area);
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);
static void example_touch_isr_cb(esp_lcd_touch_handle_t tp);
static void example_increase_lvgl_tick(void *arg);
static bool example_lvgl_lock(int timeout_ms);
static void example_lvgl_unlock(void);
void example_lvgl_port_task(void *arg);



/*********************************************************************************************/
#ifdef __cplusplus
}
#endif
