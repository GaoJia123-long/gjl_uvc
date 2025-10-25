
#include "beep_key_led.h"



uint8_t	DEEP_GPIO = 0,LED_GPIO = 0;
uint8_t Buzzer =0;

//蜂鸣器和激光头初始化
void init_beep_laser_led(uint8_t GPIO_beep,uint8_t GPIO_laser,uint8_t GPIO_led,uint8_t GPIO_key)
{
	// //LED指示灯
	// LED_GPIO = GPIO_led;
    // esp_rom_gpio_pad_select_gpio(LED_GPIO);
    // gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

	// // 配置LED指示灯通道 配置 LEDC 定时器
	ledc_timer_config_t led_timer2 = 
	{
		.speed_mode = LEDC_LOW_SPEED_MODE,  // 高速模式
		.timer_num = LEDC0_TIMER,            // 定时器号
		.duty_resolution = PWM_RESOLUTION,  // 分辨率
		.freq_hz = PWM_FREQUENCY,           // 频率
		.clk_cfg = LEDC_AUTO_CLK,           // 自动选择时钟源
	};
	ledc_timer_config(&led_timer2); // 配置定时器
 
	ledc_channel_config_t led_channel = 
	{
		.gpio_num = GPIO_led,               // LED 所在的 GPIO 引脚
		.speed_mode = LEDC_LOW_SPEED_MODE, // 高速模式
		.channel = LED0_CHANNEL,            // PWM 通道
		.intr_type = LEDC_INTR_DISABLE,    // 不使用中断
		.timer_sel = LEDC0_TIMER,           // 选择定时器
		.duty = 128,                       // 初始占空比（50%）
		.hpoint = 0,                       // 无硬件微调
	};
	ledc_channel_config(&led_channel);    // 配置通道


	// // 配置蜂鸣器通道 配置 LEDC 定时器
	ledc_timer_config_t ledc_timer1 = 
	{
		.speed_mode = LEDC_LOW_SPEED_MODE,  // 高速模式
		.timer_num = BEEP_TIMER,            // 定时器号
		.duty_resolution = PWM_RESOLUTION,  // 分辨率
		.freq_hz = PWM_FREQUENCY,           // 频率
		.clk_cfg = LEDC_AUTO_CLK,           // 自动选择时钟源
	};
	ledc_timer_config(&ledc_timer1); // 配置定时器
 
	ledc_channel_config_t beep_channel = 
	{
		.gpio_num = GPIO_beep,               // LED 所在的 GPIO 引脚
		.speed_mode = LEDC_LOW_SPEED_MODE, // 高速模式
		.channel = BEEP_CHANNEL,            // PWM 通道
		.intr_type = LEDC_INTR_DISABLE,    // 不使用中断
		.timer_sel = BEEP_TIMER,           // 选择定时器
		.duty = 128,                       // 初始占空比（50%）
		.hpoint = 0,                       // 无硬件微调
	};
	ledc_channel_config(&beep_channel);    // 配置通道

	ledc_set_duty(LEDC_LOW_SPEED_MODE, BEEP_CHANNEL,0);   //设置占空比
	ledc_update_duty(LEDC_LOW_SPEED_MODE, BEEP_CHANNEL);      //更新 PWM 信号

	
	// 配置激光头通道 配置 LEDC 定时器
	ledc_timer_config_t ledc_timer = 
	{
		.speed_mode = LEDC_LOW_SPEED_MODE,  // 高速模式
		.timer_num = LEDC_TIMER,            // 定时器号
		.duty_resolution = PWM_RESOLUTION,  // 分辨率
		.freq_hz = PWM_FREQUENCY,           // 频率
		.clk_cfg = LEDC_AUTO_CLK,           // 自动选择时钟源
	};
	ledc_timer_config(&ledc_timer); // 配置定时器

	ledc_channel_config_t ledc_channel = 
	{
		.gpio_num = GPIO_laser,            // LED 所在的 GPIO 引脚
		.speed_mode = LEDC_LOW_SPEED_MODE, // 高速模式
		.channel = LED_CHANNEL,            // PWM 通道
		.intr_type = LEDC_INTR_DISABLE,    // 不使用中断
		.timer_sel = LEDC_TIMER,           // 选择定时器
		.duty = 128,                       // 初始占空比（50%）
		.hpoint = 0,                       // 无硬件微调
	};
	ledc_channel_config(&ledc_channel);    // 配置通道

	// ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED_CHANNEL, 0);  // 设置占空比为 0
	// ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED_CHANNEL);   // 更新 PWM 信号，LED 关闭

	init_key(GPIO_key);
}

//蜂鸣器发声
void BeepSound()
{
	SetBeep(Buzzer);
	vTaskDelay(pdMS_TO_TICKS(50)); 
	SetBeep(BEEP_OFF);
}

