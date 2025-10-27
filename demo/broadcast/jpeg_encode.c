/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/jpeg_encode.h"
#include "esp_attr.h"
#include "driver/jpeg_decode.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
//#include "uvc.h"
#include "jpeg2avi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t jpeg_frame_queue;
typedef struct {
    uint8_t *data;
    size_t size;
} jpeg_frame_t;

static const char *TAG = "jpeg.example";
static sdmmc_card_t *s_card;
#define MOUNT_POINT "/sdcard"

FILE *file_raw_1080p;
FILE *file_jpg_1080p;
uint8_t *jpg_buf_1080p;
uint8_t *raw_buf_1080p;
jpeg_encoder_handle_t jpeg_handle;
uint32_t raw_size_1080p;
uint32_t jpg_size_1080p;
size_t rx_buffer_size = 0;

//1static lv_image_dsc_t video_dsc_main = {
//1    .header =
//1        {
//1            .cf = LV_COLOR_FORMAT_RGB565,
//1            .w = CAM1_PIXEL_WIDTH,
//1            .h = CAM1_PIXEL_HEIGHT,
//1        },
//1    .data_size = CAM1_PIXEL_NUM*CAM1_PIXEL_CHANNEL ,
//1};


static esp_err_t sdcard_init(void)
{
    esp_err_t ret = ESP_OK;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

#if CONFIG_EXAMPLE_SDMMC_IO_POWER_INTERNAL_LDO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4, // `LDO_VO4` is used as the SDMMC IO power
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to new an on-chip ldo power control driver");
        return ret;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    
    sdmmc_card_print_info(stdout, s_card);
    return ret;
}

static void sdcard_deinit(void)
{
    const char mount_point[] = MOUNT_POINT;
    esp_vfs_fat_sdcard_unmount(mount_point, s_card);
#if SOC_SDMMC_IO_POWER_EXTERNAL
    esp_err_t ret = sd_pwr_ctrl_del_on_chip_ldo(s_card->host.pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete on-chip ldo power control driver");
        return;
    }
#endif
}
 //   u_int8_t pixel_array2[CAM1_PIXEL_NUM*CAM1_PIXEL_CHANNEL] = {0};
static void task(void *args)
{
        jpeg_encode_cfg_t enc_config = {
        .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
        .image_quality = 30,
        .width = 240,
        .height = 240,
    };

   while(1){
#if 1    
    //1 if(video_state == 1){
    
    //1  memcpy(raw_buf_1080p, pixel_array1, raw_size_1080p); // 拷贝数据
   
        ESP_ERROR_CHECK(jpeg_encoder_process(jpeg_handle, &enc_config, raw_buf_1080p, raw_size_1080p, jpg_buf_1080p, rx_buffer_size, &jpg_size_1080p));

        // 发送JPEG帧到队列
        if (jpeg_frame_queue && jpg_size_1080p > 0) {
            jpeg_frame_t frame;
            frame.data = malloc(jpg_size_1080p);
            if (frame.data) {
                memcpy(frame.data, jpg_buf_1080p, jpg_size_1080p);
                frame.size = jpg_size_1080p;
                if (xQueueSend(jpeg_frame_queue, &frame, 0) != pdTRUE) {
                    free(frame.data); // 队列满时释放内存
                }
            }
        }
    //1}
#endif
vTaskDelay(pdMS_TO_TICKS(1));
   };
   vTaskDelete(NULL);
}

void jpeg_encode(void)
{
   // TaskHandle_t task_handle = NULL;
    jpeg_encode_engine_cfg_t encode_eng_cfg = {
        .timeout_ms = 70,
    };

    jpeg_encode_memory_alloc_cfg_t rx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    jpeg_encode_memory_alloc_cfg_t tx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };

    ESP_ERROR_CHECK(jpeg_new_encoder_engine(&encode_eng_cfg, &jpeg_handle));

    raw_size_1080p = 115200;

    size_t tx_buffer_size = 0;
    raw_buf_1080p = (uint8_t*)jpeg_alloc_encoder_mem(raw_size_1080p, &tx_mem_cfg, &tx_buffer_size);
    assert(raw_buf_1080p != NULL);

  


    jpg_buf_1080p = (uint8_t*)jpeg_alloc_encoder_mem(raw_size_1080p / 10, &rx_mem_cfg, &rx_buffer_size); // Assume that compression ratio of 10 to 1
    assert(jpg_buf_1080p != NULL);

    ESP_LOGE(TAG, "JPEG encoder 111111111111111111");
   
    xTaskCreate(task, "jpeg_encode_task", 4096, NULL, 2, NULL);
    
}

void jpeg_decode(void)
{
  //  ESP_ERROR_CHECK(sdcard_init());

    //1 lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);
    //1lv_obj_t *ui_videoMain = lv_image_create(lv_screen_active());
//1
    //1lv_obj_align(ui_videoMain, LV_ALIGN_TOP_LEFT, 0, 0);

    jpeg_decoder_handle_t jpgd_handle;

    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };

    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));

    jpeg_decode_cfg_t decode_cfg_rgb = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };


    jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };

   
for (size_t i = 0; i < 100; i++)
{
    /* code */


    int jpeg_size_1080p = 11520;
    
    size_t tx_buffer_size_1080p = 0;
    uint8_t *tx_buf_1080p = (uint8_t*)jpeg_alloc_decoder_mem(jpeg_size_1080p, &tx_mem_cfg, &tx_buffer_size_1080p);
    if (tx_buf_1080p == NULL) {
        ESP_LOGE(TAG, "alloc 1080p tx buffer error");
        return;
    }

    
    memcpy(tx_buf_1080p, g_video_frame_data->data, jpeg_size_1080p);

    

    size_t rx_buffer_size_1080p = 0;
   
    uint8_t *rx_buf_1080p = (uint8_t*)jpeg_alloc_decoder_mem(240 * 240 * 2, &rx_mem_cfg, &rx_buffer_size_1080p);
       if (rx_buf_1080p == NULL) {
        ESP_LOGE(TAG, "alloc 1080p rx buffer error");
        return;
    }
   
    jpeg_decode_picture_info_t header_info;
    ESP_ERROR_CHECK(jpeg_decoder_get_info(tx_buf_1080p, jpeg_size_1080p, &header_info));
    ESP_LOGI(TAG, "header parsed, width is %" PRId32 ", height is %" PRId32, header_info.width, header_info.height);

    uint32_t out_size_1080p = 0;
       ESP_ERROR_CHECK(jpeg_decoder_process(jpgd_handle, &decode_cfg_rgb, tx_buf_1080p, jpeg_size_1080p, rx_buf_1080p, rx_buffer_size_1080p, &out_size_1080p));
    //1   video_dsc_main.data = (const uint8_t *)rx_buf_1080p;
    //1    lv_image_set_src(ui_videoMain, &video_dsc_main);
}
   
    ESP_LOGI(TAG, "Card unmounted");

}
