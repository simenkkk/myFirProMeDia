/**************************************************************************
 *
 *       Copyright (c) 2012 by iCatch Technology, Inc.
 *
 *  This software is copyrighted by and is the property of iCatch Technology,
 *  Inc.. All rights are reserved by iCatch Technology, Inc..
 *  This software may only be used in accordance with the corresponding
 *  license agreement. Any unauthorized use, duplication, distribution,
 *  or disclosure of this software is expressly forbidden.
 *
 *  This Copyright notice MUST not be removed or modified without prior
 *  written consent of iCatch Technology, Inc..
 *
 *  iCatch Technology, Inc. reserves the right to modify this software
 *  without notice.
 *
 *  iCatch Technology, Inc.
 *  19-1, Innovation First Road, Science-Based Industrial Park,
 *  Hsin-Chu, Taiwan, R.O.C.
 *
 *  Author:
 *
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ndk_types.h>
#include <ndk_global_api.h>
#include <ndk_getopt.h>
#include <ndk_streaming.h>
#include <ctype.h>
#include <api/cmd.h>
#include <sp5k_dbg_api.h>
#include <sp5k_global_api.h>
#include <api/sp5k_fs_api.h>
#include <sp5k_pb_api.h>
#include <proflog.h>
#include <app_timer.h>
#include <app_wifi_utility.h>
#include <app_osd_api.h>
#include <gpio_custom.h>
#include "sp5k_media_api.h"
#include "app_view_param.h"
#include "app_view_param_def.h"
#include "sp5k_modesw_api.h"
#include "app_still.h"
#include "app_video.h"
#include "app_ui_para.h"
#include <app_zoom_api.h>
#include "app_cdfs_api.h"
#include "sp5k_utility_api.h"

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
UINT8 gssidname[MAX_SSID_NAME_LEN]={0};
UINT8 gpassword[MAX_PASSWORD_LEN]={0};
static UINT8 ghapMode = 0;
static UINT8 g_init_state = 0;
/*static UINT32 ndktimer = TIMER_NULL;*/
extern void ndk_msleep(ULONG millisecs);
extern void usbPIMADriveNodeFlushAll(UINT32 firstFile);/*xuan.ruan@20131113 add for mantis bug 0048877*/

#ifdef ICAT_STREAMING
static NetStParams st_params;
static BOOL bPtpStatus = 0;
static BOOL bRtpStatus = 0;
#endif

static void appCustMsrcInit(void);

/**************************************************************************
 *                         F U N C T I O N     D E F I N I T I O N                         *
 **************************************************************************/
extern void callStack(void);
static void appmediasrcPBVideo(void);
static SP5K_EVENT_FLAGS_GROUP gnet_sysevt = 0;
static unsigned char gsta_hwaddr[6];
UINT32 isStaConnected = -1;

#ifdef WIFISTA
UINT32 appStaModeRelinkInit(void);
UINT32 appStaModeRelinkDestory(void);
#define OPT_ARRAY_SIZE(a)       (sizeof(a)/sizeof((a)[0]))

void app_station_mode(void);
static void appSTAServerStop(void);
#endif


struct udp_pcb {

/* Common members of all PCB types */

  IP_PCB;
/* Protocol specific PCB members */
  struct udp_pcb *next;
  u8_t flags;
  /** ports are in host byte order */
  u16_t local_port, remote_port;
#if LWIP_IGMP
  /** outgoing network interface for multicast packets */
  ip_addr_t multicast_ip;
#endif /* LWIP_IGMP */

#if LWIP_UDPLITE

  /** used for UDP_LITE only */

  u16_t chksum_len_rx, chksum_len_tx;

#endif /* LWIP_UDPLITE */

  /** receive callback function */

  udp_recv_fn recv;

  /** user-supplied argument for the recv callback */
  void *recv_arg; 

};



int appNdkNetDrvInit(
	const char* drvname
)
{
	if (!drvname) {
		printf("Need driver name\n");
		return -1;
	}
	else if (!strcmp(drvname, "rtk412") || !strcmp(drvname, "rtk") || !strcmp(drvname, "rtk413")) {
		printf("Loading RTK driver\n");
		return ndk_netdrv_init(drvname)/*wdrv_init_rtk_v412()*/;/*!< use Realtek WiFi 413 */
	}
	else {
		printf("Unknow driver\n");
		return -1;
	}
}

void ptpipConnectcb(UINT32 sessnum)
{
	printf("[PTP-IP] session num:%d\n",sessnum);
	profLogPrintf(0,"[PTP-IP]session num:%d",sessnum);
	if(!sessnum){

	}else{
		usbPIMADriveNodeFlushAll(1);/*xuan.ruan@20131113 add for mantis bug 0048877*/
	}
}

void appRtpUpdateMediaAttrs(
	NetStParams *sp,UINT8 vidsize
)
{
	sp->jpeg_width = ALIGN_TO(sp->jpeg_width, 8);
	if (sp->jpeg_width > 640) {
		sp->jpeg_width = 640;
	}

	sp->jpeg_height = ALIGN_TO(sp->jpeg_height, 8);
	if (sp->jpeg_height > 360) {
		sp->jpeg_height = 360;
	}
	
	#if 0
	if (sp->jpeg_q_factor > RTP_MJPG_Q_FINE)
		sp->jpeg_q_factor = RTP_MJPG_Q_FINE;
	else if (sp->jpeg_q_factor < RTP_MJPG_Q_ECONOMY)
		sp->jpeg_q_factor = RTP_MJPG_Q_ECONOMY;
	#else /* MJPG quality issue */
	sp->jpeg_q_factor = 80;
	#endif

	if (sp->jpeg_bitrate > RTP_MJPG_BR_FINE)
		sp->jpeg_bitrate = RTP_MJPG_BR_FINE;
	else if (sp->jpeg_bitrate < RTP_MJPG_BR_ECONOMY)
		sp->jpeg_bitrate = RTP_MJPG_BR_ECONOMY;

	appRtpResetMediaAttrs();

	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_STAMP_OPTION, 0);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_VIDEO_BRC_TYPE, SP5K_MEDIA_CBR);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_MUTE_PERIOD, 100);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_FILE_TYPE, sp->file_type);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_CODEC, SP5K_MEDIA_AUDIO_PCM);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_SAMPLE_RATE, 44100);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_SAMPLE_BITS, 16);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_CHANNELS, 2);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_ALC_MAX_VOL, 31);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_ALC_MUTE, 0);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_ALC, SP5K_MEDIA_REC_OFF);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_ALC_HB, 500);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_AUDIO_ALC_LB, 100);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_SEAMLESS_TIME_SLOT, 60);//baiqi set seamless 60s(1min)
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_ALC, SP5K_MEDIA_REC_OFF);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_ALC_MODE, SP5K_MEDIA_REC_ALC_DRC_MODE);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_ALC, SP5K_MEDIA_REC_ON);

	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_WIDTH, sp->h264_width);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_HEIGHT, sp->h264_height);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_VIDEO_AVG_BITRATE, sp->h264_bitrate);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_VIDEO_FRAME_RATE, sp->h264_frmrate);
	sp5kMediaRecAttrSet(MEDIA_ATTR_H264_GOP_STRUCTURE, 0x10);
	sp5kMediaRecAttrSet(SP5K_MEDIA_ATTR_VIDEO_CODEC, SP5K_MEDIA_VIDEO_H264);

	if(vidsize == UI_VID_SIZE_FHD)
	{
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_SIZE, (2 * 1024 * 1024) );
  		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_CNT, 12);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_FIFO_BUF_NO ,192);
	    sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_FIFO_BUF_NO ,192);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_MSGQ_NO, 100);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_MSGQ_NO, 64);
		sp5kAwbCfgSet( SP5K_AWB_ACCUM_PERIOD, 2);
		sp5kAeCfgSet(SP5K_AE_ACCUM_PERIOD, 1);
		sp5kSensorModeCfgSet(SP5K_MODE_VIDEO_PREVIEW, SENSOR_MODE_1080P);
		appAeFrameRateSet(30);
	}else if(vidsize==UI_VID_SIZE_HD_60FPS){
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_SIZE, (1300 * 1024) );
  		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_CNT, 27);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_FIFO_BUF_NO ,256);
	    sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_FIFO_BUF_NO ,256);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_MSGQ_NO, 100);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_MSGQ_NO, 64);
		sp5kAwbCfgSet( SP5K_AWB_ACCUM_PERIOD, 2 );
		sp5kAeCfgSet(SP5K_AE_ACCUM_PERIOD, 2);
		sp5kSensorModeCfgSet(SP5K_MODE_VIDEO_PREVIEW, SENSOR_MODE_720P_60FPS);
		appAeFrameRateSet(60);
	}
	else if(vidsize == UI_VID_SIZE_HD_30FPS)
	{
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_SIZE, (1300 * 1024) );
  		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_CNT, 27);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_FIFO_BUF_NO ,256);
	    sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_FIFO_BUF_NO ,256);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_MSGQ_NO, 100);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_MSGQ_NO, 64);
		sp5kAwbCfgSet( SP5K_AWB_ACCUM_PERIOD, 2 );
		sp5kAeCfgSet(SP5K_AE_ACCUM_PERIOD, 1);
		sp5kSensorModeCfgSet(SP5K_MODE_VIDEO_PREVIEW, SENSOR_MODE_720P_30FPS);
		appAeFrameRateSet(30);
	}
	else
	{
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_SIZE, (600 * 1024) );
  		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VLC_BUF_CNT, 50);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_FIFO_BUF_NO ,256);
	    sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_FIFO_BUF_NO ,256);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_VIDEO_MSGQ_NO, 100);
		sp5kMediaRecCfgSet(SP5K_MEDIA_REC_AUDIO_MSGQ_NO, 64);
		sp5kAwbCfgSet( SP5K_AWB_ACCUM_PERIOD, 4 );
		sp5kAeCfgSet(SP5K_AE_ACCUM_PERIOD, 4);
		sp5kSensorModeCfgSet(SP5K_MODE_VIDEO_PREVIEW, SENSOR_MODE_VGA_120FPS);
		appAeFrameRateSet(120);
	}
	sp5kModeSet(SP5K_MODE_VIDEO_PREVIEW);
	sp5kModeWait(SP5K_MODE_VIDEO_PREVIEW);
	if (0) {
		UINT32 w, h;
		sp5kMediaRecAttrGet(SP5K_MEDIA_ATTR_WIDTH, &w);
		sp5kMediaRecAttrGet(SP5K_MEDIA_ATTR_HEIGHT, &h);
		MYINFO("width = %d, height = %d\n", w, h);
	}
}

void appRtpResetParams(
	NetStParams *sp,UINT8 vidsize
)
{
	UINT32 w,h;

	appVideoSizeGet(vidsize, &w, &h);
	
	sp->file_type     = SP5K_MEDIA_MOV;
	sp->h264_width   = w;
	sp->h264_height  = h;

	switch(vidsize)
	{
	 	case UI_VID_SIZE_FHD:
			sp->h264_bitrate  = 12000000;
			sp->h264_frmrate  = 30;
			break;
		case	UI_VID_SIZE_HD_60FPS:
			sp->h264_bitrate  = 10000000;
			sp->h264_frmrate  = 60;
			break;
		case	UI_VID_SIZE_HD_30FPS:
			sp->h264_bitrate  = 8000000;
			sp->h264_frmrate  = 30;
			break;			
		case	UI_VID_SIZE_VGA:
			sp->h264_bitrate  = 6000000;
			sp->h264_frmrate  = 120;
			break;
		default:
			sp->h264_width    = 1920;
			sp->h264_height   = 1080;
			sp->h264_bitrate  = 12000000;
			sp->h264_frmrate  = 30;
			break;
	}

	sp->jpeg_width    = 640;
	sp->jpeg_height   = 360;
	#if 0
	sp->jpeg_q_factor = 40;
	#else /* MJPG quality issue */
	sp->jpeg_q_factor = 80;
	#endif	
	sp->jpeg_bitrate  = 4500000;
	
	printf("[RTP][ParamReset]H(%d,%d,%d,%d)M(%d,%d,%d,%d)\n",
		sp->h264_width ,sp->h264_height,sp->h264_bitrate,sp->h264_frmrate,
		sp->jpeg_width,sp->jpeg_height ,sp->jpeg_q_factor,sp->jpeg_bitrate);	
}

