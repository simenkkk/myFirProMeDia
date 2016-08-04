

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h> 
#include <string.h>


#include "api/sp5k_utility_api.h"
#include "api/sp5k_global_api.h"
#include "app_gSensor.h"
#include "app_gSensor_def.h"
#include "../../hostfw/include/app_ui_para.h"
#include "../../hostfw/include/app_view_param_def.h"
#include "../../hostfw/include/app_hw_evb_def.h"
#include "../../hostfw/cathyware/inc/oss/linuxbase/asm/types.h" 




#define	ERROR_CODE_READ_ADDR 	1
#define	ERROR_CODE_TRUE			0
#define	ERROR_CODE_WRITE_DATA	2
#define	ERROR_CODE_WRITE_ADDR	1
 
#define GPIO_CFG_IN(PinNo)  (0x00000000L << (PinNo))
#define GPIO_CFG_OUT(PinNo) (0x00000001L << (PinNo))
#define GPIO_MASK(PinNo)    (0x00000001L << (PinNo))

#define GPIO_PIN_VAL_LO(PinNo) (0x00000000L << (PinNo))
#define GPIO_PIN_VAL_HI(PinNo) (0x00000001L << (PinNo))

#define SDA_PIN	(26)
#define SCL_PIN	(27)

#define I2C_DELAY(ucDelay)		sp5kTimeDelay(SP5K_TIME_DELAY_10US, ucDelay);
#define I2C_DELAY_LONG(ucDelay)	sp5kTimeDelay(SP5K_TIME_DELAY_10US, ucDelay);

#define SDA_HIGH		sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SDA_PIN), GPIO_PIN_VAL_HI(SDA_PIN))
#define SDA_LOW			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SDA_PIN), GPIO_PIN_VAL_LO(SDA_PIN))
#define SCL_HIGH		sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SCL_PIN), GPIO_PIN_VAL_HI(SCL_PIN))
#define SCL_LOW			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SCL_PIN), GPIO_PIN_VAL_LO(SCL_PIN))
								
#define SDA_OUT			sp5kGpioCfgSet(SP5K_GPIO_GRP_LMI,GPIO_MASK(6),GPIO_CFG_OUT(6))

#define SCL_HIGH		sp5kGpioWrite(SP5K_GPIO_GRP_LMI, GPIO_MASK(6), GPIO_PIN_VAL_HI(6))

#define SDA_IN			sp5kGpioCfgSet(SP5K_GPIO_GRP_FML,GPIO_MASK(SDA_PIN),GPIO_CFG_IN(SDA_PIN))
#define SDA_read		gpioPinLevelGet(SP5K_GPIO_GRP_FML,GPIO_MASK(SDA_PIN))


#define DA380_ADDR_READ		0XB5
#define DA380_ADDR_WRITE	0xB4
#define ENCRTY_MODE1		0X01
#define ENCRTY_MODE2		0X02
#define ENCRTY_MODE3		0X03


#define SDA_H			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SDA_PIN), GPIO_PIN_VAL_HI(SDA_PIN))
#define SDA_L			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SDA_PIN), GPIO_PIN_VAL_LO(SDA_PIN))
#define SCL_H			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SCL_PIN), GPIO_PIN_VAL_HI(SCL_PIN))
#define SCL_L			sp5kGpioWrite(SP5K_GPIO_GRP_FML, GPIO_MASK(SCL_PIN), GPIO_PIN_VAL_LO(SCL_PIN))

#define SDA_OUT			sp5kGpioCfgSet(SP5K_GPIO_GRP_FML,GPIO_MASK(SDA_PIN),GPIO_CFG_OUT(SDA_PIN))
#define SDA_IN			sp5kGpioCfgSet(SP5K_GPIO_GRP_FML,GPIO_MASK(SDA_PIN),GPIO_CFG_IN(SDA_PIN))
#define READ_SDA		gpioPinLevelGet(SP5K_GPIO_GRP_FML,GPIO_MASK(SDA_PIN))

#define delay_us(us)		sp5kTimeDelay(SP5K_TIME_DELAY_10US, ((us)*(2)));
#define delay_xus(us)		sp5kTimeDelay(SP5K_TIME_DELAY_1US, us);

//---------------------------------------------------------I2C LIB--






