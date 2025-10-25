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


#ifdef __cplusplus
extern "C" {
#endif
/*********************************************************************************************/

#define DEVICE_ID		0X00 	// ����ID,0XE5
#define THRESH_TAP		0X1D   	// �û���ֵ�Ĵ���
#define OFSX			0X1E
#define OFSY			0X1F
#define OFSZ			0X20
#define DUR				0X21
#define Latent			0X22
#define Window  		0X23 
#define THRESH_ACT		0X24	// �˶���ֵ�Ĵ���
#define THRESH_INACT	0X25 	// ��ֹ��ֵ�Ĵ���
#define TIME_INACT		0X26	// ��ֹʱ��			����1 sec /LSB
#define ACT_INACT_CTL	0X27	// �����˶�/��ֹ���
#define THRESH_FF		0X28	// ����������ֵ	�������300 mg��600 mg(0x05��0x09)֮���ֵ ����62.5 mg/LSB
#define TIME_FF			0X29 	// ��������ʱ��	�������100 ms��350 ms(0x14��0x46)֮���ֵ ����5ms/LSB
#define TAP_AXES		0X2A  
#define ACT_TAP_STATUS  0X2B 
#define BW_RATE			0X2C 
#define POWER_CTL		0X2D 

#define INT_ENABLE		0X2E		// �����ж�����
#define INT_MAP			0X2F
#define INT_SOURCE  	0X30
#define DATA_FORMAT	    0X31
#define DATA_X0			0X32
#define DATA_X1			0X33
#define DATA_Y0			0X34
#define DATA_Y1			0X35
#define DATA_Z0			0X36
#define DATA_Z1			0X37
#define FIFO_CTL		0X38
#define FIFO_STATUS		0X39

 
void init_i2c(uint8_t SCK_I2C,uint8_t SDA_I2C);

//uint8_t init_adxl345(uint8_t sclPin, uint8_t sdaPin) ;
uint8_t init_adxl345();
//�Զ�У׼
//xval,yval,zval:x,y,z���У׼ֵ
void ADXL345_AUTO_Adjust(char *xval,char *yval,char *zval);
// ��ȡоƬID
uint8_t GetDevicesID(void);

void ADXL345_WR_Reg(uint8_t addr,uint8_t val);
uint8_t ADXL345_RD_Reg(uint8_t addr);


//��ȡADXL��ƽ��ֵ
//x,y,z:��ȡ10�κ�ȡƽ��ֵ
void ADXL345_RD_Avval(short *x,short *y,short *z);
//��ȡ3���������
//x,y,z:��ȡ��������
void ADXL345_RD_XYZ(short *x,short *y,short *z);
void ADXL345_Read_Average(short *x,short *y,short *z,uint8_t times);
float ADXL345_Get_Angle(float x,float y,float z,uint8_t dir);
uint16_t Get_ADXL345_Average(void);





/*********************************************************************************************/
#ifdef __cplusplus
}
#endif