#ifdef MY_WIFI_TEST

int a[5] = {0x25,0x65,0x32,0x21};

typedef Node *node_t;

typedef struct node
{
	int value;

	struct node *next;	
}Node;


Node *pHead = NULL;

void creatList(int n)
{
	Node *p,*q;
	head = NULL

	int i;
	
	for(i=0; i<n; i++)
	{
		p = (Node *)malloc(sizeof(Node));

		if(p == NULL)
		{
			printf("malloc error\n");
		}

		if(pHead == NULL)
		{
			pHead = p;
		}
		else
		{
			q ->next = p;
		}

		q = p;
	}
	p->next = NULL;

	return;
}

Node insertHeadList(Node *head, int val)
{
	Node *p = (Node *)malloc(sizeof(Node));
	
	if (p == NULL)
	{
		printf("malloc error\n");

		return;
	}
	
	p ->value = val;
	
	if (head = NULL)
	{
		head = p;
		
		p->next = NULL;
	}
	else
	{
		p->next = head->next;

		head->next = p;
	}

	return head;
		
}

Node *inserEndList(Node *head, int val)
{
	Node *pHead = head;

	Node *tmp = pHead;

	Node *p = (Node *)malloc(sizeof(Node));
	if(p == NULL)
	{
		printf("malloc error\n");
	}
	
	while(pHead != NULL)
	{
		pHead = pHead ->next;		
	}

	pHead->next = p;
	p->next = NULL
	
	pHead = tmp;

	return pHead;
}


Node *reserveList(Node *head)
{
	if(head == NULL || head ->next == NULL)
		return head;

	Node *p,*q,*t;

	p = head->next;
	q = head->next->next;
	t = NULL;

	Node *tmp = p;
	
	while(q! = NULL)
	{
		t = q->next;
		q->next = p;
		p = q;
		q = t;		
	}

	head->next = p;
	tmp->next = NULL;

	return head;	
}

Node *findList(Node *head, int val)
{
	Node *pHead = head;

	while(pHead != NULL)
	{
	
		if(pHead ->value == val)
		{
			return pHead;
		}
		pHead = ->pHead->next;
	}

	return;	
}

void delList(Node *head)
{	
	if(head == NULL)
		return;

	Node *pHead;

	while(head != NULL)
	{
		head = head->next;

		pHead = head;
		
		free(pHead);

		head = pHead;
	}

	return;
}

void priList(Node *head)
{
	if(head == NULL)
		return;

	Node *pHead = head;
	
	while(pHead != NULL)
	{
		printf("pHead ->value = %d\n",pHead->value);

		pHead = pHead->next;
	}

	return;
}

unsigned int strlen(char *str)
{
	char *tmp;

	for(tmp = str; str != '\0',str++)

	return (str-tmp);
}

char *strcpy(char *dst, const char *src)
{
	char *tmp = dst;

	assert((dst != NULL) && (src != NULL));

	while((*dst++ = *src++)!= '\0');

	return tmp;
}

void reserve(char *str)
{
	char *p = str;
	char *q = str -1;

	int tmp;
	
	while(*++q);
	--q;
		
	while(p<q)
	{
		tmp = *p;
		*p++ = *q;
		*q-- = tmp;		
	}	

	return;
}

char *strcat(char *dst,char *src)
{
	char tmp =dst;

	while(*src)
		src++;

	while(*src++ = *dst++);

	return src;
	
}


int InsertBeforeNode(Node *node, Node *insert)
 {
     if (!node || !insert) {
         return -1;
     }
     
     Node temp = *node;
     node->data = insert->data;
     node->next= insert;
     *insert = temp;
     
     return 0;    
 }

int IsLoopList(Node *head)
{
	Node *p =head;
	Node *q = head;

	while(p!= NULL && q!= NULL)
	{
		p = p->next;
		q = q->next;	
		if(q!= NULL)
			q=q->next;
		if( p!=NULL && p == q)
			return 1;
	}
	return 0;
}

void setbit(int a,int x)
{
	a |= (x<<0x01);
}

void clrbit(int a,int x)
{
	a &= ~(x<<0x01);		
}
 
int has_loop2(node_t head)
{
    node_t p = head;
    node_t q = head;
    while (p != NULL && q != NULL)
    {
        /*
        p = p->next;
        if (q->next != NULL)
            q = q->next->next;
        if (p == q)
            return 1;
            */
        //correct it on 17/11/2012
        p = p->next;
        q = q->next;
        if (q != NULL)
            q = q->next;
        if (p != NULL && p == q)
            return 1;
    }
    return 0;
}



typedef struct StackNode
 {
    int data;
    StackNode* next;
 }StackNode,*LinkStackPtr;

 typedef struct LinkStack
 {
    LinkStackPtr top;
    int count;
 }LinkStack;


void InitStack(LinkStack* S)
{
//	   LinkStackPtr s=(LinkStackPtr)malloc(sizeof(StackNode));
//	   s->data=0;
//	   s->next=NULL;
//	   S->top=s;
//	   S->count=0;
   S->top=NULL;
   S->count=0;
}

bool Push(LinkStack* S,int e)
{
   LinkStackPtr s=(LinkStackPtr)malloc(sizeof(StackNode));
   s->data=e;
   s->next=S->top;
   S->top=s;
   S->count++;
   return true;
}

bool Pop(LinkStack* S,int *e)
{
   LinkStackPtr p;
   if (S->count==0)
   {
	   return false;
   }

   *e=S->top->data;
   p=S->top;
   S->top=S->top->next;
   free(p);
   S->count--;
   return true;
}

#endif
static BOOL bIsRtpOptAllow = TRUE;

void appRtpStartStreaming(
	void
)
{
	NetStParams *sp = &st_params;
	uiPara_t* puiPara = appUiParaGet();

	UINT32 curMode;

	if(!bIsRtpOptAllow)
	{
		printf("%s,re-entry !!\n",__FUNCTION__);
		callStack();
		return;
	}

	printf(">>> appRtpStartStreaming\n");

	/*bIsRtpOptAllow = FALSE;*/
	
	sp5kModeGet(&curMode);

	profLogPrintf(0, "[CapPerform]appRtpStartStreaming +");

#if 0 /* for Android Camera */
	appDispLayer_IMG_Ctrl(0);
	appDispLayer_OSD_Ctrl(0);
#endif
	if(!app_PTP_ContRecMode_Get())
		HOST_ASSERT(curMode != SP5K_MODE_VIDEO_RECORD);

	printf("PTPContRecMode:%d\n",app_PTP_ContRecMode_Get());
	profLogPrintf(1,"PTPContRecMode:%d",app_PTP_ContRecMode_Get());
	printf("cur mode:0x%x\n",curMode);

	appRtpStreamStateSet(1); /* 4/11 apk won't apply op mode on entering, then the streaming state is still OFF */
	
	if(app_PTP_ContRecMode_Get()&&curMode==SP5K_MODE_VIDEO_RECORD){
		printf("DSC is doing Video Recording now!\n");		
		profLogAdd(1,"DSC is doing Video Recording now!");
	}
	else{
		if (curMode != SP5K_MODE_VIDEO_PREVIEW) {
			appStateChange(APP_STATE_VIDEO_PREVIEW,  STATE_PARAM_NORMAL_INIT);
			sp5kMediaRecCfgSet(SP5K_MEDIA_REC_CAPTURE_FRAMES, 0);
			sp5kMediaRecCfgSet(SP5K_MEDIA_REC_DIS_LEVEL, 0);
		}

		#if FIX_SENSOR_MODE_4_CAM_MODE
		/* 47282 / 47614 : a fixed sensor mode for capture mode : s */
		if(app_PTP_Get_DscOpMode(MODE_ACTIVE)== PTP_DSCOP_MODE_CAMERA){
			appMediaAttrUpdate(UI_VID_SIZE_FHD);
		}	
		else
		#endif /* 47282 / 47614 : a fixed sensor mode for capture mode : e */
		{
			appMediaAttrUpdate(puiPara->VideoSize);
		}
	}

	appRtpResetParams(sp,puiPara->VideoSize); /* mjpg attr will be reset by appUrlParseJpegAttrs(), so reset again here !! */
	profLogPrintf(0,"mjpg(%d,%d,%d,%d)",sp->jpeg_width,sp->jpeg_height,sp->jpeg_bitrate,sp->jpeg_q_factor);
	if (ndk_st_dualstream_start_brc(sp->jpeg_width, sp->jpeg_height, sp->jpeg_q_factor,30, sp->jpeg_bitrate) != 0) {

	/*if (ndk_st_dualstream_start(sp->jpeg_width, sp->jpeg_height, sp->jpeg_q_factor, 30) != 0) {*/
		profLogPrintf(0, "[CapPerform]appRtpStartStreaming error");
		return;
	}
	profLogPrintf(0, "[CapPerform]appRtpStartStreaming -");
	/*bIsRtpOptAllow = TRUE;*/
}

void appRtpStopStreaming(
	void
)
{
	UINT32 curMode;

	if(!bIsRtpOptAllow)
	{
		printf("%s,re-entry !!\n",__FUNCTION__);
		callStack();
		return;
	}

	printf("<<< appRtpStopStreaming\n");

	/*bIsRtpOptAllow = FALSE;*/
	
	profLogPrintf(0, "[CapPerform]appRtpStopStreaming +");
	sp5kModeGet(&curMode);

	while(curMode <  SP5K_MODE_VIDEO_PREVIEW)
	{
		sp5kModeGet(&curMode);
		appModeSet(SP5K_MODE_VIDEO_PREVIEW);
		printf("\n Wait for video preview mode ready!!\n");
	}
		
	HOST_ASSERT(curMode == SP5K_MODE_VIDEO_PREVIEW || curMode == SP5K_MODE_VIDEO_RECORD);

	#if 0 /* for Android Camera */
	appDispLayer_IMG_Ctrl(1);
	appDispLayer_OSD_Ctrl(1);
	#endif

	if (curMode == SP5K_MODE_VIDEO_RECORD) {
		// Stop record first
		printf("PTPContRecMode:%d\n",app_PTP_ContRecMode_Get());
		profLogPrintf(1,"PTPContRecMode:%d",app_PTP_ContRecMode_Get());
		if(app_PTP_ContRecMode_Get()) 
		{
			printf("___Keeping in video record mode___!!\n");			
			/*appVideoOsdShow(1,1,gVideoCB.osd);*/
		}
		else
		{
			printf("Video Record is Stopping!!\n");
			appVideoStop();
		}
	}

	ndk_st_dualstream_stop();

	profLogPrintf(0, "[CapPerform]appRtpStopStreaming -");
	/*bIsRtpOptAllow = TRUE;*/
}

void appInitStateSet(UINT8 state)
{
	g_init_state |= state;
}

void appInitStateUnset(UINT8 state)
{
	g_init_state &= ~state;
}

void appNetStateReset(UINT8 wifiParm)
{
	if(appPtpStatGet()&&((wifiParm & PTP_IP)== PTP_IP))
		appPtpStateSet(0);
	if(appRtpStreamStateGet()&&((wifiParm & RTP_STREM)== RTP_STREM))
		appRtpStreamStateSet(0);
	/*appTimerClear(&ndktimer);*/
}