//产生IIC起始信号
void IIC_Start(void)
{
	SDA_OUT;     //sda线输出
	delay_us(2);
	SDA_H;  
	delay_us(4);
	SCL_H;
	delay_us(4);
 	SDA_L;//START:when CLK is high,DATA change form high to low 
	delay_us(4);
	SCL_L;//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void IIC_Stop(void)
{
	SDA_OUT;//sda线输出
	delay_us(4);
	SCL_L;
	delay_us(4);
	SDA_L;//STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	SCL_H; 
	delay_us(4);
	SDA_H;	//C_SDA=1;//发送I2C总线结束信号
	delay_us(4);							   	
}
//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
u8 IIC_Wait_Ack(void)
{
	u8 ucErrTime=0;
	SDA_IN;      //SDA设置为输入  
	SDA_H;		 //IIC_SDA=1;
	delay_us(2);	   
	SCL_H;
	delay_us(2);	 
	while(READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime > 250)
		{
			IIC_Stop();
			return 1;
		}
	}
	SCL_L;//时钟输出0 	   
	return 0;  
} 
//产生ACK应答
void IIC_Ack(void)
{
	SCL_L;
	delay_us(4);
	SDA_OUT;
	delay_us(4);
	SDA_L;
	delay_us(4);
	SCL_H;
	delay_us(4);
	SCL_L;

}
//不产生ACK应答		    
void IIC_NAck(void)
{
	SCL_L;
	delay_us(4);
	SDA_OUT;
	delay_us(4);
	SDA_H;				//IIC_SDA=1;
	delay_us(4);
	SCL_H;
	delay_us(4);
	SCL_L;
}					 				     
//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
void IIC_Send_Byte(u8 txd)
{                        
    u8 t;   
	SDA_OUT; 	 
	delay_us(4);
    SCL_L;//拉低时钟开始数据传输
    delay_us(4);
    for(t=0;t<8;t++)
    {              
        if(txd & 0x80)
        {
        	SDA_H;
        }
        else
        	SDA_L;
        txd<<=1; 	  
		delay_us(4);   
		SCL_H;
		delay_us(4); 
		SCL_L;	
		delay_us(4);
    }	 
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
u8 IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	SDA_IN;//SDA设置为输入
	delay_us(2);
	SDA_H;
    for(i=0;i<8;i++ )
	{
        SCL_L; 
        delay_us(4);
		SCL_H;
       delay_xus(2);
        receive<<=1;
        if(READ_SDA)
        	receive++;   
		delay_us(3); 
    }					 
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK 

    delay_us(4);
    
    return receive;
}


static UINT8 Zhe_Encrypt_i2c_write(UINT8 sub_addr, char *date)
{
	UINT8 i;

	IIC_Start();																		//Encrypt_i2c_start();
	IIC_Send_Byte(DA380_ADDR_WRITE);
	IIC_Ack();

	IIC_Send_Byte(sub_addr);
	IIC_Ack();

	
	for(i = 0; i < 4; i++)
	{
		IIC_Send_Byte(date[i]);
		IIC_Ack();
		delay_us(4);

	}
	I2C_DELAY(15);
	IIC_Stop();
	//Encrypt_i2c_stop();
	return ERROR_CODE_TRUE;
}

typedef struct node
{
	int dat;

	struct node *next;

}Node;


void creatList(int n)
{
	Node *pHead,*p1,*p2;

	int i;

	for(i=0;i<n;i++)
	{
		p1 = (Node *)malloc(sizeof(Node));

		if(pHead == NULL)
		{
			pHead = p1;
		}
		else
		{
			p2->next= p1;
		}
		p2 = p1;
		p1->next = NULL;		
	}
		return;
}

void reserve(Node *pHead)
{
	if(pHead->next == NULL)
		return pHead;

	Node *p,*q,*t;

	p = pHead->next;
	q = pHead->next->next;

	
		
}

static UINT8 Zhe_Encrypt_i2c_read(UINT8 sub_addr, UINT8 *date)
{
	//printf("\r\n_i2c_read");
	UINT8 i;
	
	IIC_Start();	

	IIC_Send_Byte(DA380_ADDR_WRITE);
	IIC_Ack();

	IIC_Send_Byte(sub_addr);
	IIC_Ack();

	IIC_Start();	
	IIC_Send_Byte(DA380_ADDR_READ);
	IIC_Ack();


	for(i = 0; i < 4; i++)
	{
		date[i] = IIC_Read_Byte(1);
		//delay_us(4);
		//IIC_Wait_NAck();
	}
	
	I2C_DELAY(15);
	IIC_Stop();
	return ERROR_CODE_TRUE;
}

