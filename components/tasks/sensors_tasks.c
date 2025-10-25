
#include "sensors_tasks.h"

#include "beep_key_led.h"
#include "jx90614.h"
#include "adxl345.h"  
#include "beep_key_led.h"


#define TAG "sensors_tasks"

extern SemaphoreHandle_t xMutex_i2c; // 定义互斥锁

#define LASER_UART_NUM  UART_NUM_1

// 指定按键IO为IO0
#define KEY_GPIO			GPIO_NUM_12

//PDM microphone
#define CONFIG_EXAMPLE_SAMPLE_RATE  36000 //44100  
#define CONFIG_EXAMPLE_BIT_SAMPLE   16
#define SPI_DMA_CHAN        SPI_DMA_CH_AUTO
#define NUM_CHANNELS        (1) // For mono recording only!
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 60 *2 )   //default:1024 调整需要的窗口大小 16*60*2 
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS //数据速率
#define BIOS 1400           //麦克风初始在 -1450 ~ 0 之间震荡
i2s_chan_handle_t rx_handle;
size_t bytes_read;

int16_t i2s_readraw_buff[SAMPLE_SIZE];
int16_t temp_i2s_readraw_buff[SAMPLE_SIZE];

//laser
const int RX_BUF_SIZE = 1024;  //1024