UINT8 appInitStateGet()
{
	return g_init_state;
}

static void appRtpResetMediaAttrs(
	void
)
{
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_RTP_STREAMING_EN, 0);
	sp5kMediaRecCfgSet(SP5K_MEDIA_REC_DISABLE_STORAGE, 0);
}

static void appVideoSizeReset(
	NetStParams *stparam,
	UINT32 w,
	UINT32 h
)
{
	uiPara_t* puiPara = appUiParaGet();
	
	if(puiPara->VideoSize== UI_VID_SIZE_FHD){
		stparam->h264_width   = w;
		stparam->h264_height  = h;
		stparam->h264_bitrate = 12000000;
		stparam->h264_frmrate = 30;
	}else if(puiPara->VideoSize == UI_VID_SIZE_HD_60FPS){
		stparam->h264_width	  = w;
		stparam->h264_height  = h;
		stparam->h264_bitrate = 10000000;
		stparam->h264_frmrate = 60;
	}else if(puiPara->VideoSize == UI_VID_SIZE_HD_30FPS){
		stparam->h264_width	  = w;
		stparam->h264_height  = h;
		stparam->h264_bitrate = 8000000;
		stparam->h264_frmrate = 30;
	}
	else{
		stparam->h264_width	  = w;
		stparam->h264_height  = h;
		stparam->h264_bitrate = 6000000;
		stparam->h264_frmrate = 120;
	}
	stparam->jpeg_height  = 360;
}

static BOOL appUrlStrNoCaseCmp(
	const char *src,
	const char *dst,
	size_t len
)
{
	while (*src && *dst && len > 0) {
		if (toupper(*src) != toupper(*dst)) {
			return FALSE;
		}

		++src;
		++dst;
		--len;
	}

	return len == 0 ? 1 : 0;
}



int Fbi(int i)
{
    if (i<2)
    {
        return i==0 ? 0:1;
    }
    return Fbi(i-1)+Fbi(i-2);
}
 


//循环队列
typedef int QElemType;	//前端删除，后端插入

typedef struct{
   QElemType data[100];
   int front;
   int rear;
}SqQueue;

BOOL InitQueue(SqQueue* Q)
{
   Q->rear=0;
   Q->front=0;
   return TRUE;
}

int QueueLength(SqQueue Q)
{
   return (Q.rear-Q.front+100)%100;
}

BOOL EnQueue(SqQueue* Q,QElemType e)
{
   if ((Q->rear+1)%100==Q->front)//队列满判断
   {
	   return FALSE;
   }

   Q->data[Q->rear]=e;
   Q->rear=(Q->rear+1)%100;//移到下一位置

   return TRUE;
}

BOOL DeQueue(SqQueue* Q,QElemType *e)
{
   if (Q->front==Q->rear)//队列空判断
   {
	   return FALSE;
   }

   *e=Q->data[Q->front];
   Q->front=(Q->front+1)%100;

   return TRUE;
}



static BOOL appUrlGetNextElem(
	char **url_,
	char leading_char,
	char **name,
	int *len
)
{
	char *url = *url_;
	if (*url == 0) {
		return FALSE;
	}

	if ((leading_char && *url != leading_char) || !strchr("/?=&", *url)) {
		return FALSE;
	}

	char *p = url + 1;
	while (*p) {
		if (strchr("/?=&", *p)) {
			break;
		}
		++p;
	}
	*name = url + 1;
	*len = (int)(p - url - 1);
	*url_ = p;

	return TRUE;
}

static BOOL appUrlParseAttrs(struct UrlAttrDef *urlAttrs, char *url
	, BOOL (*urlAttrHandler)(int id, char* val))
{
	if (!url || *url == 0)
		return TRUE;

	if (*url != '?') // must start with ?
		return FALSE;

	struct UrlAttrDef *pAttr;
	char *str;
	int  len;

	while (1) {
		if (!appUrlGetNextElem(&url, 0, &str, &len))
			return FALSE;

		for (pAttr = urlAttrs; pAttr->name != NULL; ++pAttr) {
			if (appUrlStrNoCaseCmp(pAttr->name, str, len)) {
				if (!appUrlGetNextElem(&url, '=', &str, &len))
					return FALSE;
				char c = str[len];
				str[len] = 0;
				BOOL r = urlAttrHandler(pAttr->id, str);
				str[len] = c;
				if (!r)
					return FALSE;
				break;
			}
		}

		if (pAttr->name == NULL) // Unknow attributes
			return FALSE;

		if (*url == 0) // finished
			break;

		if (*url != '&') // name=value pair must start with character '&' except the first one.
			return FALSE;
	}

	return TRUE;
}

