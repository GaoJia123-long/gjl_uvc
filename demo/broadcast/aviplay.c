#include "avi_player.h"
#include "jpeg2avi.h"
//#include "uvc.h"
#include "esp_attr.h"
#include "driver/jpeg_decode.h"

static const char *TAG = "avi_player_test";
static bool end_play = false;
frame_data_t *g_video_frame_data = NULL;



#include "freertos/queue.h"
#include "freertos/task.h"

//1static lv_obj_t *ui_videoMain = NULL;
static jpeg_decoder_handle_t jpgd_handle = NULL;
//1static lv_image_dsc_t video_dsc_main = {
//1    .header =
//1        {
//1            .cf = LV_COLOR_FORMAT_RGB565,
//1            .w = CAM1_PIXEL_WIDTH,
//1            .h = CAM1_PIXEL_HEIGHT,
//1        },
//1    .data_size = CAM1_PIXEL_NUM*CAM1_PIXEL_CHANNEL ,
//1};

#define VIDEO_FRAME_QUEUE_LEN 4
static QueueHandle_t video_frame_queue = NULL;

static void lvgl_video_task(void *arg)
{
    uint8_t *rx_buf = NULL;
    while (1) {
        if (xQueueReceive(video_frame_queue, &rx_buf, portMAX_DELAY) == pdTRUE) {
        //1    lvgl_port_lock(0); // 加锁
        //1    video_dsc_main.data = rx_buf;
        //1    lv_image_set_src(ui_videoMain, &video_dsc_main);
        //1    lvgl_port_unlock(); // 解锁
            vTaskDelay(30 / portTICK_PERIOD_MS);
            free(rx_buf);
        }
    }
}

void video_write(frame_data_t *data, void *arg)
{
    //static bool frame_displayed = false;
    //if (frame_displayed) {
    //    return;
    //}
    //frame_displayed = true;

    //if (diaplay_state != 1) {
    //    return;
    //}
    jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER };
    jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = { .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER };

    size_t tx_buffer_size = 0;
    uint8_t *tx_buf = (uint8_t*)jpeg_alloc_decoder_mem(data->data_bytes, &tx_mem_cfg, &tx_buffer_size);
    if (!tx_buf) return;
    memcpy(tx_buf, data->data, data->data_bytes);

    size_t rx_buffer_size = 0;
    uint8_t *rx_buf = (uint8_t*)jpeg_alloc_decoder_mem(240 * 240 * 2, &rx_mem_cfg, &rx_buffer_size);
    if (!rx_buf) { free(tx_buf); return; }

    jpeg_decode_picture_info_t header_info;
    if (jpeg_decoder_get_info(tx_buf, data->data_bytes, &header_info) != ESP_OK) {
        free(tx_buf); free(rx_buf); return;
    }

    jpeg_decode_cfg_t decode_cfg_rgb = { .output_format = JPEG_DECODE_OUT_FORMAT_RGB565, .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR };
    uint32_t out_size = 0;
    if (jpeg_decoder_process(jpgd_handle, &decode_cfg_rgb, tx_buf, data->data_bytes, rx_buf, rx_buffer_size, &out_size) != ESP_OK) {
        free(tx_buf); free(rx_buf); return;
    }
    free(tx_buf);
    vTaskDelay(30 / portTICK_PERIOD_MS);
    // 发送帧指针到队列
    if (video_frame_queue) {
        xQueueSend(video_frame_queue, &rx_buf, 0);
    } else {
        free(rx_buf);
    }
}

void audio_write(frame_data_t *data, void *arg)
{
  //  TEST_ASSERT_TRUE(data->type == FRAME_TYPE_AUDIO);
    ESP_LOGI(TAG, "Audio write: %d", data->data_bytes);
}

void audio_set_clock(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    ESP_LOGI(TAG, "Audio set clock, rate %"PRIu32", bits %"PRIu32", ch %"PRIu32"", rate, bits_cfg, ch);
}

void avi_play_end(void *arg)
{
    ESP_LOGI(TAG, "Play end");
    end_play = true;
}

extern TaskHandle_t avi_play_task_handle;
extern uint8_t diaplay_state;
void avi_uconplay(void)
{

    //1lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);
    //1ui_videoMain = lv_image_create(lv_screen_active());
    //1lv_obj_align(ui_videoMain, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    

    if (!video_frame_queue) {
        video_frame_queue = xQueueCreate(VIDEO_FRAME_QUEUE_LEN, sizeof(uint8_t *));
        xTaskCreate(lvgl_video_task, "lvgl_video_task", 4096, NULL, 5, NULL);
    }

    jpeg_decode_engine_cfg_t decode_eng_cfg = { .timeout_ms = 40 };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));

    end_play = false;
    avi_player_config_t config = {
        .buffer_size = 60 * 1024,
        .audio_cb = audio_write,
        .video_cb = video_write,
        .audio_set_clock_cb = audio_set_clock,
        .avi_play_end_cb = avi_play_end,
        .stack_size = 4096,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        .stack_in_psram = false,
#endif
    };

    avi_player_handle_t handle;
    avi_player_init(config, &handle);

    // 只有 diaplay_state==1 时才开始播放
    //while (diaplay_state != 1) {
    //    vTaskDelay(100 / portTICK_PERIOD_MS);
    //}
    avi_player_play_from_file(handle, "/sdcard/sample.avi");

    while (!end_play) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    avi_player_deinit(handle);
    diaplay_state = 0;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    avi_play_task_handle = NULL;
    vTaskDelete(NULL); // 任务自删除
}
