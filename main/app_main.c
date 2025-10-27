

#include "sdkconfig.h"              
#include "app_https.h"
#include "app_wifi.h"
#include "app_uvc.h"
#include "esp_ota_ops.h" 
#include "app_ble.h" 
#include "app_lcd.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "app_sd_card.h"


#define TAG "app_main"





/**********************************************************************************************************************************/
/****************************************************** Main **********************************************************************/
/**********************************************************************************************************************************/
void app_main(void)
{
    // /********************* Initialization *********************************/
    ESP_LOGI(TAG, "Initialize Flash");
    nvs_flash_init();

        
    ESP_LOGI(TAG, "Initialize LCD");
    init_lcd_lvgl();

    ESP_LOGI(TAG, "Create LVGL Tasks");
    xTaskCreatePinnedToCore(example_lvgl_port_task,"LVGL", (12*1024 / sizeof(StackType_t)) ,NULL,1,NULL,1);

    // Wait for LVGL task to start
    vTaskDelay(pdMS_TO_TICKS(100));

    // Temporarily disable WiFi/BLE to let SD card use SDMMC exclusively
    // ESP_LOGI(TAG, "Initialize WiFi");
    // app_wifi_init();
    // ESP_LOGI(TAG, "Initialize Https");
    // app_https_init();
    ESP_LOGI(TAG, "Initialize UVC");
    app_uvc_init();
    // ESP_LOGI(TAG, "Initialize BLE");
    // app_ble_init();

    // Initialize SD Card with LDO power control
    ESP_LOGI(TAG, "Initialize SD Card");
    if (app_sd_card_init() != ESP_OK) {
        ESP_LOGW(TAG, "SD card initialization failed, recording will not be available");
    }
   

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

 
