//蜂鸣器发声
void BeepSound3()
{
	SetBeep(Buzzer);
	SetLaser(LASER_ON);
	vTaskDelay(pdMS_TO_TICKS(100)); 
	SetBeep(BEEP_OFF);
	SetLaser(LASER_OFF);
	vTaskDelay(pdMS_TO_TICKS(100));
	SetBeep(Buzzer);
	SetLaser(LASER_ON);
	vTaskDelay(pdMS_TO_TICKS(100)); 
	SetBeep(BEEP_OFF);
	SetLaser(LASER_OFF);
	vTaskDelay(pdMS_TO_TICKS(100));
	SetBeep(Buzzer);
	SetLaser(LASER_ON);
	vTaskDelay(pdMS_TO_TICKS(100)); 
	SetBeep(BEEP_OFF);
	//SetLaser(LASER_OFF);
}


//蜂鸣器开关
void SetBeep(uint8_t ONOFF)
{
    /******************进入临界区*******************/
    //portENTER_CRITICAL(&mux_beep);                 
	ledc_set_duty(LEDC_LOW_SPEED_MODE, BEEP_CHANNEL,ONOFF);   //设置占空比
	ledc_update_duty(LEDC_LOW_SPEED_MODE, BEEP_CHANNEL);      //更新 PWM 信号
	//portEXIT_CRITICAL(&mux_beep);    
    /******************退出临界区*******************/
}

//LED开关
void SetLed(uint8_t ONOFF)
{
	/******************进入临界区*******************/
    //portENTER_CRITICAL(&mux_laser);       
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LED0_CHANNEL,ONOFF);   //设置占空比
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LED0_CHANNEL);      //更新 PWM 信号
	//portEXIT_CRITICAL(&mux_laser);    
    /******************退出临界区*******************/

	// if(ONOFF == LED_ON)
	// {
	// 	gpio_set_level(LED_GPIO, LED_ON);
	// }
	// else
	// {
	// 	gpio_set_level(LED_GPIO, LED_OFF);
	// }
}

//激光头开关
void SetLaser(uint8_t ONOFF)
{
	/******************进入临界区*******************/
   // portENTER_CRITICAL(&mux_laser);       
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL,ONOFF);   //设置占空比
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL);      //更新 PWM 信号
	//portEXIT_CRITICAL(&mux_laser);    
    /******************退出临界区*******************/
}








// void init_key(uint8_t GPIO_KEY)
// {
// 	esp_rom_gpio_pad_select_gpio(GPIO_KEY);          //按键
// 	gpio_set_direction(GPIO_KEY, GPIO_MODE_INPUT);
// }

int getKeyValue(uint8_t GPIO_KEY)
{
	return gpio_get_level(GPIO_KEY);
}



// 创建队列的句柄
static QueueHandle_t gpio_evt_queue = NULL;

// GPIO中断处理函数
void IRAM_ATTR gpio_isr_handler(void *arg) 
{
	uint32_t gpio_num = (uint32_t) arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// 按键GPIO初始化
void init_key(uint32_t key_gpio_pin) 
{
	//配置GPIO结构体
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;		// 下降沿和上升沿触发中断
	io_conf.pin_bit_mask = 1 << key_gpio_pin;	// 设置GPIO号
	io_conf.mode = GPIO_MODE_INPUT;				// 模式输入
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;	// 端口上拉使能
	gpio_config(&io_conf);

	// 初始化GPIO事件队列
	gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));
	// 添加GPIO中断事件回调函数
	gpio_isr_handler_add(key_gpio_pin, gpio_isr_handler, (void *) key_gpio_pin);

	// 配置 EXT0 深度休眠唤醒
	esp_sleep_enable_ext1_wakeup(key_gpio_pin, 0); // 低电平触发唤醒
}

// 按键扫描函数，任务中调用
esp_err_t alink_key_scan(TickType_t ticks_to_wait) 
{
	uint32_t io_num;
	BaseType_t press_key = pdFALSE;
	BaseType_t release_key = pdFALSE;
	int backup_time = 0;

	while (1) 
	{
		
			//接收从消息队列发来的消息
			xQueueReceive(gpio_evt_queue, &io_num, ticks_to_wait);

			if (gpio_get_level(io_num) == 0) 
			{
				//当前低电平，记录下用户按下按键的时间点
				press_key = pdTRUE;
				backup_time = esp_timer_get_time();

				//如果当前GPIO口的电平已经记录为按下，则开始减去上次按下按键的时间点
			} 
			else if (press_key) 
			{
				//记录抬升时间点
				release_key = pdTRUE;
				backup_time = esp_timer_get_time() - backup_time;
			}
			
			//近当按下标志位和按键弹起标志位都为1时候，才执行回调
			if (press_key & release_key) 
			{
				press_key = pdFALSE;
				release_key = pdFALSE;
				//如果大于1s则回调长按，否则就短按回调
				if (backup_time > 1000000) 
				{
					return KEY_LONG_PRESS;
				} 
				else 
				{
					return KEY_SHORT_PRESS;
				}
			}

			vTaskDelay(pdMS_TO_TICKS(100));       //已测试，延时相对较准

	}
}