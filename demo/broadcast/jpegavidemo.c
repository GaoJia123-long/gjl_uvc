#include "jpeg2avi.h"
#include <string.h>
#include "esp_log.h"
#include "uvc.h"

static const char *TAG = "jpegavidemo";

#define JPEG_NUM 100         //JPEG图像数量
#define JPEG_FRAME_QUEUE_LEN 20


FILE *fp_avi;

typedef struct {
    uint8_t *data;
    size_t size;
} jpeg_frame_t;

QueueHandle_t jpeg_frame_queue = NULL;


// 消费者任务：从队列取JPEG帧合成AVI
static void jpeg_avi_task(void *pargs)
{
    fp_avi = NULL;
    while (1) {
        if (video_state == 1) {
            if (fp_avi == NULL) {
                fp_avi = fopen("/sdcard/sample.avi", "wb");
                ESP_LOGI(TAG, "outfile:/sdcard/sample.avi");
                if (fp_avi == NULL) {
                    ESP_LOGE(TAG, "fopen fp_avi error");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                }
                jpeg2avi_start(fp_avi);
            }
            jpeg_frame_t frame;
            if (xQueueReceive(jpeg_frame_queue, &frame, pdMS_TO_TICKS(100)) == pdTRUE) {
                jpeg2avi_add_frame(fp_avi, frame.data, frame.size);
                free(frame.data);
            }
        } else {
            // 录像结束，关闭文件
            if (fp_avi) {
                jpeg2avi_end(fp_avi, 240, 240, 11);
                fclose(fp_avi);
                fp_avi = NULL;
                ESP_LOGI(TAG, "Done avi.");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    vTaskDelete(NULL);
}



void jprgavidemo(void)
{
    // 初始化JPEG帧队列
    if (jpeg_frame_queue == NULL) {
        jpeg_frame_queue = xQueueCreate(JPEG_FRAME_QUEUE_LEN, sizeof(jpeg_frame_t));
        if (jpeg_frame_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create jpeg_frame_queue");
            return;
        }
    }
    xTaskCreate(jpeg_avi_task, "jpeg_avi_task", 4096, NULL, 2, NULL);
    printf("end\n");
    return;
}