static BOOL appUrlMjpgAttrHandler(
	int id,
	char* val
)
{
	NetStParams *sp = &st_params;

	switch (id) {
	case URL_MJPG_WIDTH:
		sp->jpeg_width = strtoul(val, NULL, 10);
		break;

	case URL_MJPG_HEIGHT:
		sp->jpeg_height = strtoul(val, NULL, 10);
		break;

	case URL_MJPG_Q_FACTOR:
		if (!strcasecmp(val, "FINE"))
			sp->jpeg_q_factor = RTP_MJPG_Q_FINE;
		else if (!strcasecmp(val, "NORMAL"))
			sp->jpeg_q_factor = RTP_MJPG_Q_NORMAL;
		else if (!strcasecmp(val, "ECONOMY"))
			sp->jpeg_q_factor = RTP_MJPG_Q_ECONOMY;
		else
			sp->jpeg_q_factor = strtoul(val, NULL, 10);
		break;
	case URL_MJPG_BITRATE:
		if (!strcasecmp(val, "FINE"))
			sp->jpeg_bitrate = RTP_MJPG_BR_FINE;
		else if (!strcasecmp(val, "NORMAL"))
			sp->jpeg_bitrate = RTP_MJPG_BR_NORMAL;
		else if (!strcasecmp(val, "ECONOMY"))
			sp->jpeg_bitrate = RTP_MJPG_BR_ECONOMY;
		else
			sp->jpeg_bitrate = strtoul(val, NULL, 10);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

static BOOL appUrlParseJpegAttrs(
	char *url
)
{
	HOST_ASSERT(url);

	NetStParams params_bak;
	memcpy(&params_bak, &st_params, sizeof(params_bak));

	if (!appUrlParseAttrs(mjpgUrlAttrs, url, appUrlMjpgAttrHandler))
		return FALSE;

	if (memcmp(&st_params, &params_bak, sizeof(params_bak)) != 0) {
		// changed
	}

	return TRUE;
}

static BOOL appRtpEventHandler(
	UINT32 event,
	UINT32 data
)
{

	switch (event) {
	case NDK_ST_EVT_RTSP_REQUEST: {
		char *url = (char *)data;
		MYINFO("URL = '%s'\n", url);

		if (appUrlStrNoCaseCmp(url, "MJPG", 4)) {
			return appUrlParseJpegAttrs(url + 4);
		}
		else {
			MYINFO("Stream not found\n");
			return TRUE;
		}
		break; }

	case NDK_ST_EVT_ON_STOPPED: {
		profLogPrintf(0,"[CapPerform]NDK_ST_EVT_ON_STOPPED mode=%d", ndk_st_get_mode());
		sp5kHostMsgSend(APP_UI_MSG_RTP_STOP,0);
		return TRUE; }

	case NDK_ST_EVT_ON_STARTED: {
		
		if(appPreviousStateGet()==APP_STATE_STILL_AAA){
			do{
				appTimeDelay(10, 1);
				sp5kModeGet(&currentMode);
				if(currentMode == SP5K_MODE_STILL_PREVIEW){
					modeSwDone = TRUE;
				}
			}while(modeSwDone == FALSE);
		}
		profLogPrintf(0,"[CapPerform]NDK_ST_EVT_ON_STARTED mode=%d", ndk_st_get_mode());
		sp5kHostMsgSend(APP_UI_MSG_RTP_START,0);
		return TRUE;}

	case NDK_ST_EVT_FRAME_DROPPED: {
		profLogPrintf(0, "drop frame %u", data);
		return TRUE; }
	}

	return FALSE;
}

BOOL appWiFiInit(
	BOOL bMptoolEn,
	const char *drvname
)
{
	/* Control MPtool behavior */
	#if NDK_MP_TOOL_EN
	ndk_netdrv_mpset(0); /* NOTE: NDK rev must be latter than 15995 */
	#endif
	if (appNdkNetDrvInit(drvname) != 0) { /* Loading Realtek WiFi driver */
		HOST_ASSERT(0);
		return -1;
	}

	
	ndk_netif_ioctl(NDK_IOCS_IF_UP, (long)"wlan0", NULL);
	if(ndk_netif_set_address_s("wlan0", "192.168.0.1", "255.255.255.0", "192.168.0.1")!=0){
		printf("Net address set failed!\n");
		return -1;
	}

	ndk_netif_ioctl(NDK_IOCS_WIFI_COUNTRY_US, (long)"wlan0", NULL);

	printf("Loading WiFi module succeed\n");
	appInitStateSet(WIFI_LOAD);
	return 0; /* succeed */
}

static BOOL appWiFiSuspend(
	void
)
{
	ndk_netif_ioctl(NDK_IOCS_WIFI_SUSPEND, (long)"wlan0", NULL);
	return 0;
}

static BOOL appWiFiResume(
	void
)
{
	ndk_netif_ioctl(NDK_IOCS_WIFI_RESUME, (long)"wlan0", NULL);
	return 0;
}

static void appDhcpServerStart(
	void
)
{
	struct in_addr local, start, netmask;
	//printf("DHCP server start....\n");
	inet_aton("192.168.1.1", &local);
	inet_aton("192.168.1.10", &start);
	inet_aton("255.255.255.0", &netmask);

	ndk_netif_set_address("wlan0", &local, &netmask, &local);
	ndk_dhcpd_start("wlan0", &start, 10);
	appInitStateSet(DHCP_SERVER);
}

static void appDhcpServerStop(
	void
)
{
	UINT8 state = appInitStateGet();

	if (state & DHCP_SERVER) {
		ndk_dhcpd_stop();
		appInitStateUnset(DHCP_SERVER);
	}
}

static void appHostAPStart(
	const char* cfg_file,
	const char* ssid_name,
	const char* passphrase,
	const char* channel_scan
)
{
	printf("Host AP start(%s,%s)\n",cfg_file,ssid_name);
	if(passphrase)
		printf("pwd:%s\n",passphrase);
	NDKHapdOpt opts[] = {
		{"ssid", ssid_name},
		{"wpa_passphrase", passphrase},
		{"channel", channel_scan},
		{"wpa", "2"},
		{NULL, NULL}
	};
	ndk_hapd_start(cfg_file, opts);
	appInitStateSet(HOST_AP);
}

static void appHostAPStop(
	void
)
{
	UINT8 state;

	state = appInitStateGet();

	if (state & HOST_AP) {
		ndk_hapd_stop();
		appInitStateUnset(HOST_AP);
	}
}

static void appPtpIpStart(
	void
)
{
	extern void ptpip_reg_n_start(void);
	extern void ptpip_timeout_set(UINT32 timeout_sec);
	extern void ptpip_connection_state_cb_reg(void (*cb)(UINT32));
	printf("ptp-ip start....\n");
	ptpip_timeout_set(PTPIP_ALIVE_TIME);
	ptpip_reg_n_start();
	appPtpStateSet(1);
	ptpip_connection_state_cb_reg(ptpipConnectcb);
	appInitStateSet(PTP_IP);
}

static void appPtpIpStop(
	void
)
{
	UINT8 state = appInitStateGet();

	extern void ptpip_unreg_n_stop(void);
	if (state & PTP_IP) {
		ptpip_unreg_n_stop();
		appInitStateUnset(PTP_IP);
	}
}

#ifdef ICAT_STREAMING
static void appMediaSrvStart(UINT32 w, UINT32 h)
{
	extern void cmdProcess(char *);	cmdProcess((char*)"os nice Cmd 31");
	extern void sioCtrl(UINT32); sioCtrl(3);
	uiPara_t* puiPara = appUiParaGet();

	NetStParams *sp = &st_params;
	const char *root_dir = "D:/";

	appMediaAttrUpdate(puiPara->VideoSize);

	sp->file_type  = SP5K_MEDIA_MOV;
	sp->jpeg_width = 640;
	sp->jpeg_height= 360;

	{
		struct NDKStCfg cfg;
		cfg.root_dir = root_dir;
		cfg.port = 554;
		cfg.st_mode = NDK_ST_DUAL_STREAMING;
		cfg.audio_on = 0;
		cfg.keepalive_secs = RTP_ALIVE_TIME;
		cfg.evt_handler = appRtpEventHandler;

		if (ndk_st_start_server(&cfg) != 0) {
			return;
		}
	}
	printf(">>> start media streaming server...........\n");
	profLogPrintf(0,"media streaming server is started");
	appInitStateSet(RTP_STREM);
}

static void appMediaSrvStop(
	void
)
{
	UINT8 state = appInitStateGet();

	if (state & RTP_STREM) {
		ndk_st_stop_server();
		appInitStateUnset(RTP_STREM);
	}
}

static void appFtpSrvStart(
	void
)
{
	UINT8 *drv;
	drv = sp5kMalloc(32*sizeof(UINT8));
	sprintf(drv,"%s",appActiveDiskGet()==2 ? "D:/":"C:/");
	ndk_ftpd_start(drv, 21);
	sp5kFree(drv);
	appInitStateSet(FTP_SRV);
}


static void appFtpSrvStop(
	void
)
{
	ndk_ftpd_stop();
	appInitStateUnset(FTP_SRV);
}

static void appStreamClose(
	void
)
{
	printf("<<< ndk streaming closed...........\n");
	ndk_st_close_streaming();
	appRtpStreamStateSet(0);
}
#endif

static BOOL appSSIDNameGet(
	UINT8 *ssidname
)
{
	UINT32 fd;
	UINT32 Len,i,j,size=0;
	UINT8  buffer[MAX_SSID_NAME_LEN+MAX_PASSWORD_LEN]; 
	/* ssid name cfg file for ssid name save assigned by user */
	fd = sp5kFsFileOpen("B:/UDF/SSID_PW.CFG",SP5K_FS_OPEN_RDONLY|SP5K_FS_OPEN_RDWR);

	if(!fd)
	{
		printf("ssid cfg file open fail!\n");
		sp5kFsFileClose(fd);
		return FAIL;
	}

	if(gssidname[0]) /* user assign custom ssid name */
	{
		memcpy(ssidname, gssidname, MAX_SSID_NAME_LEN*sizeof(UINT8));
		printf("[custom]ssidname = %s\n", ssidname);
	}
	else /* read default or user save ssid name */
#if 1	
	{
		size = sp5kFsFileSizeGet(fd);
		Len = sp5kFsFileRead(fd, buffer, size);

		for(i=0 ;i<Len ;i++ ){
			if(buffer[i]==0x0d || buffer[i]==0x0a ){
				UINT8 cnt=0;
                memcpy(ssidname, buffer, i);
				ssidname[i]='\0';
				if(buffer[i+1]==0x0d || buffer[i+1]==0x0a ){
					cnt+=1;
				}
				for(j=i+1+cnt;j<Len ;j++ ){ 
					if((buffer[j]==0x0d || buffer[j]==0x0a ) ||(j==Len-1)  ){
						memcpy(gpassword, buffer+i+1+cnt, j-i-1-cnt+1);
						if(cnt==0){
							gpassword[j-i-1-cnt+1]='\0';
						}else{
							gpassword[j-i-1-cnt]='\0';
						}
					   	break;
					}
				}
                break;
			}
		}
		printf("[cfg]ssidname = %s Password=%s,Len = %d i=%d j=%d\n", ssidname,gpassword ,Len ,i ,j);

	}
#else	
	{
		size = sp5kFsFileSizeGet(fd);
		Len = sp5kFsFileRead(fd, buffer, size);

		for(i=0 ;i<Len ;i++ ){
			if(buffer[i]==0x0d && buffer[i+1]==0x0A ){
                memcpy(ssidname, buffer, i);
				ssidname[i]='\0';
				for(j=i+2 ;j<Len ;j++ )
					if((buffer[j]==0x0d && buffer[j+1]==0x0A) ||(j==Len-1)  ){
					    
						memcpy(gpassword, buffer+i+2, Len-i-2);
					    if(buffer[j]==0x0d){;
						 gpassword[j-i-2]='\0';
						}
					   break; 
				}
                break;
			}	
		}	
		printf("[cfg]ssidname = %s Password=%s,Len = %d i=%d j=%d\n", ssidname,gpassword ,Len ,i ,j);

	}

#endif
	if(gssidname[0]){/* if no change, won't write the ssid name to file again */
		sp5kFsFileWrite( fd, buffer, (MAX_SSID_NAME_LEN+MAX_PASSWORD_LEN)*sizeof(UINT8) );
		printf("baiqi write wifi password\n");
	}

	sp5kFsFileClose(fd);
	return SUCCESS;
}

static BOOL appAPModeGet(
	UINT8 *hapMode
)
{
	UINT32 fd;
	UINT32 Len,size;
	UINT8 mode = 0;
	char smode[1]={0};
	fd = sp5kFsFileOpen("B:/UDF/APMODE.CFG",SP5K_FS_OPEN_RDONLY|SP5K_FS_OPEN_RDWR);

	if(!fd)
	{
		printf("apmode cfg file open fail!\n");
		sp5kFsFileClose(fd);
		return FAIL;
	}

	if(ghapMode) /* user assign custom ap mode */
	{
		memcpy(&mode, &ghapMode, 1*sizeof(UINT8));
	}
	else /* read default or ssid name saved by user  */
	{
		size = sp5kFsFileSizeGet(fd);
		/*printf("[deb2] size = %d\n",size);*/
		Len = sp5kFsFileRead(fd, &mode, size*sizeof(UINT8));
		mode = atoi(&mode);
		printf("[deb2] mode = %d\n",mode);
	}

	*hapMode = mode;
	printf("hapMode = %d\n", *hapMode);

	if(ghapMode){/* if no change, won't write the ap mode to file again */
		sprintf(smode,"%d",*hapMode);
		sp5kFsFileWrite( fd, smode, 1*sizeof(UINT8) );
	}

	sp5kFsFileClose(fd);
	return SUCCESS;
}


void appNetDoExit(
	UINT8 parm
)
{
	UINT8 wifiParm = (UINT8)parm;

	/* [47844] key is not allowed till net init procedure is done, SP5K_MSG_USB_PTP_OP_END |0x80009805 */
	if ((wifiParm & BTN_LCK) == BTN_LCK){
		appBtnEnable(BTN_ALL); 
		callStack();
	}
#ifdef WIFISTA	
	if ((wifiParm & STATION_MODE)== STATION_MODE){	    //Add Station mode stop case in appDoExit
		printf("exit:0x%x\n",(wifiParm & STATION_MODE));
		appSTAServerStop();
	}
#endif

	if ((wifiParm & FTP_SRV)== FTP_SRV){
		printf("exit:0x%x\n",(wifiParm & FTP_SRV));
		appFtpSrvStop();
	}
	
	if ((wifiParm & RTP_STREM)== RTP_STREM){
		printf("exit:0x%x\n",(wifiParm & RTP_STREM));
		appMediaSrvStop();
	}

	if ((wifiParm & PTP_IP)== PTP_IP){
		printf("exit:0x%x\n",(wifiParm & PTP_IP));
		appPtpIpStop();
		ndk_msleep(100); /* [workaround] : hapd can't stop normally issue */
	}

	if ((wifiParm & HOST_AP)== HOST_AP){
		printf("exit:0x%x\n",(wifiParm & HOST_AP));
		appHostAPStop();
	}

	if ((wifiParm & DHCP_SERVER)== DHCP_SERVER){
		printf("exit:0x%x\n",(wifiParm & DHCP_SERVER));
		appDhcpServerStop();
	}

	if((wifiParm & WIFI_LOAD)== WIFI_LOAD){
		printf("exit:0x%x\n",(wifiParm & WIFI_LOAD));
		if (appWiFiSuspend() != 0) {
			printf("WiFi suspend failed!\n");
			return;
		}
	}

	//fix close app on share mode can't open app again
	if(app_PTP_Get_DscOpMode(MODE_ACTIVE)== PTP_DSCOP_MODE_SHARE){
		app_PTP_Set_DscOpMode(PTP_DSCOP_MODE_CAMERA);
	}
}

static void appNetDoInit(
	UINT32 parm
)
{
	UINT8 wifiParm = (UINT8)parm;
	NetStParams *sp = &st_params;
	UINT8 state;
	BOOL start_record = FALSE, stop_record = FALSE;
	uiPara_t* puiPara = appUiParaGet();

	/* [47844] key is not allowed till net init procedure is done, SP5K_MSG_USB_PTP_OP_END |0x80009805 */
	if ((wifiParm & BTN_LCK) == BTN_LCK){
		appBtnDisable(BTN_ALL); 
		
		/*xuan.ruan@20131128 add for mantis bug 0048884*/
		#ifdef _EVB_HW_  //evb use button right as wifi ctrl
		appBtnEnable(BTN_RIGHT);
		#endif
		callStack();
	}

	if ((wifiParm & WIFI_LOAD) == WIFI_LOAD) {
		state = appInitStateGet();
		if (state & WIFI_LOAD) {
            /* WIFI LOADED */
			if (appWiFiResume() != 0) {
				printf("WiFi resume failed!\n");
				return;
			}
		}
		else {
			if(appWiFiInit(0,"rtk")!=0){
				printf("Loading WiFi module failed!\n");
				return;
			}
		}
	}

#ifdef WIFISTA																	// add by simen wifi station 
	if ((wifiParm & STATION_MODE) == STATION_MODE) {	
		printf("Entry station mode\n");
		UINT16 staTimeout = 0;
		app_station_mode();
	}
#endif

	if ((wifiParm & DHCP_SERVER) == DHCP_SERVER) {
		appDhcpServerStart();
	}

	if ((wifiParm & HOST_AP) == HOST_AP) {
		UINT8 ssidname[MAX_SSID_NAME_LEN]={0};
		UINT8 hapMode = 0;

		if(appSSIDNameGet(ssidname)){
			printf("ssid name get failed!\n");
			return;
		}

		if(appAPModeGet(&hapMode)){
			printf("ap mode get failed!\n");
			return;
		}

		if(hapMode==2)
		{
			const char* passphrase = "1234567890";
			const char* channelscan = "auto";
			if(gpassword[0]){
				appHostAPStart("B:/UDF/HAPD0.CFG", ssidname, gpassword, channelscan);
					
			}
			else{  
				appHostAPStart("B:/UDF/HAPD0.CFG", ssidname, passphrase , channelscan);
				
			}
		}
		else
		{
			const char* channelscan = "auto";
			if(gpassword[0])
				appHostAPStart("B:/UDF/HAPD0.CFG", ssidname, gpassword ,channelscan);
            else
				appHostAPStart("B:/UDF/HAPD0.CFG", ssidname, NULL ,channelscan);
			
		}

		/*ndktimer = appTimerSet(1000, "WiFiinfo");*/
	}

	if ((wifiParm & PTP_IP) == PTP_IP) {
		sp5kUsbPimaConfigSet(SP5K_PIMA_CONFIG_MTP_BK_BUFFER_SIZE, 512*1024); /* 512KB support about 18XX images buffer */
		sp5kTimeDelay(3, 500);
		sp5kUsbPtpOpFuncInstall(USB_PIMA_OC_DELETE_OBJECT, 0, NULL, pimaDelFileCb);
		appPtpIpStart();
		sp5kUsbPimaMtpBkCtl(SP5K_USB_PIMA_MTP_BK_MANUAL);
	}

	if ((wifiParm & FTP_SRV) == FTP_SRV) {
		state = appInitStateGet();
		if (!(state & FTP_SRV)){/* ftpd won't be stopped after WiFi off  */
			appFtpSrvStart();
		}
	}

#ifdef ICAT_STREAMING
	if ((wifiParm & RTP_STREM) == RTP_STREM) {
		UINT32 w,h;

		profLogPrintf(1,"[PTPEVT,PTPQ](%d,%d)",PTP_EVT_SEND_ENABLE,PTP_EVT_QUEUE_ENABLE);

	     /* device property get */
		appVideoSizeGet(puiPara->VideoSize, &w, &h);
		if(appPtpOpModeHandler(app_PTP_Get_DscOpMode(MODE_ACTIVE),&start_record,&stop_record)!=0)
		{
			return;
		}

		printf("****** RTP Streaming setting ******\n");
		printf("**  W(%d)\n**  H(%d)\n**  Video(%s)\n", w, h
			, start_record ? "rec+": (stop_record ? "rec-" : "n/a"));
		printf("***********************************\n");

		/* the param is set in appMediaSrvStart() originally, but since no teardown flow doesn't need to restart media server,
		so w&h should be reset here
		*/
		appRtpResetParams(sp,puiPara->VideoSize);
		if(appPtpVideoHandler(sp,start_record,stop_record)!=0)
		{
			return;
		}

        /* start media server */
		if((app_PTP_Get_DscOpMode(MODE_ACTIVE)==PTP_DSCOP_MODE_CAMERA
			||app_PTP_Get_DscOpMode(MODE_ACTIVE)==PTP_DSCOP_MODE_VIDEO_OFF
			||app_PTP_Get_DscOpMode(MODE_ACTIVE)==PTP_DSCOP_MODE_VIDEO_ON)
			&&!appRtpStreamStateGet())
		{
			appRtpStreamStateSet(1);
			if (!ndk_st_streaming_is_started())
				appMediaSrvStart(w, h);
				appCustMsrcInit();

				
		}
	}
#endif

	#if 0 /* no need, already sent in SP5K_MSG_USB_PTP_SESSION_OPEN */
	if(!IS_CARD_EXIST){
		appStill_PIMA_Send_iCatch_Event(PTP_ICAT_EVENT_SDCARD_OUT, 0, 0);
	}
	#endif
	
	if ((wifiParm & DO_ASYNC) == DO_ASYNC) {
		sp5kOsThreadDelete(NULL);
	}
}

UINT32 appPtpOpModeHandler(PTPDscOpMode_e opModeState, BOOL *isStartRec, BOOL *isStopRec)
{
	UINT32 curMode, ret = 0;
	uiPara_t* puiPara = appUiParaGet();
	
	sp5kModeGet(&curMode);
	switch(opModeState){ 
		case PTP_DSCOP_MODE_CAMERA:
		case PTP_DSCOP_MODE_VIDEO_OFF:  /* PTP cmd to control the streaming mode */
			printf("[DSC OP] CAMERA mode / VIDEO mode OFF!\n");
			*isStopRec = TRUE;
			if(curMode==SP5K_MODE_STANDBY){
				sp5kModeSet(SP5K_MODE_VIDEO_PREVIEW);
				sp5kModeWait(SP5K_MODE_VIDEO_PREVIEW);
			}
			#if FIX_SENSOR_MODE_4_CAM_MODE
			/* 47282 / 47614 : a fixed sensor mode for capture mode : s */
			if(app_PTP_Get_DscOpMode(MODE_ACTIVE)== PTP_DSCOP_MODE_CAMERA
				&&puiPara->VideoSize!=UI_VID_SIZE_FHD){
				appMediaAttrUpdate(UI_VID_SIZE_FHD);
			}

			if(app_PTP_Get_DscOpMode(MODE_ACTIVE)== PTP_DSCOP_MODE_VIDEO_OFF
				&&app_PTP_Get_DscOpMode(MODE_PREV)==PTP_DSCOP_MODE_CAMERA
				&&puiPara->VideoSize!=UI_VID_SIZE_FHD)
			{
				sp5kHostMsgSend(APP_UI_MSG_RTP_STOP,0);
				appMediaAttrUpdate(puiPara->VideoSize);
				sp5kTimeDelay(SP5K_TIME_DELAY_1MS, 100);
	   			sp5kHostMsgSend(APP_UI_MSG_RTP_START,0);
			}
			/* 47282 / 47614 : a fixed sensor mode for capture mode : e*/
			#endif
			break;
		case PTP_DSCOP_MODE_VIDEO_ON:  /* PTP cmd to control the streaming mode */
			printf("[DSC OP] VIDEO mode ON!\n");
			*isStartRec = TRUE;
			if(curMode==SP5K_MODE_STANDBY){
				sp5kModeSet(SP5K_MODE_VIDEO_PREVIEW);
				sp5kModeWait(SP5K_MODE_VIDEO_PREVIEW);
			}
			break;
		case PTP_DSCOP_MODE_SHARE:
			printf("[DSC OP] SHARE mode!\n");
			
			#ifdef ICAT_STREAMING
			if(appRtpStreamStateGet())
			{
				appStreamClose();
			}
			#endif

			if(curMode!=SP5K_MODE_STANDBY){
				profLogPrintf(0,"enter standy:s");
				sp5kModeSet(SP5K_MODE_STANDBY);
				sp5kModeWait(SP5K_MODE_STANDBY);
				profLogPrintf(0,"enter standy:e");
				printf("idle mode ready!\n");
			}
			
			return 1;
			break;
		case PTP_DSCOP_MODE_APP:
			printf("[DSC OP] APP mode!\n");
			#ifdef ICAT_STREAMING
			if(appRtpStreamStateGet())
			{
				appStreamClose();
			}
			#endif
			if(app_PTP_ContRecMode_Get()&&curMode==SP5K_MODE_VIDEO_RECORD&&
				app_PTP_Get_DscOpMode(MODE_PREV)==PTP_DSCOP_MODE_VIDEO_ON){
				app_PTP_Set_DscOpMode(PTP_DSCOP_MODE_VIDEO_ON);
			}
			else if(curMode==SP5K_MODE_VIDEO_PREVIEW&&
				app_PTP_Get_DscOpMode(MODE_PREV)==PTP_DSCOP_MODE_CAMERA){
				app_PTP_Set_DscOpMode(PTP_DSCOP_MODE_CAMERA);
			}
			else if(curMode==SP5K_MODE_VIDEO_PREVIEW&&
				app_PTP_Get_DscOpMode(MODE_PREV)==PTP_DSCOP_MODE_VIDEO_OFF){
				app_PTP_Set_DscOpMode(PTP_DSCOP_MODE_VIDEO_OFF);
			}
			else {
				app_PTP_Set_DscOpMode(PTP_DSCOP_MODE_VIDEO_OFF);
			}
			app_PTP_clear_EventQ();
			/* Digital Zoom Reset : s */
			if(appDZoomGetRation()!=1000){
				appDZoomReset(APP_DZOOM_UI_MODE_VIDEO_VIEW);
				appStill_SetDZoom(puiPara->DigitalZoom);
			}
			/* Digital Zoom Reset : e */
			return 1;
			break;
		default:
			printf("[PTP] you got the wrong camera mode:%d\n",app_PTP_Get_DscOpMode(MODE_ACTIVE));
			return 1;
			break;
	}
	return ret;
}	

UINT32 appPtpVideoHandler(NetStParams *sp, BOOL isStartRec, BOOL isStopRec)
{
	UINT32 ret = 0;
	uiPara_t* puiPara = appUiParaGet();
	
	HOST_ASSERT(!(isStartRec && isStopRec));
	// Start Record
	if (appRtpStreamStateGet() && isStartRec) {
		// Enter VideoRecord mode
		UINT32 curMode;
		sp5kModeGet(&curMode);

		if (curMode == SP5K_MODE_VIDEO_RECORD){
			printf("[RTP]Already in record mode\n");
			return 1;
		}
		else
		{
			#if !SENSOR_MODE_CHANGE_WITH_VIDEOSIZE
			/* Mantis issue #47000, video size no change issue */
			appRtpUpdateMediaAttrs(sp,puiPara->VideoSize);
			#endif
			
			#if SP5K_CDFS_OPT
			appCdfsFileFolderSet(CDFS_FILE_FOLDER_VIDEO);
			appCdfsFolderInit(CDFS_FILE_FOLDER_VIDEO);
			#endif
			appStateChange(APP_STATE_VIDEO_REC,  APP_STATE_MSG_INIT);
		}
		printf("\n[RTP] Start record!\n");
		return 1;
	}
	// Stop Record
	else if (appRtpStreamStateGet() && isStopRec) {
		// Leave VideoRecord mode
		UINT32 curMode;
		sp5kModeGet(&curMode);
		printf("\n[RTP] Video Stop now!\n");
		if (curMode == SP5K_MODE_VIDEO_RECORD){
			appBtnDisable(BTN_ALL);

			/*xuan.ruan@20131128 add for mantis bug 0048884*/
			#ifdef _EVB_HW_  //evb use button right as wifi ctrl
			appBtnEnable(BTN_RIGHT);
			#endif
			appVideoStop();
			UINT32 totalFile;
			UINT8 pMediafilename[32];
			sp5kDcfAttrElm_t pdcfAttr;

			sp5kDcfFsInfoGet(SP5K_DCF_INFO_TOTAL_FILENUM,&totalFile);
			if(sp5kDcfFsFileAttrGet(totalFile, &pdcfAttr, pMediafilename))
				printf("[RTP] Get dcf attr fail!\n");
			else
				printf("totalFile = %d, media file : %s\n",totalFile, pMediafilename);

			/* Notify mobile APP that new video object file is created */
			#if SP5K_CDFS_OPT/* tmp disable, wait for the video file path issue solved 20130305*/
			pMediafilename[2] = '\\';
			pMediafilename[8] = '\\';
			#endif
			printf("media file : %s\n",pMediafilename);
			appStill_PIMA_Send_Event(PTP_EC_OBJECT_ADDED, (UINT32)pMediafilename, 0, 0);
			appBtnEnable(BTN_ALL); 
		}
		else{
			printf("[RTP]Not in record mode\n");
			return 1;
		}

		printf("\n[RTP] Stop record!\n");
		return 1;
	}
	else if(appRtpStreamStateGet())
	{
		printf("\n[RTP] No Streaming mode switch !!\n");
		return 1;
	}
	return ret;
}

void appMediaAttrUpdate(UINT8 vidsize)
{
	NetStParams *sp = &st_params;
	
	printf("Update Param\n");
	appRtpResetParams(sp,vidsize);
	appRtpUpdateMediaAttrs(sp,vidsize);
}

/* External using API */
void appPtpStateSet(BOOL en)
{
	bPtpStatus = en;
}

BOOL appPtpStatGet()
{
	return bPtpStatus;
}

void appRtpStreamStateSet(
	BOOL en
)
{
	profLogPrintf(0,"rtp streaming state set:%d",en);
	bRtpStatus = en;
}

BOOL appRtpStreamStateGet()
{
	profLogPrintf(0,"rtp streaming state get:%d",bRtpStatus);
	return bRtpStatus;
}

BOOL appIsStreamingActive()
{

#ifdef ICAT_STREAMING
	return ndk_st_streaming_is_started();
#else
	return 0;
#endif

}

UINT32 appNetNumGet()
{
	static long sta_nr = 0;
	long n;
	if (ndk_netif_ioctl(NDK_IOCG_WIFI_STA_NR, (long)"wlan0", &n) == 0) {
		if (n != sta_nr) {
			profLogPrintf(1,"sta number:%u -> %u", (unsigned int)sta_nr, (unsigned int)n);
			sta_nr = n;
		}
	}
	else
		return 0;

	return n;
}

void appNetSTAInfoGet(long mac,int expire,int expire_max)
{
#if 0
	struct if_point ifp;
	char buf[256];
	//long long mac = 0;
	int count = 0;
	char *pch;

	ifp.pointer = buf;
	ifp.length = NDK_ARRAY_SIZE(buf);

	if (ndk_netif_ioctl(NDK_IOCG_WIFI_STA_INFO, (long)"wlan0", (long *)&ifp) != 0) {
		printf("[IOCTL] NDK_IOCG_WIFI_STA_INFO error\n");
		return;
	}

	pch = buf;

	/* FIXME: it seems ndk_sscanf() doesn't support %llx pattern */
	while (ndk_sscanf(pch, "MAC:%x,EXP:%d/%d\n*", &mac, &expire, &expire_max) == 3) {
		printf("[%d] MAC address %12llX have no reponse during %d second\n", count, mac, (expire_max - expire) * 2);
		pch = memchr(pch, '\n', buf + NDK_ARRAY_SIZE(buf) - pch);
		if (pch == NULL) {
			break;
		}
		++pch;
		++count;
	}
#endif
}

void appWiFiStartConnection(UINT8 wifiParm)
{
	printf("%s,0x%x\n",__FUNCTION__,wifiParm);
	if ((wifiParm & DO_ASYNC) == DO_ASYNC) {
		sp5kOsThreadCreate("netStart", appNetDoInit, (UINT32)wifiParm
		                   , ndk_thread_get_priority(), 0, 0
		                   , TX_AUTO_START);
	}
	else {
		appNetDoInit((UINT32)wifiParm);
	}
}

void appWiFiStopConnection(UINT8 wifiParm)
{
	profLogAdd(0,"appWiFiStopConnection");
	printf("%s,0x%x\n",__FUNCTION__,wifiParm);
	appExceptionHandle();
	appNetDoExit(wifiParm);
	appNetStateReset(wifiParm);
}

void appExceptionHandle()
{
	appStill_PIMA_Send_iCatch_Event(PTP_ICAT_EVENT_EXCEPTION, 0, 0); /* notify the APP to do connection close */
	ndk_msleep(1500);/* wait for mobile process*/
}

/* app_net_cmd.c is removed */
void net_system_init()
{
	ndk_global_init(20);
	printf("WiFi system init done.\n");
}

/* AP mode <-> STA mode switch */
#if 0
void util_apmode_enter();
void util_apmode_exit();
void util_stamode_enter(UINT8 *mob_ssid,UINT8 *mob_pass);
void util_sta_test();

#define MOB_WPA2 (1)
void util_apmode_enter()
{
	printf(">>> enter AP mode\n");
	appWiFiStartConnection(0x07);
}

void util_apmode_exit()
{
	printf("<<< exit AP mode\n");
	appHostAPStop();
	appDhcpServerStop();
}

void util_stamode_enter(UINT8 *mob_ssid,UINT8 *mob_pass)
{
	char buf[64];
	ndk_wpas_start();
	ndk_msleep(300);

	ndk_wpac_begin();
	ndk_wpac_request("add_network");

	sprintf(buf, "set_network 0 ssid '\"%s\"'", mob_ssid);
	ndk_wpac_request(buf);

	#if MOB_WPA2
	sprintf(buf, "set_network 0 psk '\"%s\"'", mob_pass);
	ndk_wpac_request(buf);
	ndk_wpac_request("set_network 0 key_mgmt WPA-PSK");
	#else
	ndk_wpac_request("set_network 0 NONE WPA-PSK");
	#endif

	ndk_wpac_request("enable_network 0");
	ndk_wpac_end();
	ndk_dhcp_start("wlan0");
}

void util_sta_test()
{
	util_apmode_enter();
	#if 0
	static long sta_num=0;
	while( !sta_num ){
		ndk_netif_ioctl(NDK_IOCG_WIFI_STA_NR, (long)"wlan0", &sta_num);
	}
	#endif
	profLogPrintf(0,"exit AP mode +");
	util_apmode_exit();
	util_stamode_enter("lumia_920_tien","0916816138");
	profLogPrintf(0,"enter STA mode -");
}
#endif

/*****************************************************************************/
/*				   Custom Media Source for Media Files						 */
/*****************************************************************************/

#define DSTWR16(p_dst, dst16)  *(UINT16*)(p_dst)=(dst16); p_dst+=2
#define DSTWR32(p_dst, dst32)  *(UINT32*)(p_dst)=(dst32); p_dst+=4
#define SRCRD16(p_src, src16)  src16=*(UINT16*)(p_src); p_src+=2
#define SRCRD32(p_src, src32)  src32=*(UINT32*)(p_src); p_src+=4
#define ROUND_DOWN_TO_DIVISIBLE(num,div) \
	( (UINT32)(num) & -(UINT32)(div) )
#define ROUND_UP_TO_DIVISIBLE(num,div) \
	ROUND_DOWN_TO_DIVISIBLE( (UINT32)(num) + (div) - 1, div )

static UINT8  gfseek=0;
static UINT8  gPlayabort=0;
static UINT32 gSkipFrmModular = 1;

typedef struct {
	NDKStMediaSrcPushBufferFunc push_func;
	void *push_param;
	char fname[256];

	BOOL   bAbort;

	UINT64 audTotalBytes;
	SINT64 audStartPTS;
	UINT32 audBytesPerSec;
	UINT32 audSampleBits;

	UINT32 vidDurationTime;
	UINT32 vidStartPTS;
	UINT32 vidFrmRate;
	UINT32 vidWidth;
	UINT32 vidHeight;
	UINT32 vidFrmCnt;

	UINT32 yuvWidth;
	UINT32 yuvHeight;
	UINT8* yuv422Buf;
	UINT8* jfifBuf;
} appCustMsrcCtx_t;

typedef struct {
	BOOL	bAudio;
	UINT32	length;
	UINT8	*data;
} appCustMsrcBuf_t;

/* The global and the only one RTP playback instance. */
static appCustMsrcCtx_t *g_custpb_ctx = NULL;

static UINT8*
appCustMsrcYUVScale2Jfif(
	appCustMsrcCtx_t *c,
	UINT8 *yuvAddr,
	UINT32 yuvWidth,
	UINT32 yuvHeight,
	UINT32 yuvFmt, /*0: yuv422, 1: yuv420. Ref.: DEC_OUTPUT_YUV_FORMAT */
	UINT32 *length
)
{
	UINT32 nRet = 0;
	#if 0
	char fname[32];
	static UINT8 yuvId = 0;
	static UINT8 jpgId = 0;
	#endif

	sp5kGfxObj_t dstObj;
	sp5kGfxObj_t srcObj = {
		(UINT8 *)yuvAddr, NULL,
		ROUND_UP_TO_DIVISIBLE(yuvWidth, 16), ROUND_UP_TO_DIVISIBLE(yuvHeight, 16),
		0, 0,
		SP5K_GFX_FMT_YUV420,
		yuvWidth, yuvHeight
	};
	srcObj.fmt = SP5K_GFX_FMT_YUV420;
	srcObj.roiX = dstObj.roiX = 60;

	if (abs(yuvWidth*9 - yuvHeight*16) < 128){ // 16:9
		dstObj.roiH = dstObj.bufH = 360 ;
		dstObj.roiW = dstObj.bufW = 640 ;
	}
	else { // 16:9 , too .
		dstObj.roiH = dstObj.bufH = 360 ;
		dstObj.roiW = dstObj.bufW = 640 ;
	}

	dstObj.fmt = SP5K_GFX_FMT_JFIF;
	dstObj.pbuf = sp5kYuvBufferAlloc(512, 1024);
	if (!dstObj.pbuf) {
		printf("appCustMsrc> error: jfif alloc\n");
		return NULL;
	}

	nRet = sp5kGfxObjectConvert(&srcObj, &dstObj);

	/* Reduce memory usage */
	UINT8 *buf = sp5kMallocCache(dstObj.roiX);
	if (buf) {
		memcpy(buf, dstObj.pbuf, dstObj.roiX);
		*length = (UINT32)dstObj.roiX;
	}
	else {
		printf("appCustMsrc> error: buf alloc\n");
	}

	sp5kYuvBufferFree(dstObj.pbuf);
	return buf;
}

static void
appCustMsrcSwapAudData16(
	UINT8 *dst,
	UINT8 *src,
	UINT32 length
)
{
	UINT8 *p_src, *p_dst, *p_src_end4;
	UINT32 d32;
	UINT16 d16;

	p_src = src;
	p_dst = dst;
	p_src_end4 = (UINT8*)(((UINT32)src + length) & ~0x03);

	if ((UINT32)p_src & 0x03) {
		SRCRD16(p_src, d16);
		DSTWR16(p_dst, ((d16 >> 8) & 0x00FF) | ((d16 << 8) & 0xFF00));
	}

	while (p_src < p_src_end4) {
		SRCRD32(p_src, d32);
		DSTWR32(p_dst, ((d32 >> 8) & 0x00FF00FFU) | ((d32 << 8) & 0xFF00FF00U));
	}

	if (p_src < src + length) {
		SRCRD16(p_src, d16);
		DSTWR16(p_dst, ((d16 >> 8) & 0x00FF) | ((d16 << 8) & 0xFF00));
	}
}

static BOOL
appCustMsrcPushData(
	appCustMsrcCtx_t *c,
	UINT8 *data,
	UINT32 length,
	SINT64 pts,
	BOOL bAudio
)
{
	if (gfseek) {
		return FALSE;
	}

	NDKStMediaBuffer mb;
	appCustMsrcBuf_t *cbuf = (appCustMsrcBuf_t*)sp5kMallocCache(sizeof(appCustMsrcBuf_t));
	if (!cbuf) {
		printf("sp5kMallocCache false!\n");
		return FALSE;
	}

	cbuf->bAudio = bAudio;
	cbuf->data   = data;
	cbuf->length = length;

	mb.bufobj		= cbuf;
	mb.f_buftype	= bAudio ? NDK_ST_MEDIABUFFER_AUDIO : NDK_ST_MEDIABUFFER_VIDEO;
	mb.f_keyframe	= 1;
	mb.f_paramframe	= 0;
	mb.data			= data;
	mb.length		= length;
	if (bAudio) {
		mb.duration = 1000 * length / c->audBytesPerSec;
	}
	else {
		mb.duration = 1000 / c->vidFrmRate;
	}
	mb.pts.tv_sec   = pts/1000;
	mb.pts.tv_usec  = (pts % 1000) * 1000;

	c->push_func(&mb, c->push_param);

	int cnt = 0;
	if ((cnt != pts/1000)) {
		profLogPrintf(0 , "[audio=%d] transport success[%d:%d]\n",bAudio,(UINT32)(pts/1000/60),(UINT32)(pts/1000%60));
	}
	cnt = pts/1000;

	return TRUE;
}

static void
appCustMsrcVidDataCb(
	UINT8 *yuvAddr,
	UINT32 yuvWidth,
	UINT32 yuvHeight, 
	UINT32 yuvFmt
)
{
	appCustMsrcCtx_t *c = g_custpb_ctx;
	/*SINT64 pts = 0;*/
	UINT32 pts = 0;
	UINT32 frmcnt = 0;
	//printf("########################################################### appCustMsrcVidDataCb s\n");
	//printf("appCustMsrcVidDataCb 00000> g_custpb_ctx->vidFrmCnt=%d  frmcnt=%d\n", g_custpb_ctx->vidFrmCnt, frmcnt);

	if (gfseek) {
		return;
	}

	//printf("appCustMsrcVidDataCb> gSkipFrmModular=%d\n", gSkipFrmModular);
	if (gSkipFrmModular == 4) {
		//printf("appCustMsrcVidDataCb 11111> c->vidFrmCnt=%d  c->vidFrmRate=%d\n", c->vidFrmCnt, c->vidFrmRate);
		/*pts = (SINT64)(((c->vidFrmCnt*gSkipFrmModular * 1000)/c->vidFrmRate) + c->vidStartPTS);*/
		pts = ((c->vidFrmCnt*gSkipFrmModular * 1000)/c->vidFrmRate) + c->vidStartPTS;
		frmcnt = c->vidFrmCnt++;
	}
	else {
		//printf("appCustMsrcVidDataCb 22222> c->vidFrmCnt=%d  c->vidFrmRate=%d\n", c->vidFrmCnt, c->vidFrmRate);
		pts = (SINT64)(((c->vidFrmCnt * 1000)/c->vidFrmRate) + c->vidStartPTS);
		frmcnt = c->vidFrmCnt++;
		//printf("appCustMsrcVidDataCb 33333> frmcnt=%d  c->vidFrmCnt=%d\n", frmcnt, c->vidFrmCnt);

		if (gSkipFrmModular > 1 && (frmcnt % gSkipFrmModular) != 0) {
			return;
		}
	}
	//printf("vid pts = %d, %d, %d\n", pts, frmcnt, c->vidStartPTS);
	//return;

	UINT32 length = 0;
	UINT8 *v_data = appCustMsrcYUVScale2Jfif(g_custpb_ctx, yuvAddr, yuvWidth, yuvHeight, yuvFmt, &length);

	if (v_data) {
		if (!appCustMsrcPushData(g_custpb_ctx, v_data, length, pts, FALSE)) {
			sp5kFree(v_data);
			printf("video transport false!\ns");
		}
	}
	//printf("########################################################### appCustMsrcVidDataCb e\n");
}

static void
appCustMsrcAudDataCb(
	UINT8 *audAddr,
	UINT32 decodedSize
)
{
	appCustMsrcCtx_t *c = g_custpb_ctx;
	//printf("########################################################### appCustMsrcAudDataCb s\n");
	//printf("appCustMsrcAudDataCb 00000> g_custpb_ctx->audTotalBytes=%d  g_custpb_ctx->audBytesPerSec=%d\n", g_custpb_ctx->audTotalBytes, g_custpb_ctx->audBytesPerSec);

	if (gfseek || !c->vidFrmCnt) {
		return;
	}

	//printf("appCustMsrcAudDataCb 11111> c->audTotalBytes=%d  c->audBytesPerSec=%d  decodedSize=%d\n", c->audTotalBytes, c->audBytesPerSec, decodedSize);
	/*SINT64 pts = (SINT64)(((c->audTotalBytes*1000)/c->audBytesPerSec) + c->audStartPTS);*/
	UINT32 pts = ((c->audTotalBytes*1000)/c->audBytesPerSec) + c->audStartPTS;

	c->audTotalBytes += decodedSize;
	//printf("appCustMsrcAudDataCb 22222> c->audTotalBytes=%d  c->audBytesPerSec=%d\n", c->audTotalBytes, c->audBytesPerSec);
	//printf("aud pts = %d, %d, ,%d, %d\n", pts, c->audTotalBytes,c->audBytesPerSec,decodedSize);
	//return;
	UINT8 *a_data = sp5kMallocCache(decodedSize);
	//printf("rtppb %s:%d	%p %u\n", __func__, __LINE__, audAddr, decodedSize);
	if (!a_data) {
		printf("appCustMsrc> aud malloc\n");
		return;
	}

	if (g_custpb_ctx->audSampleBits == 16) {
		appCustMsrcSwapAudData16(a_data, audAddr, decodedSize);
	}
	else {
		memcpy(a_data, audAddr, decodedSize);
	}
 
	if (!appCustMsrcPushData(g_custpb_ctx, a_data, decodedSize, pts, TRUE)) {
		sp5kFree(a_data);
		printf("audio transport false!\ns");
	}
	//printf("########################################################### appCustMsrcAudDataCb e\n");
}

static NDKStMediaSrcHandle
appCustMsrcOpen(
	NDKStMediaSrcPushBufferFunc push_func,
	void *push_param,
	void *src_arg,
	...
)
{
	if (g_custpb_ctx) {
		return NULL;
	}

	UINT32 audSampleBits, audSampleRate, audChannels;
	appCustMsrcCtx_t *c = sp5kMallocCache(sizeof(appCustMsrcCtx_t));
	if (!c) {
		return NULL;
	}

	memset(c, 0, sizeof(appCustMsrcCtx_t));
	c->push_func = push_func;
	c->push_param = push_param;
	strcpy(c->fname, src_arg);

	sp5kModeSet(SP5K_MODE_STANDBY);
	sp5kModeWait(SP5K_MODE_STANDBY);

	sp5kMediaPlayAttrSet(SP5K_MEDIA_ATTR_FILE_NAME, (UINT32)src_arg);
	sp5kStillPlayCfgSet(SP5K_SPB_RING_BUFFER_MAX_NUM, 8);
	sp5kMediaPlayCfgSet(SP5K_MEDIA_PLAY_MAX_DEC_BUF_NUM, 8);

	sp5kModeSet(SP5K_MODE_VIDEO_PLAYBACK);
	sp5kModeWait(SP5K_MODE_VIDEO_PLAYBACK);

	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_WIDTH, &c->vidWidth);
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_HEIGHT, &c->vidHeight);
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_VIDEO_FRAME_RATE, &c->vidFrmRate);
	//appHostMsgWaitExact(SP5K_MSG_MODE_READY,SP5K_MODE_VIDEO_PLAYBACK,5000);
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_DURATION, &c->vidDurationTime) ;

	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_AUDIO_SAMPLE_BITS, &audSampleBits);
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_AUDIO_SAMPLE_RATE, &audSampleRate);
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_AUDIO_CHANNELS, &audChannels);

	sp5kMediaPlayCfgSet(SP5K_MEDIA_PLAY_VIDEO_FRAME_CB, (UINT32)appCustMsrcVidDataCb);
	sp5kMediaPlayCfgSet(SP5K_MEDIA_PLAY_AUDIO_DATA_CB, (UINT32)appCustMsrcAudDataCb);
	sp5kMediaPlayCfgSet (SP5K_MEDIA_PLAY_TARGET_FRAME_RATE, 0);
	sp5kMediaPlayControl(SP5K_MEDIA_PLAY_START, 0, 0);
	gPlayabort = 0;

	if (c->vidFrmRate <=30 ) {
		gSkipFrmModular = 1;
	}
	else if (c->vidFrmRate == 60) {
		gSkipFrmModular = 2;
	}
	else if (c->vidFrmRate == 120) {
		gSkipFrmModular = 4;
	}
	else {
		sp5kFree(c);
		printf("Wrong vid frmrate\n");
		return NULL;
	}

	c->audBytesPerSec = audSampleBits * audSampleRate * audChannels / 8;
	c->audSampleBits = audSampleBits;
	printf("appCustMsrc> audSampleRate=%d  audChannels=%d\n", audSampleRate, audChannels);
	printf("appCustMsrc> open A: %u %u, V:%u\n", c->audBytesPerSec, audSampleBits, c->vidFrmRate);

	g_custpb_ctx = c;
	printf("appCustMsrc 11111> open A: %u %u, V:%u\n", g_custpb_ctx->audBytesPerSec, audSampleBits, g_custpb_ctx->vidFrmRate);

	LCD_BACKLIGHT_OFF;
	CLEAR_OSD_SRC;
	appOsd_ColorDrawTextColorSet(PAL_WHITE, PAL_BLACK, PAL_NOCOLOR, PAL_BLEND_100);
	sp5kAudDevVolumeSet(SP5K_AUD_DEV_PLAY|SP5K_AUD_PLAY_CH_ALL, 0);
	APP_OSD_REFRESH_ON;

	return (NDKStMediaSrcHandle)c;
}

