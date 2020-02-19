#ifndef __NORDIC_UART_H__
#define __NORDIC_UART_H__

#include "frame_define.h"

#include "FreeRTOS.h"
#include "queue.h"

#define NORDIC_UART HAL_UART_2

typedef enum NORDIC_UART_MSG
{
    NORDIC_UART_READY_TO_READ = 1,
    NORDIC_UART_READY_TO_WRITE = 2,

    NET_CLOSE_RESPONSE = 9,
    NET_CONNECT_RESPONSE = 10,
    SIM_STATE_RESPONSE = 11,
    CSQ_RESPONSE = 12,
    GPS_DATA_RESPONSE = 13,
    SWITCH_NET_RESPONSE = 14,
    CURRENT_NET_RESPONSE = 15,
    SDK_VER_RESPONSE = 16,
    AGPS_STATE_RESPONSE = 17,
    RFCAL_FLAG_RESPONSE = 18,
    IMEI_STATE_RESPONSE = 19,
    NB_STATE_RESPONSE	= 20,
    GSV_DATA_RESPONSE = 21,

    NDC_UART_MSG_GET_SERV_IP           = CMD_GET_SERV_IP,
    NDC_UART_MSG_DEV_UPDATE_TIME       = CMD_DEV_UPDATE_TIME,
    NDC_UART_MSG_DEV_VERS_UPDATE       = CMD_DEV_VERS_UPDATE,
    NDC_UART_MSG_GET_QRCODE            = CMD_GET_QRCODE,
    NDC_UART_MSG_DEV_PARA_INIT         = CMD_DEV_PARA_INIT,

	NDC_UART_MSG_NB_GSV					= CMD_NB_GSV,
	NDC_UART_MSG_NB_STATE				= CMD_NB_STATE,
	NDC_UART_MSG_NB_WRITE_AGPS			= CMD_NB_WRITE_AGPS,
	NDC_UART_MSG_NB_RFCAL				= CMD_NB_RFCAL,
	NDC_UART_MSG_NB_GPS					= CMD_NB_GPS,
    NDC_UART_MSG_NB_VERS               = CMD_NB_VERS,
    NDC_UART_MSG_NB_IMEI_SN            = CMD_NB_IMEI_SN,
    NDC_UART_MSG_NB_NET_STATE          = CMD_NB_NET_STATE,
    NDC_UART_MSG_DEV_CCID              = CMD_DEV_CCID,
    NDC_UART_MSG_DEV_IMSI				= CMD_DEV_IMSI,
}NORDIC_UART_MSG_E;

typedef enum
{
    NB_STATE_GET,
    NB_STATE_SET,
}nb_state_set_or_get_e;

typedef enum
{
    CLOSE_NB,
    CLOSE_GSM,
    ENTER_DUAL_MODE,
    CLOSE_UBLOX,
    OPEN_UBLOX,
}nb_state_type_e;

typedef struct ndc_uart_que_msg
{
    NORDIC_UART_MSG_E message_id;
    int state;
    int para_len;
    void *param;
}ndc_uart_que_msg_t;

typedef struct
{
    int msg_id;
    int data_len;
    void *param;
}socket_recv_queue_msg_t;

typedef struct
{
	int msg_id;
	void* param;
}nb_msg_t;


#define MAX_GPS_NUM 10
typedef struct
{
    int num;
    gps_data gpsData[10];//warning
    uint32_t timeStamp[10]; //no use
}gps_data_t;


typedef struct
{
    bool is_frame_completed;
    char message_buffer[1024];
    uint16 index; //!can not uint8
}frame_t;

#define MAX_LAC_NUM 10
typedef struct 
{
    uint8 cnt;// lac cnt
    lac_data lacData[MAX_LAC_NUM];
}cell_data_t;


//-------------------------------------------
#pragma pack(0x1)
typedef struct 
{
	char nb_ver[5+1];
	char iccid[20+1];
	char imsi[15+1];
	nordic_get_imei_response_t sn_imei;
	uint32_t csq;
	uint8_t nb_net_state;//current net
	uint8_t write_agps_state;
	rfcal_flag_response_t rfcal;
	volatile sg_gps_t gps;
	gsv_rsp_t gsv;
	
	double time;
	char server_ip[64+1];
	char token[32+1];
	
}nb_local_data_t;
#pragma pack()
//============================================
extern QueueHandle_t ndc_uart_task_que;
extern const char *g_nb_sdk_ver;

extern nb_local_data_t g_nb_local_data;
//============================================
void nordic_uart_task_init(void);

#endif

