/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "lv_demos.h"
#include "uvc.h"
#include "ui.h"
#include "sdmmc.h"

#include "jpeg2avi.h"

static const char *TAG = "main";

void app_main(void)
{
    
   
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    //bsp_display_lock(0);
    //lv_demo_music();
    // lv_demo_benchmark();
    // lv_demo_widgets();
      ESP_ERROR_CHECK(sdcard_init());

        ESP_LOGE(TAG," uvc_main ok11111111111111");
   // single_mjpeg_encode_test();
        jpeg_encode();

        jprgavidemo();
   //avi_uconplay();
    ESP_LOGE(TAG," uvc_main ok2222222222");

      uvc_main();
     
    sdcard_deinit();




    while(1)
    {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    //   

    //bsp_display_unlock();
}