//----------------------------------------------------

typedef struct node
{
	int value;
	struct node *pnext;

}Node;

Node *creatList(int n)
{
	int i;

	Node *head,*p1,*p2;

	head = NULL;
	for(i=0;i<n;i++)
	{
		p1 = (Node *)malloc(sizeof(Node));

		scanf("%d",&p1->value)

		if(head == NULL)
		{
			head = p1;
		}
		else
		{
			p2 ->next = p1;
		}
		p2 = p1;
		p1->next = NULL;
	}
	return head;
}

Node *reserve(Node *pHead)
{
	if(pHead ->next == NULL || pHead->next->next == NULL)
		return pHead;

	Node *p =pHead ->next;
	Node *q =pHead ->next->next;
	Node *t = NULL;

	while(q != NULL)
	{
		t = q->next;
		q->next = p;
		p = q;
		q = t;		
	}
	pHead->next = p;

	return pHead;
}


void delList(Node *head)
{
	if(head == NULL)
		return;
		
	Node *pHead;
	while(head != NULL)
	{
		pHead = head->next;
		free(head);
		head = pHead;
	}
	
}

void initList(Node **pList)
{
	*pList = NULL;	
}

struct LNode
{
	int data;
	struct LNode *next;
};


struct LNode *creat(int n)
{

	int i;
	struct LNode *head,*p1,*p2;
	/*head用来标记链表，p1总是用来指向新分配的内存空间，
	p2总是指向尾结点，并通过p2来链入新分配的结点*/
	

	int a;
	head=NULL;
	for(i=1;i<=n;i++)
	{
		p1=(struct LNode *)malloc(sizeof(struct LNode));
		/*动态分配内存空间，并数据转换为(struct LNode)类型*/
		printf("请输入链表中的第%d个数：",i);
		scanf("%d",&a);
		p1->data=a;
		if(head==NULL)/*指定链表的头指针*/
		{
			head=p1;
			p2=p1;
		}
		else
		{
			p2->next=p1;
			p2=p1;
		}

		p2->next=NULL;/*尾结点的后继指针为NULL(空)*/
	}
	
	return head;/*返回链表的头指针*/
}

static UINT8 entrypt_mod_two(UINT8 data)
{
	UINT8 ret; 

	ret = data^0xa5;

	return ret;
}
UINT8 encrypt_data[4];
UINT8 encrypt_read[4];


Node *creatList(int n)
{

	int i;
	Node *head,*p1,*p2;

	head = NULL;
	for(i=0;i<n;i++)
	{
		p1 = (Node *)malloc(sizeof(Node));
		
		scanf("%d",&p1->value);

		if(head == NULL)
		{
			head = p1;
		}
		else
		{
			p2->next = p1;
		}
			p2 = p1;			
	}

	p2->next = NULL;

	return head;
}


Node *creatList(Node *pHead)
{
	
	Node *p1;
    Node *p2;
 
    p1=p2=(Node *)malloc(sizeof(Node)); //申请新节点
    if(p1 == NULL || p2 ==NULL)
    {
        printf("内存分配失败\n");
        exit(0);
    }
    memset(p1,0,sizeof(Node));
 
    scanf("%d",&p1->element);    //输入新节点
    p1->next = NULL;         //新节点的指针置为空
    while(p1->element > 0)        //输入的值大于0则继续，直到输入的值为负
    {
        if(pHead == NULL)       //空表，接入表头
        {
            pHead = p1;
        }
        else               
        {
            p2->next = p1;       //非空表，接入表尾
        }
        p2 = p1;
        p1=(Node *)malloc(sizeof(Node));    //再重申请一个节点
        if(p1 == NULL || p2 ==NULL)
        {
        printf("内存分配失败\n");
        exit(0);
        }
        memset(p1,0,sizeof(Node));
        scanf("%d",&p1->element);
        p1->next = NULL;
    }
    printf("creatList函数执行，链表创建成功\n");
    return pHead;  
}