static void
appCustMsrcClose(
	NDKStMediaSrcHandle h
)
{
	appCustMsrcCtx_t *c = (appCustMsrcCtx_t *)h;
	UINT32 elapsedTime;

	HOST_ASSERT(c == g_custpb_ctx);

	gfseek =1;
	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_ELAPSED_TIME, &elapsedTime);
	printf("appCustMsrc> close: %d  %d\n", c->vidDurationTime, elapsedTime);
	sp5kMediaPlayControl(SP5K_MEDIA_PLAY_ABORT, 0, 0);
	gfseek =0;

	APP_SLEEP_MS(100);
	if (c->yuv422Buf) {
		sp5kYuvBufferFree(c->yuv422Buf);
	}
	if (c->jfifBuf) {
		sp5kYuvBufferFree(c->jfifBuf);
	}
	sp5kFree(c);

	extern uiParamSetting_t *pUiSetting;
	if(pUiSetting->sound.volume >= SOUND_VOLUME_MIDDLE) {
		appSundVolumeSet(pUiSetting->sound.volume);
	}

	sp5kModeSet(SP5K_MODE_STANDBY);
	sp5kModeWait(SP5K_MODE_STANDBY);

	g_custpb_ctx = NULL;
}

static UINT32
appCustMsrcPause(
	NDKStMediaSrcHandle h
)
{
	appCustMsrcCtx_t *c = (appCustMsrcCtx_t *)h;
	UINT32 ret = FAIL;
#if 0
	if (!c->bAbort) {
		//printf("appCustMsrcPause\n");
		ret = sp5kMediaPlayControl(SP5K_MEDIA_PLAY_PAUSE, 0, 0);
		ret = SUCCESS;
	}

	return ret;
#endif

	return SUCCESS;
}

