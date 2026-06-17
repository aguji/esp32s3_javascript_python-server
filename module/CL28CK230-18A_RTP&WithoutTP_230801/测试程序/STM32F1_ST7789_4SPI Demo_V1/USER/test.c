#include "sys.h"
#include "usart.h"		
#include "delay.h"	  
#include "lcd.h"   
#include "touch.h"  
#include "w25qx.h"
#include "test code.h"


//深圳市鸿讯电子有限公司
//TEL:13528896127
//创建日期:2023/3/30
//版本：V1.0
//单片机型号：STM32F103C8T6  3.3V

//=========================================电源接线================================================//
//VCC = DC 3.3V电源
//GND = 地

//=======================================液晶屏接线===========================================//
//数据总线类型为4SPI
// CS        = PA12             		片选信号
// RST       = PC15             		复位信号
// DC        = PA15            			数据/命令选择控制信号
// SDI(MOSI) = PC13  								数据输入
// SCK			 = PC14    							时钟
// SDO(MISO) = PA11									数据输出
// LED       = PA2            			背光开关控制


//=======================================电阻触摸接线==========================================//
//芯片：XPT2046
//TPEN  = PA5  中断
//TSDO  = PA3	 数据输出
//TSDI  = PA4  数据出入
//TCLK  = PA6  时钟
//TCS   = PA7	 片选

//=======================================电容触摸接线==========================================//
//芯片：FT6336
//FT_INT  		= PA5  中断
//CT_IIC_SDA  = PA4  数据 输出\出入
//CT_IIC_SCL  = PA6  时钟
//FT_RST 		  = PA7	 复位

//========================================FLASH接线===========================================//
//芯片：W25Q32FVSIG
//F_CS     = PA9  片选
//F_SCLK   = PA8  时钟      
//F_DIO    = PA0  数据输入
//DO_Read  = PA1  数据输出
//=============================================================================================//

/////////////////////////////////////用户配置区 在LCD.h文件///////////////////////////////////	 
//#define USE_LCM_DIR  	  0   	//定义液晶屏顺时针旋转方向 	0-0度旋转，1-180度旋转，2-270度旋转，3-90度旋转
//#define USE_TP_TYPE   	RTP 	//定义触摸类型  CTP = 电容模式 , RTP = 电阻模式


int main(void)
{		 
	
 	Stm32_Clock_Init(9);	//系统时钟设置
	uart_init(72,9600);	 	//串口初始化为9600
	delay_init(72);	   	 	//延时初始化 
	
	LCD_Init();			   	//初始化LCD 	
	tp_dev.touchtype|=USE_TP_TYPE;			//电容触摸&电阻触摸
	LCD_Display_Dir(USE_LCM_DIR);		 		//屏幕方向
	LCD_Clear(WHITE);		//清屏
	LCD_LED=1;					//点亮背光	

	main_test("IC:ST7789");		  //测试主页
	Color_Test();								//纯色测试
	Read_Test();								//读颜色测试
	FillRec_Test();							//图形测试
	English_Font_test();				//英文测试
	Chinese_Font_test();				//中文测试
	Pic_test();									//图片测试
	Switch_test();							//显示开关测试
	Rotate_Test();							//旋转测试

	//不需要触摸请注释掉以下代码
	LCD_Display_Dir(0);					//屏幕切换为竖屏状态
	tp_dev.init();							//触摸屏初始化
 	POINT_COLOR=RED;						//设置字体为红色 

	Load_Drow_Dialog();	 	
	if(tp_dev.touchtype&0X80)ctp_test();	//电容屏测试
	else rtp_test(); 											//电阻屏测试
}