void printList(Node *pHead)
{

	if(NULL == pHead)
	{
		printf("it's null\n");
	}
	else
	{
		while(pHead != NULL)
		{
			printf("%d ",pHead->value);
			pHead = pHead->next;
		}
	}
}


//--------------------------
typedef struct tagListNode{ 
    int data; 
    struct tagListNode* next; 
}ListNode, *List; 
 
void PrintList(List head); 
List ReverseList(List head); 
 
int main() 
{ 
    //分配链表头结点 
    ListNode *head; 
    head = (ListNode*)malloc(sizeof(ListNode)); 
    head->next = NULL; 
    head->data = -1; 
 
    //将[1,10]加入链表 
    int i; 
    ListNode *p, *q; 
    p = head; 
    for(int i = 1; i <= 10; i++) 
    { 
        q = (ListNode *)malloc(sizeof(ListNode)); 
        q->data = i; 
        q->next = NULL; 
        p->next = q; 
        p = q;         
    } 
 
    PrintList(head);           /*输出原始链表*/ 
    head = ReverseList(head);  /*逆序链表*/ 
    PrintList(head);           /*输出逆序后的链表*/ 
    return 0; 
} 
 
List ReverseList(List head) 
{ 
    if(head->next == NULL || head->next->next == NULL)   
    { 
       return head;   /*链表为空或只有一个元素则直接返回*/ 
    } 
 
    ListNode *t = NULL, 
             *p = head->next, 
             *q = head->next->next; 
    while(q != NULL) 
    {         
      t = q->next; 
      q->next = p; 
      p = q; 
      q = t; 
    } 
 
    /*此时q指向原始链表最后一个元素，也是逆转后的链表的表头元素*/ 
    head->next->next = NULL;  /*设置链表尾*/ 
    head->next = p;           /*调整链表头*/ 
    return head; 
} 

typedef union{
	char i;
	int cnt;
}iduan;

unsigned int checkbig(void)
{
	iduan.cnt = 0x01;

	if(iduan.i == 0x01)
		return 1;
	else
		return 0;
		
}

void* memcpy( void *dst, const void *src, unsigned int len )  
{  
    register char *d;  
    register char *s;  
    if (len == 0)  
        return dst;  
    if ( dst > src )   //考虑覆盖情况  
    {  
        d = (char *)dst + len - 1;  
        s = (char *)src + len - 1;  
        while ( len >= 4 )   //循环展开，提高执行效率  
        {  
            *d-- = *s--;  
            *d-- = *s--;  
            *d-- = *s--;  
            *d-- = *s--;  
            len -= 4;  
        }  
        while ( len-- )   
        {  
            *d-- = *s--;  
        }  
    }   
    else if ( dst < src )   
    {  
        d = (char *)dst;  
        s = (char *)src;  
        while ( len >= 4 )  
        {  
            *d++ = *s++;  
            *d++ = *s++;  
            *d++ = *s++;  
            *d++ = *s++;  
            len -= 4;  
        }  
        while ( len-- )  
        {  
            *d++ = *s++;  
        }  
    }  
    return dst;  
}  

char * search(char *cpSource, char ch)  
{  
         char *cpTemp=NULL, *cpDest=NULL;  
         int iTemp, iCount=0;  
         while(*cpSource)  
         {  
                 if(*cpSource == ch)  
                 {  
                          iTemp = 0;  
                          cpTemp = cpSource;  
                          while(*cpSource == ch)
                          {
                                   ++iTemp;
                                   ++cpSource;  
						  }
                          if(iTemp > iCount)
                          {
                                iCount = iTemp;
                                cpDest = cpTemp;
                          }      
                        if(!*cpSource)   
                            break;  
                 }  
                 ++cpSource;  
     }  
     return cpDest;  
}     

Node *reserveList(Node *head)
{
	if(head->next == NULL || head->next->next == NULL)
	{
			printf("it's ok");
	}

	Node *p = head->next;
	Node *q = head->next->next;
	Node *t = NULL;

	while(q != NULL)
	{
		t = q->next;
		q->next = p;
		p = q;
		q = t;
	}
	head->next = p;
	return head;
}
void PrintList(List head) 
{ 
    ListNode* p = head->next; 
    while(p != NULL) 
    { 
        printf("%d ", p->data); 
        p = p->next; 
    } 
    printf("/n"); 
} 