static UINT32
appCustMsrcResume(
	NDKStMediaSrcHandle h
)
{
	appCustMsrcCtx_t *c = (appCustMsrcCtx_t *)h;
	UINT32 ret = FAIL;

	if (!c->bAbort && gPlayabort == 0) {
		profLogAdd(0," video resume start ^^\n");
		sp5kMediaPlayControl(SP5K_MEDIA_PLAY_RESUME, 0, 0);
		ret = SUCCESS;
	}

	return ret;
}

/*return TRUE: at the end of source. FALSE: not.*/
static BOOL
appCustMsrcEndOfSrc(
	NDKStMediaSrcHandle h
)
{
	appCustMsrcCtx_t *c = (appCustMsrcCtx_t *)h;
	UINT32 ret = FALSE;
	UINT32 elapsedTime;

	sp5kMediaPlayAttrGet(SP5K_MEDIA_ATTR_ELAPSED_TIME, &elapsedTime);

	if (c->vidDurationTime == elapsedTime) {
		printf("%s is todo %d  %d\n", __FUNCTION__, c->vidDurationTime, elapsedTime);
		profLogPrintf(0 , "%s is todo %d  %d\n", __FUNCTION__, c->vidDurationTime, elapsedTime);
		gfseek =1;
		gPlayabort = 1;
		sp5kMediaPlayControl(SP5K_MEDIA_PLAY_ABORT, 0, 0);
		gfseek =0;

		ret = TRUE;
	}

	return ret;
}

