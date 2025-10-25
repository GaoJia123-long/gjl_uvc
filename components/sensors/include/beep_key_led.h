
#pragma once

#include "stdint.h"
#include "stdbool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include <rom/ets_sys.h>
#include <stdint.h>
#include <stdio.h>

#include <driver/gpio.h>
#include "esp_rom_gpio.h"
#include "driver/ledc.h"
#include "esp_sleep.h"
#include "esp_timer.h"





#ifdef __cplusplus
extern "C" {
#endif
/*********************************************************************************************/
#define BEEP_CHANNEL      LEDC_CHANNEL_1    // PWM 通道 1
#define BEEP_TIMER        LEDC_TIMER_1      // PWM 定时器 1

#define LED0_CHANNEL       LEDC_CHANNEL_2    // PWM 通道 2
#define LEDC0_TIMER        LEDC_TIMER_2      // PWM 定时器 2

#define LED_CHANNEL       LEDC_CHANNEL_0    // PWM 通道 0
#define LEDC_TIMER        LEDC_TIMER_0      // PWM 定时器 0
#define PWM_FREQUENCY     3300              // PWM 信号频率（1 kHz）
#define PWM_RESOLUTION    LEDC_TIMER_8_BIT  // 8 位分辨率，亮度范围 0 - 255

#define	BEEP_ON			20 //70
#define	BEEP_OFF		0
#define	LASER_ON		50  
#define	LASER_OFF		0
#define	LED_ON		    80
#define	LED_OFF		    0

void init_beep_laser_led(uint8_t GPIO_beep,uint8_t GPIO_laser,uint8_t GPIO_led,uint8_t GPIO_key);
void SetBeep(uint8_t ONOFF);
void SetLaser(uint8_t ONOFF);
void SetLed(uint8_t ONOFF);
void BeepSound();
void BeepSound3();


// 定义按键短按长按事件枚举
typedef enum 
{
	KEY_SHORT_PRESS = 1, 
	KEY_LONG_PRESS,
} alink_key_t;



void IRAM_ATTR gpio_isr_handler(void *arg);
void init_key(uint32_t key_gpio_pin);
esp_err_t alink_key_scan(TickType_t ticks_to_wait);
int getKeyValue(uint8_t GPIO_KEY);
/*********************************************************************************************/
#ifdef __cplusplus
}
#endif