typedef struct Node{    
    elemType element;
    Node *next;
}Node;
 
Node *inserhead(Node *head,int data)
{
	Node *p1 = (Node *)malloc(sizeof(Node));

		p1->value = data;

		p1->next = head->next;
		head->next = p1;

		return head;
}

Node * insertEnd(Node *head,int data)
{
	Node * pHead = head;

	Node * tmp = pHead;

	Node *p1 = (Node *)malloc(sizeof(Node));

	while(pHead != NULL)
	{
		pHead = pHead->next;
	}

	pHead->next = p1;
	p1->next = NULL;

	head = tmp;


}

#define MAX(A,B) ((A)>(B)?(A):(B))

int findmax(int arr[],int size)
{
	if(size ==1)
		return arr[0];
	if(size ==2)
		return(arr[0],arr[1]);
	return(findmax(arr+1,size-1);	
}

int insertHeadList(Node **pNode,elemType insertElem)
{
    Node *pInsert;
    pInsert = (Node *)malloc(sizeof(Node));
    memset(pInsert,0,sizeof(Node));
    pInsert->element = insertElem;
    pInsert->next = *pNode;
    *pNode = pInsert;
 
    return 1;
}



Node *reserve(Node *head)
{
	if(head == NULL || head->next == NULL)
		return;

	Node *p1,*p2, *t;
	t = NULL;
	p1 = head->next;
	p2 = head->next->next;

	while(p2 != NULL)
	{
		t = p2->next;
		p2->next = p1;
		p1 = p2;
		p2 = t;		
	}
	head->next->next = NULL;
	head->next = p1;
	return head;
}
 Linklist *head_insert(Linklist *head,int value) //头插法，先插的元素排在后面  
{  
    Linklist *p,*t;  
    t=head;  
    p=(Linklist *)malloc(sizeof(Linklist));  
    p->data=value;  
    p->next=t->next;  
    t->next=p;  
    return head;   
}  


int insertLastList(Node **pNode,elemType insertElem)
{
    Node *pInsert;
    Node *pHead;
    Node *pTmp; //定义一个临时链表用来存放第一个节点
 
    pHead = *pNode;
    pTmp = pHead;
    pInsert = (Node *)malloc(sizeof(Node)); //申请一个新节点
    memset(pInsert,0,sizeof(Node));
    pInsert->element = insertElem;
 
    while(pHead->next != NULL)
    {
        pHead = pHead->next;
    }
    pHead->next = pInsert;   //将链表末尾节点的下一结点指向新添加的节点
    *pNode = pTmp;
    printf("insertLastList函数执行，向表尾插入元素成功\n");
 
    return 1;
}

int continumax(char *outputstr, char *inputstr)  
{  
    char *in = inputstr, *out = outputstr, *temp, *final;  
    int count = 0, maxlen = 0;  
    while( *in != '/0' )  
    {  
        if( *in > 47 && *in < 58 )  
        {  
            for(temp = in; *in > 47 && *in < 58 ; in++ )  
            count++;  
        }  
    	else  
    		in++;  

	    if( maxlen < count )  
	    {  
	        maxlen = count;  
	        count = 0;  
	        final = temp;  
	    }  
    }  
    
    for(int i = 0; i < maxlen; i++)  
    {  
        *out = *final;  
        out++;  
        final++;  
    } 
    
    *out = '/0';  
    return maxlen;  
}  


int findList(Node *head, int data)
{
	if(head ->next == NULL)
		return 0;

	Node *pTmp = head;
	
	while(head->next != NULL)
	{
		head = head->next;

		if (head->value == data);
			return TRUE; 
	}
		return FALSE;
}


char *findSubSameStr(char *src, char ch)
{
	int iT,iD = 0;
	
	char *cpT,*cpD;
	while(*src)
	{
		if(*src ==ch)
		{
			iT = 0;
			cpT = src;

			while(*src == ch)
			{
				src++;
				iT++;
			}
			if(iT > iD)
			{
				iT = 0;
				iD = iT;
				cpD = cpT;
			}
		}	
		src++;
	}

	return cpD;
}
#define MAX(A,B)  ((A)>(B)?(A):(B))
int findmax(int arr[], int size)
{
	if(size == 1)
		return arr[0];
	if(size == 2)
		return MAX(arr[0],arr[1]);
		
	return findmax(arr+1,size-1);	
}

int mystrlen(char *str)
{
	
}
// find the sub str in the main str, and return the number
int FindSubStrCnt(char* str,char* s)  
{
	char *s1,*s2;

	int cnt = 0;
	while(*str != '0')
	{
		s1 = str;
		s2 = s;
		while(*s1 == *s2 && *s1 != '0' && *s2 != '0')
		{
			s1++;
			s2++;
		}
		if(s2 == '0')
			++cnt;
		str++;	
	}
	return cnt;
}

int strcmp(char *cmp, char *dr)
{
	assert(cmp!=NULL && dr !=NULL)
	
	while(*cmp == *dr && *cmp != '\0' && dr != '\0')
	{
		cmp++;
		dr++;
	}

	if(*cmp > *dr)
		return 1;
	else if(*cmp == *dr)
		return 0;
	else
		return -1;

}

void reserve(char *str)
{

	char *p = str;
	char *q = str;
	char c;

	while(*++q);
	q -= 1;
		
	while(p>q)
	{
		c = *p;
		*p++ = *q;
		*q-- = c;		
	}	
}


char *strcpy(char *dst,const char*src)
{

	assert(dst != NULL && src != NULL);
		
	char *tmp = dst;

	while(*dst ++ = *src++);

	return tmp;
}

int strlen(const char *src)
{
	const char *tmp;

	for(tmp = src; tmp!= '\0';++tmp);

	return (tmp-src);
}

// find the positon the sub in the str
size_t find(char* s1,char* s2)  
    {  
        size_t i=0;  
         size_t len1 = strlen(s1); 
        size_t len2 = strlen(s2);  
        if(len1-len2<0) return len1;  
        for(;i<len1-len2;i++)  
        {  
            size_t m = i;  
            for(size_t j=0;j<len2;j++)  
            {  
                if(s1[m]!=s2[j])  
                    break;  
                m++;  
            }  
            if(j==len2)  
                break;  
        }  
        return i<len1-len2?i:len1;  
    }


char *mystrstr(char*s1,char*s2)
{
    if(*s1==0)
    {
        if(*s2)
            return (char*)NULL;
        return (char*)s1;
    }
    while(*s1)
    {
        int i=0;
        while(1)
        {
            if(s2[i]==0)
                return s1;
            if(s2[i]!=s1[i])
                break;
            i++;
        }
        s1++;
    }
    return (char*)NULL;
}


int strlen(const char *str)
{
	char *cnt;

	for(cnt = str; cnt != '\0'; ++cnt);

	return (cnt-str);
}

char *mystrstr(char *str1, char *str2)
{
	int len1 = strlen(str1);
	int len2 = strlen(str2);

	if(len1 < len2)
		return;

	int i,j;
	int m;
	for(i=0;i<len1-len2;i++)
	{
			m = i;
			for(j=0;j<len2;j++)
			{
				str1[m] != str2[j];
					break;
				m++;	
			}

			if(str1[j] == '\0')
				break;
	}
	return;
}  


int IsReverseStr(char *str)  
{  
    int i,j;  
    int found=1;  
    if(str==NULL)  
        return -1;  
    char* p = str-1;  
    while(*++p!= '/0');  
    --p;  
    while(*str==*p&&str<p) str++,p--;  
    if(str < p)  
        found = 0;  
    return found;  
} 


int mystrlen(const char *str)
{
	if(*str == '\0')
		return 0;
	else
		return (mystrlen(str+1)+1);
}

UINT8 mcu_encrypt_run(UINT8 mode)
{
	UINT8 ret = 0;
	UINT8 data = 0;
	

	if(1 == mode )
	{

		int i;

		int n = 0;
#if 1		
		/*randomize(); 
		
		for(i=0; i<4; i++) 
		{
			encrypt_data[i] = (rand() % 100);

			printf("encrypt_data[%d] =  %d \n",i,encrypt_data[i]);
		}	
		*/

	    UINT8 t1,t2,t3,K;
		
		for(i=0; i<4; i++) 
		{
				
			t1=sp5kMsTimeGet();
			int y1=t1;
			sp5kTimeDelay(SP5K_TIME_DELAY_1MS, 2);
			t2=sp5kMsTimeGet();
			int y2=t2+y1;
			sp5kTimeDelay(SP5K_TIME_DELAY_1MS, 4);
			t3=sp5kMsTimeGet();
			int y3=t3+y2;//取1/100s作为基数
			K= y1+y2+y3;//产生的随机数0~99*99*99
					
			encrypt_data[i] = K;
			printf("encrypt_data[%d] =  %d \n",i,encrypt_data[i]);
		}	
#endif

#if 1		
		while(1)
		{
			memset(encrypt_read,0,4);
	#if 0
//		   
		    ret = Zhe_Encrypt_i2c_write(ENCRTY_MODE1,encrypt_data);
				    
			for(i = 0; i< 4; i++)
			{
				//			printf("add mcu encrypt 1 -----encrypt_data [%d]-- = 0x%x------------\n",i,encrypt_data[i]);
			}


			I2C_DELAY_LONG(1000);
			I2C_DELAY_LONG(1000);
//			Zhe_Encrypt_i2c_read(ENCRTY_MODE1,encrypt_read);
			
			for(i = 0; i< 4; i++)
			{
		//		printf("add mcu encrypt1  -----encrypt_read [%d]-- = 0x%x------------\n",i,encrypt_read[i]);
			}

			//---------------------------
				//---------------------------
			I2C_DELAY_LONG(1000);
			I2C_DELAY_LONG(1000);

			memset(encrypt_read,0,4);
	#endif
			
			ret = Zhe_Encrypt_i2c_write(ENCRTY_MODE2,encrypt_data);
								
			for(i = 0; i< 4; i++)
			{
		//		printf("add mcu encrypt 2 -----encrypt_data [%d]-- = 0x%x------------\n",i,encrypt_data[i]);
			}


			I2C_DELAY_LONG(1000);
			I2C_DELAY_LONG(1000);

			Zhe_Encrypt_i2c_read(ENCRTY_MODE2,encrypt_read);
			
			for(i = 0; i< 4; i++)
			{
		//		printf("add mcu encrypt2  -----encrypt_read [%d]-- = 0x%x------------\n",i,encrypt_read[i]);
			}
			
			I2C_DELAY_LONG(1000);
			I2C_DELAY_LONG(1000);
		
			ret =  entrypt_mod_two(encrypt_data[2]);
			
//			printf("entrypt_mod_two    ret = %x\n",ret);
			
			if(ret != encrypt_read[2])
			{
				n++;
				printf("error  entrypt mode two  xxxxxxxxxxxxxxxxxxxxx \n");
				if(n == 3)
				{
					n = 0;
					return FALSE;
				}
				
				continue;
			}	
			
			printf("entrypt is ok kkkkkkkkkkkkkkkkkkkkkk \n");
#if 0				//--------------------------

			ret = Zhe_Encrypt_i2c_write(ENCRTY_MODE2,encrypt_data);									
			for(i = 0; i< 4; i++)
			{
				printf("add mcu encrypt 2 -----encrypt_data [%d]-- = 0x%x------------\n",i,encrypt_data[i]);
			}

			I2C_DELAY_LONG(10000);
			I2C_DELAY_LONG(10000);
			Zhe_Encrypt_i2c_read(ENCRTY_MODE3,encrypt_read);
			
			for(i = 0; i< 4; i++)
			{
				printf("add mcu encrypt3  -----encrypt_read [%d]-- = 0x%x------------\n",i,encrypt_read[i]);
			}


#endif
			break;	
		}
#endif
		return TRUE;
	}
	else
	{
		return TRUE;
	}


return TRUE;
}

void *memcpy(void *dst, void *src, const unsigned int len)
{
	if(len <= 0)
		return dst;

	char *d; 
	char *s;
	
	if(dst > src)
	{
		d = (char *)dst+len;
		s = (char *)src+len;

		while(len--)
		{
			*d-- = *s--
		}
	}
	else
	{
		d =(char *)dst;
		s =(char *)src;

		while(len--)
		{
			*d++ = *s++;
		}
	}
	
}

int IsReserveStr(char *str)
{
	assert(str != NULL);

	char *p = str-1;

	while(*++p);
	--p;

	while(*str==*p&&str<p)
	{
		str++;
		p--;
	}

	if(str<p)
		return FALSE;
	return 	TRUE;	
}



