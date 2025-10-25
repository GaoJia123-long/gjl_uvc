
#include "jx90614.h"

#define I2C_MASTER_NUM         I2C_NUM_1   



static const char *TAG = "JX90614";

// #define MLX90614_SDA  (GPIO_NUM_8) 
// #define MLX90614_SCL  (GPIO_NUM_9) 


#define JX90164_ADDRESS 0x7F



// //I2C初始化在该函数声明，主函数中初始化一次就可以了
// void init_i2c(uint8_t SCK_I2C,uint8_t SDA_I2C)
// {
//     i2c_config_t conf;
//     conf.mode = I2C_MODE_MASTER;
//     conf.sda_io_num = SDA_I2C;  //SDA IO映射
//     conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
//     conf.scl_io_num = SCK_I2C;  //SCL IO映射
//     conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
//     conf.master.clk_speed = 100000;            //I2C CLK 频率
//     conf.clk_flags =0;
//     i2c_param_config(I2C_MASTER_NUM, &conf);  //配置I2C
//     i2c_driver_install(I2C_MASTER_NUM, conf.mode,0, 0, 0); //I2C功能安装使能函数
// }


void JX90164_WriteReg(uint8_t reg,uint8_t data)
{
    uint8_t write_buf[2] = {reg, data};
    i2c_master_write_to_device(I2C_MASTER_NUM,JX90164_ADDRESS, write_buf,sizeof(write_buf),5000/ portTICK_PERIOD_MS);
}

void JX90164_ReadReg(uint8_t reg,uint8_t* data,uint8_t len)
{
	i2c_master_write_read_device(I2C_MASTER_NUM,JX90164_ADDRESS,&reg,1,data,len,5000/ portTICK_PERIOD_MS);
}

void init_jx90164(void)
{
	//init_i2c(MLX90614_SCL,MLX90614_SDA);
	JX90164_WriteReg(CMD,0x00);
	JX90164_WriteReg(CMD,0x08);
	//JX90164_WriteReg(CMD,0x00);
}


void JX90164_Get_T_Data(int32_t* Temp)
{
	uint8_t DataH,DataC,DataL;
	uint8_t Flag = 0x00;
	JX90164_ReadReg(Data_ready,&Flag,1);
	 
	JX90164_ReadReg(DATA1_MSB,&DataH,1);
	JX90164_ReadReg(DATA1_CSB,&DataC,1);
	JX90164_ReadReg(DATA1_LSB,&DataL,1);

	*Temp = (DataH<<16)|(DataC<<8)|(DataL);
}




