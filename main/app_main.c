
#include <stdio.h>
#include "esp_log.h"
#include "app_https.h"
#include "app_wifi.h"
#include "app_uvc.h"
#include "esp_log.h"
#include "esp_ota_ops.h" 

#define TAG "app_main"

void app_main(void)
{

    // 获取应用信息
    const esp_app_desc_t *app_desc = esp_app_get_description();
    
    // 打印版本信息
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32 Camera System with OTA");
    ESP_LOGI(TAG, "Build Version: %s", app_desc->version);
    ESP_LOGI(TAG, "Build Date: %s %s", app_desc->date, app_desc->time);
    ESP_LOGI(TAG, "IDF Version: %s", app_desc->idf_ver);
    ESP_LOGI(TAG, "========================================");

    
    app_wifi_init();
    app_https_init();
    app_uvc_init();
}