static UINT32
appCustMsrcSeekTo(
	NDKStMediaSrcHandle h,
	UINT32 position
)
{
	appCustMsrcCtx_t *c = (appCustMsrcCtx_t *)h;
	UINT32 ret = FAIL;

	if (!c->bAbort&& gPlayabort == 0) {
		gfseek =1;
		profLogPrintf(0, "seek ++ %d", position);
		ret = sp5kMediaPlayControl(SP5K_MEDIA_PLAY_SEEK, position, 0);
		profLogPrintf(0, "seek -- %d", position);
		//sp5kMediaPlayControl(SP5K_MEDIA_PLAY_RESUME, 0, 0);
		gfseek =0;

		c->audStartPTS = position;
		c->audTotalBytes = 0;

		c->vidStartPTS = position;
		c->vidFrmCnt = 0;

		printf("seek end %u ......\n", position);
		ret = SUCCESS;
	}

	return ret;
}

static UINT32
appCustMsrcGetAttr(
	NDKStMediaSrcHandle h,
	UINT32 attr,
	UINT32 *val
)
{
	UINT32 ret = SUCCESS;

	switch (attr) {
		case SP5K_MEDIA_ATTR_WIDTH:
			*val = 640;
			break;
		case SP5K_MEDIA_ATTR_HEIGHT:
			*val = 360;
			break;
		case SP5K_MEDIA_ATTR_VIDEO_CODEC:
			*val = SP5K_MEDIA_VIDEO_MJPG;
			break;

		case SP5K_MEDIA_ATTR_DURATION:
		case SP5K_MEDIA_ATTR_VIDEO_FRAME_RATE:
		case SP5K_MEDIA_ATTR_AUDIO_SAMPLE_BITS:
		case SP5K_MEDIA_ATTR_AUDIO_SAMPLE_RATE:
		case SP5K_MEDIA_ATTR_AUDIO_CHANNELS:
		case SP5K_MEDIA_ATTR_AUDIO_CODEC:
			/* IMPORTANT: wait here if PB mode is not ready */
			sp5kModeWait(SP5K_MODE_VIDEO_PLAYBACK);
			sp5kMediaPlayAttrGet(attr, val);

			if (attr == SP5K_MEDIA_ATTR_AUDIO_CODEC) {
				printf("\nSP5K_MEDIA_ATTR_AUDIO_CODEC val=%d\n", *val);
			}
			break;
		default:
			printf("unknown attr %x required\n", attr);
			ret = FAIL;
			break;
	}

	return  ret;
}

