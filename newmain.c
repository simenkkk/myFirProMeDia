/* 
 * File:   newmain.c
 * Author: Administrator
 *
 * Created on 2016年4月13日, 下午2:36
 */

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>

#define   led   GP0
#define   led1  GP4
#define   led2  GP5
    
#define	uchar	unsigned char
#define uint	unsigned int

#define PT2314_SET_DATA  GP2 = 1
#define PT2314_CLR_DATA  GP2 = 0

#define PT2314_CLR_CLK   GP1

#define PT2314_DATA_IN   TRIS = 0x06;

#define PT2314_GET_DATA  GP2

#define PT2314_DATA_OUT  TRIS = 0x02;

unsigned char SEND_BUF[5];

bit	PreState;
bit	NowState;
bit     START_flag;
bit     STOP_flag;
bit     SHU;         //数据处理时间

uchar NN;
uchar DEVICE_ADR;	//器件地址
uchar WORD_ADR;		//内存地址
uchar REC_DATA;		//接收到的内容
uchar temp;             //暂时数据
uchar shu1;              //暂时保存接收到的数据
uchar shu2;             //暂时保存接收到的数据
/*************  外部函数和变量声明 *****************/
void init(void);//初如化函数
void delay_24C02(void); // 延时 5us
void I2cSetAck(void);//应答
/********************主从机应答***********************/
void I2cSetAck(void)
{
    while(!PT2314_CLR_CLK);
    PT2314_DATA_OUT;
    asm("nop");
    PT2314_CLR_DATA;
    while(PT2314_CLR_CLK);
    asm("nop");
    asm("nop");
    //PT2314_SET_DATA;
    asm("nop");
    PT2314_DATA_IN;
}

void main()
{
    OSCCAL = 0x70;
    OPTION = 0x40;
    TRIS   = 0x06;
    GPIO   = 0x00;
    START_flag = 0;
    unsigned char M,i,j;
    while(1)
    {
        while(PT2314_CLR_CLK)              //CLK信号为高执行
        {
            if(PT2314_GET_DATA)           //CLK为高时判断DATA是否为高，高则进入起始判断
            {
                while(PT2314_GET_DATA);   //DATA为高则等待变低
                if(PT2314_CLR_CLK)        //数据变低，此时CLK为高则为起始信号，为低则不是，退出判断
                {
                    START_flag = 1;       //起始信号则置标志位
                }
            }
            //判断起始信号以后，开始接收器件地址
            if(START_flag==1)
            {
                //led = 1;
                START_flag = 0;
                for(i=8;i>0;i--)
                {
                    while(!PT2314_CLR_CLK);//低电平等待
                    DEVICE_ADR <<= 1;
                    if(PT2314_GET_DATA) //数据的第一个CLK高电平来临
                        DEVICE_ADR |= 0x01;
                    while(PT2314_CLR_CLK); //SCL高电平状态就等待
                }
                I2cSetAck(); //对设备地址应答
                //判断设备地址是否为0xa9
                if(DEVICE_ADR==0xa9)
                {
                    led1 = 1;
                    for(i=8;i>0;i--)     //接收内存单元
                    {
                        while(!PT2314_CLR_CLK);//低电平等待
                        WORD_ADR<<=1;
                        if(PT2314_GET_DATA) //数据的第一个CLK高电平来临
                            WORD_ADR |= 0x01;
                        while(PT2314_CLR_CLK); //SCL高电平状态就等待
                    }
                    I2cSetAck(); //对内存地址应答
                    //再次判断是否有起始信号
                    while(!PT2314_CLR_CLK);  //等待应答信号变为高
                    while(PT2314_CLR_CLK)     //CLK为高判断
                    {
                        if(PT2314_GET_DATA)   //如果为高
                        {
                            while(PT2314_GET_DATA && PT2314_CLR_CLK);  //则等待CLK或者DATA一个为低
                            if(PT2314_CLR_CLK && !PT2314_GET_DATA)   //如果是DATA为低，则为起始信号
                            {
                                NowState = 1;
                            }
                            else  //如果不是起始信号，则为接收主机数据
                            {
                                NowState = 0;
                                SHU = PT2314_GET_DATA;
                            }
                        }
                    }
                    //如果是主机读取数据
                    if(NowState==1)
                    {
                        //led2 = 1;
                        NowState = 0;
                        for(i=8;i>0;i--)
                        {
                            while(!PT2314_CLR_CLK); //等待时钟
                            DEVICE_ADR <<= 1;
                            if(PT2314_GET_DATA) //数据的第一个CLK高电平来临
                                DEVICE_ADR |= 0x01;
                            while(PT2314_CLR_CLK);
                        }
                        I2cSetAck(); //对内存地址应答
                        if(DEVICE_ADR==0xa9)   //内存地址读命令
                        {
                            for(i=0;i<4;i++)
                            {
                                temp = SEND_BUF[i];
                                for(j=8;j>0;j--)
                                {
                                    if(temp & 0x80)
                                    {
                                        PT2314_DATA_IN;
                                    }
                                    else
                                    {
                                        PT2314_DATA_OUT;
                                        PT2314_GET_DATA = 0;
                                    }
                                    while(!PT2314_CLR_CLK);//等待高电平
                                    while(PT2314_CLR_CLK);//高电平维持数据
                                    temp <<= 1;
                                }
                                I2cSetAck(); //数据应答
                            }
                        }
                    }
                    else
                    {
                        //led2 = 0;
                        for(i=7;i>0;i--)
                        {
                            while(!PT2314_CLR_CLK);
                            SEND_BUF[3] <<= 1;
                            if(PT2314_GET_DATA)
                                SEND_BUF[3] |= 0x01;
                            while(PT2314_CLR_CLK);
                        }
                        I2cSetAck(); //数据应答
                        for(i=0;i<3;i++)
                        {
                            for(j=8;j>0;j--)
                            {
                                while(!PT2314_CLR_CLK);
                                SEND_BUF[i] <<= 1;
                                if(PT2314_GET_DATA)
                                    SEND_BUF[i] |= 0x01;
                                while(PT2314_CLR_CLK);
                            }
                            I2cSetAck(); //数据应答
                        }
                        SEND_BUF[3] |= SHU;
                        //只接受此三位数据
                        if(WORD_ADR==0x01)
                        {
                            NN = 1;
                        }
                        else if(WORD_ADR==0x02)
                        {
                            NN = 2;
                        }
                        else if(WORD_ADR==0x03)
                        {
                            NN = 3;
                        }
                        //
                        shu1 = SEND_BUF[NN];
                        shu1 ^= 0x5a;
                        //
                        shu2 = SEND_BUF[NN-1];
                        shu2 ^= 0xa5;
                        for(i=0;i<4;i++)
                        {
                            SEND_BUF[i] ^= shu1;
                            SEND_BUF[i] ^= shu2;
                        }
                    }
                }
            }
        }
    }
}