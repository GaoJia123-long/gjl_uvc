
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "stdbool.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "driver/gpio.h"
#include "driver/i2c.h"


#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************/

#define Soft_Reset 0x00
#define Data_ready 0x02
#define DATA1_MSB  0x10
#define DATA1_CSB  0x11
#define DATA1_LSB  0x12
#define CMD        0x30

//void init_i2c(uint8_t SCK_I2C,uint8_t SDA_I2C);
void JX90164_WriteReg(uint8_t reg,uint8_t data);
void JX90164_ReadReg(uint8_t reg,uint8_t* data,uint8_t len);
void init_jx90164(void);
void JX90164_Get_T_Data(int32_t* Temp);




/*********************************************************************************************/
#ifdef __cplusplus
}
#endif