static void
appCustMsrcFreeBufObj(
	NDKStMediaBufferObject obj
)
{
	appCustMsrcBuf_t *cbuf = (appCustMsrcBuf_t *)obj;

	if (cbuf) {
		if (cbuf->bAudio) {
			sp5kFree(cbuf->data);
		}
		else {
			sp5kFree(cbuf->data);
		}

		sp5kFree(cbuf);
	}
}
static NDKStMediaSrcCallback g_msrc_cb;
static void
appCustMsrcInit(
	void
)
{
	g_msrc_cb.open = appCustMsrcOpen;
	g_msrc_cb.close = appCustMsrcClose;
	g_msrc_cb.pause = appCustMsrcPause;
	g_msrc_cb.resume = appCustMsrcResume;
	g_msrc_cb.end_of_source = appCustMsrcEndOfSrc;
	g_msrc_cb.seek_to = appCustMsrcSeekTo;
	g_msrc_cb.get_attribute = appCustMsrcGetAttr;
	g_msrc_cb.free_buffer_object = appCustMsrcFreeBufObj;

	ndk_st_register_mediasrc(NDK_ST_MEDIASRC_FILE, &g_msrc_cb);
}

/*---------------------------------STA function-------------------------------------------*/

# if WIFISTA
static void appSTAServerStop(void)
{
	char ifname[32] = { 0 };
	strcpy(ifname, 	"wlan0");
 
	ndk_dhcp_stop(ifname);
	ndk_wpas_stop_daemon();
}

 


static void app_sysevt_handler(NDKSysEvt evt, unsigned long param, unsigned long udata)
{
	switch (evt) {
	case NDK_SYSEVT_HAPD_ASSOC:
		printf("ASSOC: %s\n", eth_addr_ntoa((unsigned char*)param));
		memcpy(gsta_hwaddr, (void *)param, 6);
		break;
 
	case NDK_SYSEVT_HAPD_DISASSOC:
		printf("DIS-ASSOC: %s\n", eth_addr_ntoa((unsigned char*)param));
		break;
 
	case NDK_SYSEVT_DHCPD_ALLOC_ADDR:
		//printf("dhcpd alloc: %s\n", ipaddr_ntoa((ip_addr_t*)param));
		break;
 
	case NDK_SYSEVT_STA_CONNECTED:
		printf("Sta: connected %s\n", param ? (const char*)param : "");
                if(isStaConnected == 0xFFFFFFFF || isStaConnected == 0)
		{
			isStaConnected = 1;
		}
		break;
 
	case NDK_SYSEVT_STA_DISCONNECTED:
		printf("Sta: disconnected %s\n", param ? (const char*)param : "");
                if(isStaConnected == 1)
		{
			isStaConnected = 0;
		}
		break;
 
	case NDK_SYSEVT_DHCP_BOUND: {
		printf("DHCP: bound %s\n", param ? (const char*)param : "");
		break; }
 
	default:
		printf("event %u, %lu\n", evt, param);
		break;
	}
 
	if (gnet_sysevt)
		sp5kOsEventFlagsSet(&gnet_sysevt, 1 << evt, TX_OR);
}
 
 static void app_sysevt_register_handler(unsigned long udata)
 {
	 if (!gnet_sysevt) {
		 sp5kOsEventFlagsCreate(&gnet_sysevt, "netsysevt");
	 }
  
	 ndk_sysevt_handler_set(app_sysevt_handler, udata);
 }

 static void app_sysevt_clear(UINT32 evt)
 {
	 sp5kOsEventFlagsSet(&gnet_sysevt, ~(1 << evt), TX_AND);
 }
  
 static BOOL app_sysevt_wait(UINT32 evt, UINT32 secs)
 {
	 if (gnet_sysevt) {
		 UINT32 cur = 0;
		 if (sp5kOsEventFlagsGet(&gnet_sysevt, 1 << evt, TX_OR_CLEAR, &cur, 1000 * secs) == SUCCESS) {
			 if (cur & (1 << evt))
				 return TRUE;
		 }
	 }
  
	 return FALSE;
 }

 void app_station_mode(void)
 {
  
	 char ifname[32] = { 0 };
	 char ssid[32]	 = { 0 };
	 char psk[32]	 = { 0 };
	 int res, priority = 1;
	 char high_speed_sdio = 1;
  
  
	 strcpy(ifname,  "wlan0");
	 strcpy(ssid,	 "rev_wap");
	 strcpy(psk,	 "1234567890");
  
  
  
  
	 ndk_getopt_reset(FALSE);
  
  
	 if (!ssid[0]) {
		 printf("No SSID is provided\n");
		 return;
	 }
  
	 //ndk_global_init(priority);
        #if SP5K_NDK2  //NDK2 driver need for V35
	 {
		 NDKInitOpt opts[] = {
		 {NDK_INIT_OPT_PRIORITY, 20},
		 };
		 ndk_global_init(OPT_ARRAY_SIZE(opts), opts);
	 }
	#else
		 ndk_global_init(priority);
	#endif
  
	 app_sysevt_register_handler(0);
  
	 printf("\n= STA Mode on %s. Join '%s'\n", ifname, ssid);
	 ndk_netif_ioctl(NDK_IOCS_IF_UP, (long)ifname, NULL);
  
	 char *av[] = { "-Cwpas_conf", "-Dwext", "-i", (char*)ifname };
	 res = ndk_wpas_start_daemon(4, av);
	 app_sysevt_clear(NDK_SYSEVT_STA_CONNECTED);
  
	 NDKWpasOpt opts_psk[] = {
		 {"proto", "WPA RSN"},
		 {"key_mgmt", "WPA-PSK"},
		 {"pairwise", "CCMP TKIP"},
		 {"group", "CCMP TKIP WEP104 WEP40"},
		 {"*psk", psk},
		 {NULL, NULL}
	 };
  
	 NDKWpasOpt opts_none[] = {
		 {"key_mgmt", "NONE"},
		 {NULL, NULL}
	 };
  
	 NDKWpasOpt *opts = psk[0] ? opts_psk : opts_none;
  
	 res = ndk_wpas_add_network(ssid, opts);
	 if (res < 0) {
		 printf("add network err(%d)\n", res);
		 return;
	 }
  
	 res = ndk_wpas_enable_network(ssid, TRUE);
	 if (res < 0) {
		 printf("enable network err = %d\n", res);
		 return;
	 }
  
	 if (!app_sysevt_wait(NDK_SYSEVT_STA_CONNECTED, 100)) {
		 printf("Connect failed\n");
				 isStaConnected = 0;
		 return;
	 }
  
	 app_sysevt_clear(NDK_SYSEVT_DHCP_BOUND);
  
	 ndk_netif_set_address_s(ifname, "0.0.0.0", "255.255.255.0", NULL);
	 ndk_dhcp_start(ifname);
  
	 if (!app_sysevt_wait(NDK_SYSEVT_DHCP_BOUND, 100)) {
				 isStaConnected = 0;
		 printf("DHCP bound failed\n");
	 }
  
 }


 void app_apmode_exit()
 {
	 appWiFiStopConnection(HOST_AP|DHCP_SERVER);
 }


 int appScanBestChannel(
    UINT8 *ssidname)
{

    /*
       if ssidname != 0 , check essid exit or not for STA mode
          ret = 1 exit , others not exit
       if ssidname == 0 , find the best channel for AP mode
         ret = -1 not find , others is channel
    */

    int ret = -1 ;
    int bestchannel = 11 ;
    UINT8 ifname[IF_NAMESIZE];

    if ( ssidname == 0 ) {
        NDKWifiChanArray24G ca;
        void *pca = (void *)&ca ;
        long ch = -1 ;

        if (ndk_netif_ioctl(NDK_IOCG_IF_INDEXTONAME, 1, (long *)ifname) != 0) {
            printf("ERROR:indextoname\n");
        } else {
            memset(ifname,0,IF_NAMESIZE);
            snprintf((char *) ifname, IF_NAMESIZE , "%s", "wlan0");
        }

        ndk_netif_ioctl(NDK_IOCG_WIFI_ACS_24G, (long)ifname, &ch);

        if (ndk_netif_ioctl(NDK_IOCG_WIFI_BEST_CHAN_24G, (long)ifname, (long *)pca) == 0) {
            if ( ca.num < 11 ) {
                bestchannel = 1 ;
                if ( ca.channels[5] < ca.channels[0] ) {
                    bestchannel = 6 ;
                }
            } else {
                if ( ca.channels[5] < ca.channels[10] ) {
                    bestchannel = 6 ;
                }
                if ( ca.channels[0] < ca.channels[bestchannel-1] ) {
                    bestchannel = 1 ;
                }
            }

            ret = bestchannel ;
            /* debug print */
            #if 0
            int i;
            printf("==============>>>> bestchannel:%d\n", bestchannel);
            printf("==============>>>> ca.num:%d\n", ca.num);
            for (i = 0; i < ca.num && i < NDK_ARRAY_SIZE(ca.channels); ++i) {
                printf("==============>>>> ca->channels[%d]:%d\n", i , ca.channels[i]);
            }
            #endif
        }
    } else {
        /* toDO: */
    }
    return ret ;
}

#endif
