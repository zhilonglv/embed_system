#include "STC15F2K60S2.H"
#include "LED_segment.h"
#include "KEY.h"
#include "ADC.h"
#include "Real_Time.h"
#include "EEPROM.h"
//#include "Uart.h"
//#include "M485.h"
#include "Beep.h"
#include "ModBus_Mid_Layer.h"
#define uchar unsigned char 
#define uint unsigned int
#define ID_Adder_High 0x00
#define ID_Adder_Low 0x01

#define Hour_Addr 0x02
#define Minute_Addr 0x03
bit flag_500us=0;
bit flag_1ms=0;
bit flag_10ms=0;
bit flag_100ms=0;
bit flag_1s=0;
uchar data_485=0;
char Dis[8];
uchar count_500us=0;
uchar count_1ms=0;
uchar count_10ms=0;
uchar count_100ms=0;
uchar count_1s=0;
uchar WATER=0x01;
uchar temp_K;
uint Light_Digit=0;
uint Temp_Digit=0;
uint player1=0;
uint player2=0;
uint player3=0;
bit p1,p2,p3;
uint wait;
uchar minute_save,hour_save;
void Data_fresh(){//刷新寄存器和线圈
	Regs[1]=Temp_Digit;
	Regs[2]=Light_Digit;
	Regs[3]=player1;
	Regs[4]=player2;
	Regs[5]=player3;
	Regs[6]=hour_save;
	Regs[7]=minute_save;
	Coil=p1?(Coil|0x01):(Coil&~0x01);
	Coil=p2?(Coil|0x02):(Coil&~0x02);
	Coil=p3?(Coil|0x04):(Coil&~0x04);
	Coil=bp_ON?(Coil|0x08):(Coil&~0x08);
}

void GPIO_Init(void)				                                                    //硬件初始化/BootLoad/BIOS
{  	
	P0M1=0x00;						//设置P0为推挽模式，点亮数码管
	P0M0=0xff;
	P2M1=0x00;
	P2M0=0x08;						//将P2^3设置为推挽模式，其余为准双向口模式
	P3M0 = 0x00 ;
  P3M1 = 0x00 ;
	P1M0 = 0x00 ;
  P1M1 = 0x00 ;       //P1?P3????
}
void Timer0_Init(void)	  //1mS@12.000MHz
{	AUXR &= 0x7f;		  
	TMOD &= 0xf0;		  //使用定时器0，16位重装载模式
	TH0=(65535-500)/256; //高8位赋初值
	TL0=(65535-500)%256; //低8位赋初值
	TR0=1;				  //启动定时器1	
	ET0=1;                //开启定时器0中断
}

void timer0() interrupt 1
{
	flag_500us=1;
	if(wait){wait--;}	 
}
void Func_500us(){
	flag_500us=0;
	count_500us++;
	if(count_500us==2){
		flag_1ms=1;
		count_500us=0;
	}
	Voice();//蜂鸣器驱动
}
void Key_Count(){//按键技术
	switch (temp_K){
		case 1:player1=player1+1;p1=!p1;break;//计数的同时，将线圈（p1和线圈等价）反转
		case 2:player2=player2+1;p2=!p2;break;
		case 3:player3=player3+1;p3=!p3;break;
	}
}
void Func_1ms(){
	flag_1ms=0;
	count_1ms++;						   
	if( count_1ms == 10 )   {
		flag_10ms = 1; 
		count_1ms = 0;  
	}
	Data_fresh();//更新寄存器和线圈
	Display();	//数码管&LED灯驱动
	count_key();//按键驱动
	ModBus_Instruct_Handle(Request);//Mid层检查是否有请求报文
	temp_K = key_push();//按键
		if(temp_K==1){
			write_add( Hour_Addr, realtime.hour );
		}
		else if(temp_K==2){
			write_add( Minute_Addr, realtime.second );
		}
		else if(temp_K==3){
			hour_save=read_add( Hour_Addr );
			_nop_();
			minute_save=read_add( Minute_Addr );
			_nop_();
		}
		Key_Count();
}
void Func_10ms(){
	flag_10ms=0;
	count_10ms++; 
	if( count_10ms == 10 )  {
		flag_100ms = 1; 
		count_10ms = 0;  
	}
	print_Light(p1 | p2*4 | p3*16 | bp_ON*64);
	ADC_Continue();
	Light_Digit=adc.ADC_Num[0];
	Temp_Digit=tempdata[(adc.ADC_Num[1]>>2)-65];
}
void get_number(){				//数码管显示时分秒
	Dis[0]=realtime.hour/10;
	Dis[1]=realtime.hour%10;
	Dis[2]=0x7f;
	Dis[3]=realtime.minute/10;
	Dis[4]=realtime.minute%10;
	Dis[5]=0x7f;
	Dis[6]=realtime.second/10;
	Dis[7]=realtime.second%10;
	print_LED(Dis[0],Dis[1],Dis[2],Dis[3],Dis[4],Dis[5],Dis[6],Dis[7]);
}

void Func_100ms(){
	flag_100ms=0;
	count_100ms++;
	if(count_100ms==10) {count_100ms=0;flag_1s=1;}
	realtime=GetDA1302();//获取时间
	get_number();////数码管显示时分秒
}


void Func_1s(){
	flag_1s=0;
	count_1s++;	
}
void main(){
	GPIO_Init();
	Timer0_Init();
	EA=1;
	IIC_init();												//初始化IIC，非易失存储器
	set_pose(0,7);										//让0~7位数码管亮
	Light_ON();												//打开流水灯
	set_LED_char(2,0x40);    					//3号位置显示横杠
	set_LED_char(5,0x40);              //4号位置显示横杠
	print_Light(WATER);								//流水灯显示0x01
	Segment_ON();											//开启数码管
	key_init();												//按键初始化
	set_Beep(2000,0xff);							//蜂鸣器2000Hz，0xff表示一直发声
	Beep_OFF();												//关闭蜂鸣器
	key_enable(1);										//使能按键1
	key_enable(2);										//使能按键2
	key_enable(3);										//使能按键3
	ModBus_INIT();											//初始化ModBus
	ADC_Func();													//注册ADC
	ADC_ON(LIGHT);											//
	ADC_ON(TEMP);												//
	Light_Init();												//初始化Light
	//Regs[0]=read_add( ID_Addr );	
	if(DS1302Read(DS1302_second_read)&0X80)
		 Init_DS1302();										//初始DS1302，向1302的CE置0，启动实时时钟
	Regs[0]=0;													
	Regs[0]=read_add( ID_Adder_High )*256;
	_nop_();
	Regs[0]=Regs[0]+read_add( ID_Adder_Low);//读取存储在非易失中的ID
	_nop_();
	while(1){
		if(flag_500us  ==1)	{Func_500us();}
		if(flag_1ms  ==1) {Func_1ms();   }
		if(flag_10ms ==1) {Func_10ms();ModBus_send(); }//Base层检查是否有响应报文发送
		if(flag_100ms==1) {Func_100ms();}
		if(flag_1s   ==1)	{Func_1s();}
	}
}


