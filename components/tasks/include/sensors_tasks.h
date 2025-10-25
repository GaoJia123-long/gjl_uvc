
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2s_pdm.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************/
void init_microphone(uint8_t SCK,uint8_t SDL);
void init_laser(uint8_t txd_pin, uint8_t rxd_pin);

void sensors_task(void *pvParameters);


/*********************************************************************************************/

#ifdef __cplusplus
}
#endif