void init_microphone(uint8_t SCK,uint8_t SDL)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    
    i2s_pdm_rx_config_t pdm_rx_cfg = 
    {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_EXAMPLE_SAMPLE_RATE),
        /* The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down) */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = 
        {
            .clk = SCK,
            .din = SDL,
            .invert_flags = 
            {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}


void init_laser(uint8_t txd_pin, uint8_t rxd_pin)
{

    /**************************串口初始化************************** */
    //串口1 --- 激光
    const uart_config_t uart_config = 
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
   
    uart_param_config(LASER_UART_NUM, &uart_config);
    uart_set_pin(LASER_UART_NUM, txd_pin, rxd_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(LASER_UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0);

    // //串口0 --- 标定
    // uart_param_config(UART_NUM_0, &uart_config);
    // uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    
    /**************************激光测距出厂配置************************** */
    //固定反射强度设置为3档
    const char setting_intensity[7] = {0x5A,0x07,0x25,0x00,0x01,0x00,0x00};  //0x01,0x00,0x00:3档  00 01 00:2档
    uart_write_bytes(LASER_UART_NUM, setting_intensity, 7); 
    vTaskDelay(pdMS_TO_TICKS(20));

    // //0x59 0x59 HDist LDist HAmp LAmp Temp Current Check_sum
    // const char setting_test[5] = {0x5A,0x05,0x05,0x0B,0x6F};
    // uart_write_bytes(LASER_UART_NUM, setting_test, 5);
    // vTaskDelay(pdMS_TO_TICKS(20)); 

    //9字节mm输出
    const char setting_distance[5] = {0x5A,0x05,0x05,0x06,0x00};   // 0x01: cm  0x06: mm
    uart_write_bytes(LASER_UART_NUM, setting_distance, 5); 
    vTaskDelay(pdMS_TO_TICKS(20));  

    //250Hz [5A 06 03 FA 00 00]  低功耗模式10Hz输出 [5A 06 35 0A 00 00]
    const char setting_frequency[6] = {0x5A,0x06,0x35,0x0A,0x00,0x00};
    uart_write_bytes(LASER_UART_NUM, setting_frequency, 6);
    vTaskDelay(pdMS_TO_TICKS(20));  

    //掉电保存
    const char setting_saving[4] = {0x5A,0x04,0x11,0x00};
    uart_write_bytes(LASER_UART_NUM, setting_saving, 4);
    vTaskDelay(pdMS_TO_TICKS(100));  
}









void sensors_task(void *pvParameters) 
{
	const char *SENSORS_TASK_TAG = "sensors_task";
    ESP_LOGI(SENSORS_TASK_TAG, "Starting sensors task!");
    ESP_LOGI("sensors_task", "Running on core %d", xPortGetCoreID());

	
	//角度传感器ADXL345
	short a_x=0,a_y=0,a_z=0;
	float angx=0.0f,angy=0.0f,angz=0.0f;
	int16_t roll_current = 0;
	int16_t pitch_current = 0;
	int16_t roll_last =0;
	int16_t pitch_last =0;

	//laser and microphone
	uint16_t distance = 0;
	uint16_t intensity =0;
    uint8_t  *laser_data  = (uint8_t *)malloc(RX_BUF_SIZE * sizeof(uint8_t));
    assert(laser_data);

	//ADC初始化 ADC1_channel4 ---> GPIO_20
	adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = 
    {
        .unit_id = ADC_UNIT_1, 
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t config = 
    {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11 // 适用于最大3.6V输入
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &config);
    int adc_raw;

	//0. 蜂鸣器 激光绿灯 电源灯
	SetBeep(BEEP_ON); 
	SetLed(LED_ON); 
	SetLaser(LASER_ON); 



	while(1)
    {
		// //1. ADC
		// adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &adc_raw);
        // printf("%.1f\n", adc_raw*1.5); 


		// //2. 按键Key
		// if(getKeyValue(KEY_GPIO)==1)
		// {
		// 	//10ms消抖
		// 	vTaskDelay(pdMS_TO_TICKS(10));  
		// 	//二次判断
		// 	if(getKeyValue(KEY_GPIO)==1)
		// 	{
		// 		printf("Key testing! \r\n");
		// 		vTaskDelay(pdMS_TO_TICKS(500));
		// 	}
		// }

		// //3. 北醒TF-Luna 激光测距模块
        // int rxBytes = uart_read_bytes(LASER_UART_NUM, laser_data, RX_BUF_SIZE, pdMS_TO_TICKS(10));

        // if (rxBytes >= 6 && laser_data[0] == 0x59 && laser_data[1] == 0x59) //Laser decode
        // {
		// 	distance = (((uint16_t)laser_data[3]<<8)|((uint16_t)laser_data[2]<<0));  //mm
		// 	intensity = (((uint16_t)laser_data[5]<<8)|((uint16_t)laser_data[4]<<0));
		// 	printf("Distance: %d  Intensity: %d \r\n",distance,intensity);
		// }

		//4. SPM1423HM4H-B 麦克风模块
		if(i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 1000) == ESP_OK && bytes_read == SAMPLE_SIZE)
		{
			for (int i = 0; i < SAMPLE_SIZE; i++) 
			{
			    printf("I2S_readraw_buff[%d] = %d \r\n", i, i2s_readraw_buff[i]); 
			}   

			//memcpy(temp_i2s_readraw_buff, i2s_readraw_buff, sizeof(i2s_readraw_buff));
            // for (int i = 0; i < SAMPLE_SIZE; i++) 
            // {
            //     printf("i2s_readraw_buff[%d] = %d\n", i, temp_i2s_readraw_buff[i]); 
            // }
		}
		else 
		{
			ESP_LOGI(TAG, "Read Failed! \r\n");
		}

		// //5. ADXL345 角度传感器
		// short a_x=0,a_y=0,a_z=0;
		// if (xSemaphoreTake(xMutex_i2c, pdMS_TO_TICKS(20)) == pdTRUE) 
		// {
		// 	ADXL345_Read_Average(&a_x,&a_y,&a_z,10);   
		// 	xSemaphoreGive(xMutex_i2c);
		// }
        // angy = (ADXL345_Get_Angle(a_x,a_y,a_z,2))/10.0f;
		// printf("Pitch: %.2f \r\n",angy);

		// //6. JX90164 热红外测温
		// int32_t Temp =0,realT =0;
        // if (xSemaphoreTake(xMutex_i2c, pdMS_TO_TICKS(20)) == pdTRUE) 
		// {
		//     JX90164_Get_T_Data(&Temp);
        //     xSemaphoreGive(xMutex_i2c);
		// }
		// realT = Temp/(pow(2,14));
		// printf("IR_temp: %lu \r\n", realT);



		vTaskDelay(pdMS_TO_TICKS(100));
		
	}
	vTaskDelete(NULL);
}












// /**************************************************************************/
// //sealevel 通常表示海平面处的标准大气压 通常为1013.25 hPa（即标准大气压）
// float getAltitude(float seaLevel)
// {
//     // Equation taken from BMP180 datasheet (page 16):
//     //  http://www.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf

//     // Note that using the equation from wikipedia can give bad results
//     // at high altitude. See this thread for more information:
//     //  http://forums.adafruit.com/viewtopic.php?f=22&t=58064

//     float atmospheric = readPressure() / 100.0F;
//     return 44330.0 * (1.0 - pow(atmospheric / seaLevel, 0.1903));
// }
