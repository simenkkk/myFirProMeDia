/* 
 * File:   newmain.c
 * Author: Administrator
 *
 * Created on 2016��4��13��, ����2:36
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
bit     SHU;         //���ݴ���ʱ��

uchar NN;
uchar DEVICE_ADR;	//������ַ
uchar WORD_ADR;		//�ڴ��ַ
uchar REC_DATA;		//���յ�������
uchar temp;             //��ʱ����
uchar shu1;              //��ʱ������յ�������
uchar shu2;             //��ʱ������յ�������
/*************  �ⲿ�����ͱ������� *****************/
void init(void);//���绯����
void delay_24C02(void); // ��ʱ 5us
void I2cSetAck(void);//Ӧ��
/********************���ӻ�Ӧ��***********************/
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
        while(PT2314_CLR_CLK)              //CLK�ź�Ϊ��ִ��
        {
            if(PT2314_GET_DATA)           //CLKΪ��ʱ�ж�DATA�Ƿ�Ϊ�ߣ����������ʼ�ж�
            {
                while(PT2314_GET_DATA);   //DATAΪ����ȴ����
                if(PT2314_CLR_CLK)        //���ݱ�ͣ���ʱCLKΪ����Ϊ��ʼ�źţ�Ϊ�����ǣ��˳��ж�
                {
                    START_flag = 1;       //��ʼ�ź����ñ�־λ
                }
            }
            //�ж���ʼ�ź��Ժ󣬿�ʼ����������ַ
            if(START_flag==1)
            {
                //led = 1;
                START_flag = 0;
                for(i=8;i>0;i--)
                {
                    while(!PT2314_CLR_CLK);//�͵�ƽ�ȴ�
                    DEVICE_ADR <<= 1;
                    if(PT2314_GET_DATA) //���ݵĵ�һ��CLK�ߵ�ƽ����
                        DEVICE_ADR |= 0x01;
                    while(PT2314_CLR_CLK); //SCL�ߵ�ƽ״̬�͵ȴ�
                }
                I2cSetAck(); //���豸��ַӦ��
                //�ж��豸��ַ�Ƿ�Ϊ0xa9
                if(DEVICE_ADR==0xa9)
                {
                    led1 = 1;
                    for(i=8;i>0;i--)     //�����ڴ浥Ԫ
                    {
                        while(!PT2314_CLR_CLK);//�͵�ƽ�ȴ�
                        WORD_ADR<<=1;
                        if(PT2314_GET_DATA) //���ݵĵ�һ��CLK�ߵ�ƽ����
                            WORD_ADR |= 0x01;
                        while(PT2314_CLR_CLK); //SCL�ߵ�ƽ״̬�͵ȴ�
                    }
                    I2cSetAck(); //���ڴ��ַӦ��
                    //�ٴ��ж��Ƿ�����ʼ�ź�
                    while(!PT2314_CLR_CLK);  //�ȴ�Ӧ���źű�Ϊ��
                    while(PT2314_CLR_CLK)     //CLKΪ���ж�
                    {
                        if(PT2314_GET_DATA)   //���Ϊ��
                        {
                            while(PT2314_GET_DATA && PT2314_CLR_CLK);  //��ȴ�CLK����DATAһ��Ϊ��
                            if(PT2314_CLR_CLK && !PT2314_GET_DATA)   //�����DATAΪ�ͣ���Ϊ��ʼ�ź�
                            {
                                NowState = 1;
                            }
                            else  //���������ʼ�źţ���Ϊ������������
                            {
                                NowState = 0;
                                SHU = PT2314_GET_DATA;
                            }
                        }
                    }
                    //�����������ȡ����
                    if(NowState==1)
                    {
                        //led2 = 1;
                        NowState = 0;
                        for(i=8;i>0;i--)
                        {
                            while(!PT2314_CLR_CLK); //�ȴ�ʱ��
                            DEVICE_ADR <<= 1;
                            if(PT2314_GET_DATA) //���ݵĵ�һ��CLK�ߵ�ƽ����
                                DEVICE_ADR |= 0x01;
                            while(PT2314_CLR_CLK);
                        }
                        I2cSetAck(); //���ڴ��ַӦ��
                        if(DEVICE_ADR==0xa9)   //�ڴ��ַ������
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
                                    while(!PT2314_CLR_CLK);//�ȴ��ߵ�ƽ
                                    while(PT2314_CLR_CLK);//�ߵ�ƽά������
                                    temp <<= 1;
                                }
                                I2cSetAck(); //����Ӧ��
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
                        I2cSetAck(); //����Ӧ��
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
                            I2cSetAck(); //����Ӧ��
                        }
                        SEND_BUF[3] |= SHU;
                        //ֻ���ܴ���λ����
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