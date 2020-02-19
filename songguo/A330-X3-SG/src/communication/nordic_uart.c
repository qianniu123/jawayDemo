
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "type_def.h"
/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "task_def.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

/* device.h includes */
#include "mt2625.h"

/* hal includes */
#include "hal.h"
#include "hal_uart.h"
#include "hal_gpt.h"
#include "hal_platform.h"
#include "hal_pinmux_define.h"

#include "ril.h"
#include "ril_gsm.h"
#include "inet.h"
#include "auto_register.h"
#include "modem_switch.h"
#include "ril_internal_use.h"

#include "nordic_uart.h"
#include "frame_define.h"
#include "utility.h"
#include "nb_http.h" //agps
#include "nb_rfcal.h"
#include "ringbuffer.h"

#include "nb_modem_monitor.h"
#include "cJSON.h"
#include "time.h"

//============log====================================================================================
#define NORDIC_UART_LOG_ENABLE 1
log_create_module(nordic_uart, PRINT_LEVEL_INFO);
#if NORDIC_UART_LOG_ENABLE
#define NORDIC_UART_LOGI(fmt, args...)               LOG_I(nordic_uart, "[NORDIC_UART]: "fmt, ##args)
#define NORDIC_UART_LOGW(fmt, args...)               LOG_W(nordic_uart, "[NORDIC_UART]: "fmt, ##args)
#define NORDIC_UART_LOGE(fmt, args...)               LOG_E(nordic_uart, "[NORDIC_UART]: "fmt, ##args)
#else
#define NORDIC_UART_LOGI(fmt, args...)
#define NORDIC_UART_LOGW(fmt, args...)
#define NORDIC_UART_LOGE(fmt, args...)
#endif

//===================================================

#define NORDIC_DMA_RX_VFIFO_BUFFER_SIZE 2048 //1024
#define NORDIC_DMA_TX_VFIFO_BUFFER_SIZE 2048 //1024

#define NORDIC_DMA_RX_BUFFER_SIZE 2048 //1024
#define NORDIC_DMA_TX_BUFFER_SZIE 2048 //1024

#define NORDIC_UART_QUEUE_SIZE 30
#define NB_RECV_SOCKET_QUEUE_SIZE 10
//----------------------------------
//extern const char HAL_UART2_RXD_PIN_M_UART2_RXD;
//extern const char HAL_UART2_TXD_PIN_M_UART2_TXD;

#ifdef NB_SOCKET
    extern void nb_socket_send(void *data, uint32_t len);
    extern void nb_socket_close(void);
#endif

#ifdef NB_HTTP
extern QueueHandle_t nb_http_task_queue;
#endif

#ifdef MTK_TELECOM_CTWING_SUPPORT
	extern void sg_send_data(void *msg, int len);
#endif
//-----------------------------------------------------------------------------------------------------------------------

ATTR_ZIDATA_IN_NONCACHED_RAM_4BYTE_ALIGN static uint8_t nordic_uart_dma_rx_vfifo_buffer[NORDIC_DMA_RX_VFIFO_BUFFER_SIZE];
ATTR_ZIDATA_IN_NONCACHED_RAM_4BYTE_ALIGN static uint8_t nordic_uart_dma_tx_vfifo_buffer[NORDIC_DMA_TX_VFIFO_BUFFER_SIZE];

uint8_t nordic_uart_dma_rx_buffer[NORDIC_DMA_RX_BUFFER_SIZE];
uint8_t nordic_uart_dma_tx_buffer[NORDIC_DMA_TX_BUFFER_SZIE];

ring_buffer_t nordic_uart_ring_buffer;


QueueHandle_t ndc_uart_task_que = NULL;
QueueHandle_t nb_recv_socket_que = NULL;

SemaphoreHandle_t mutex_gps = NULL;
SemaphoreHandle_t mutex_lac = NULL;

TimerHandle_t daemon_timer_handle = NULL;

cell_data_t g_lac_data = {0};//global lac data

uint32_t param_timeStamp = 0x5cd90dfd;

//char g_sdk_ver[20] = "NB_V2.6.0_1.6.0";
const char *g_nb_sdk_ver = "V0103";

//gps 定位数据
nordic_get_gps_response_t param_gps_data; //for nordic
nordic_get_gsv_response_t param_gsv_data;

gps_data_t g_gps_data = {0};

frame_t message_frame = {1, {0}, 0};


nb_local_data_t g_nb_local_data = {0};

//=============prototype===========================================================
void socket_recv_msg_process(void *msg, int size);

void nordic_uart_callback(hal_uart_callback_event_t event, void *user_data);

unsigned char sum_cal2(char* array, uint len);

void nb_state_set_and_get(nordic_get_and_set_nb_state_t state);

void parse_pack_send(sg_commut_data_t *pack, int len);

//==================definition========================================================

void nordic_uart_callback(hal_uart_callback_event_t event, void * user_data)
{
    ndc_uart_que_msg_t queue_message;
    BaseType_t xHigherPriorityTaskWoken;

    if(event == HAL_UART_EVENT_READY_TO_READ)
    {
        queue_message.message_id = NORDIC_UART_READY_TO_READ;
        xQueueSendFromISR(ndc_uart_task_que, &queue_message, &xHigherPriorityTaskWoken);
    }
    else if(event == HAL_UART_EVENT_READY_TO_WRITE)
    {
        queue_message.message_id = NORDIC_UART_READY_TO_WRITE;
    }
    else if(event == HAL_UART_EVENT_TRANSACTION_ERROR)
    {	/*
        queue_message.message_id = NORDIC_UART_READY_TO_READ;
        xQueueSendFromISR(ndc_uart_task_que, &queue_message, &xHigherPriorityTaskWoken);
        */
    }

}

//先把uart2 AT命令功能关掉
/* //nb_custom_port_config.h
#if defined (__NB_IOT_GSM_CCCI__)
#define DEFAULT_PORT_FOR_AT_CMD_2     SERIAL_PORT_DEV_UART_2
#else
#define DEFAULT_PORT_FOR_AT_CMD_2     SERIAL_PORT_DEV_UNDEFINED
#endif
*/
//#include "nb_custom_port_config.h"

void nordic_uart_pinmux_init(void)
{
    hal_gpio_init(HAL_GPIO_3);
    hal_pinmux_set_function(HAL_GPIO_3, HAL_GPIO_3_UART2_RXD);//HAL_UART2_RXD_PIN_M_UART2_RXD  //HAL_GPIO_3_UART2_RXD
    hal_gpio_init(HAL_GPIO_4);
    hal_pinmux_set_function(HAL_GPIO_4, HAL_GPIO_4_UART2_TXD);//HAL_UART2_TXD_PIN_M_UART2_TXD  //HAL_GPIO_4_UART2_TXD
}

void nordic_uart_init(void)
{
    hal_uart_config_t uart_config;
    hal_uart_dma_config_t dma_config;

    hal_uart_register_callback(NORDIC_UART, NULL, NULL);
    hal_uart_deinit(NORDIC_UART);

    nordic_uart_pinmux_init();

    uart_config.baudrate = HAL_UART_BAUDRATE_115200;
    uart_config.parity = HAL_UART_PARITY_NONE;
    uart_config.stop_bit = HAL_UART_STOP_BIT_1;
    uart_config.word_length = HAL_UART_WORD_LENGTH_8;
    hal_uart_init(NORDIC_UART, &uart_config);

    dma_config.receive_vfifo_alert_size = 50;
    dma_config.receive_vfifo_buffer	= nordic_uart_dma_rx_vfifo_buffer;
    dma_config.receive_vfifo_buffer_size = NORDIC_DMA_RX_VFIFO_BUFFER_SIZE;
    dma_config.receive_vfifo_threshold_size = 1024;//512;
    dma_config.send_vfifo_buffer = nordic_uart_dma_tx_vfifo_buffer;
    dma_config.send_vfifo_buffer_size = NORDIC_DMA_TX_VFIFO_BUFFER_SIZE;
    dma_config.send_vfifo_threshold_size = 512;//128
    hal_uart_set_dma(NORDIC_UART, &dma_config);
    hal_uart_register_callback(NORDIC_UART, nordic_uart_callback, NULL);


}

void nordic_eint_io_init(void)//io interrupt
{
    hal_gpio_init(HAL_GPIO_21);
    hal_pinmux_set_function(HAL_GPIO_21, 0);
    //hal_gpio_disable_pull(HAL_GPIO_21);
    hal_gpio_set_direction(HAL_GPIO_21,HAL_GPIO_DIRECTION_OUTPUT);

    //hal_gpio_set_output(HAL_GPIO_21, HAL_GPIO_DATA_LOW);
    hal_gpio_set_output(HAL_GPIO_21, HAL_GPIO_DATA_HIGH);
}

//respond data to nordic
static void resp_data_nb2ndc(ndc_uart_que_msg_t* msg)
{
    uint8_t resp_buf[1024] = {0};
    sg_commut_data_t *p_packet = (sg_commut_data_t*)resp_buf;

	http_que_msg_t http_queue_msg = {0};
	
    NORDIC_UART_LOGI("=====>>resp_data_nb2ndc cmd:%X, %d",msg->message_id, msg->message_id);

    switch(msg->message_id)
    {
        /*case NDC_UART_MSG_GET_SERV_IP:
            p_packet->cmd = ntohl(NDC_UART_MSG_GET_SERV_IP);
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_GET_SERV_IP");
        break;       
        case NDC_UART_MSG_DEV_VERS_UPDATE:
            p_packet->cmd = ntohl(NDC_UART_MSG_DEV_VERS_UPDATE);
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_DEV_VERS_UPDATE");
        break;
        case NDC_UART_MSG_GET_QRCODE:
            p_packet->cmd = ntohl(NDC_UART_MSG_GET_QRCODE);
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_GET_QRCODE");
        break;
        case NDC_UART_MSG_DEV_PARA_INIT:
            p_packet->cmd = ntohl(NDC_UART_MSG_DEV_PARA_INIT);
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_DEV_PARA_INIT");
        break;*/
        //---------------------------------------------------------------
        case NDC_UART_MSG_DEV_UPDATE_TIME:
            p_packet->cmd = (NDC_UART_MSG_DEV_UPDATE_TIME);
			p_packet->packet_len = sizeof(g_nb_local_data.time);
            memcpy(p_packet->data, &g_nb_local_data.time, p_packet->packet_len);
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_DEV_UPDATE_TIME");
        break;
        //---------------------------------------------------------------
        case NDC_UART_MSG_DEV_CCID:
        	get_ccid_and_response();
        	get_csq_and_response();

        	ccid_csq_rsp_t rsp={0};
        	memcpy(rsp.ccid, g_nb_local_data.iccid, 20);
        	rsp.csq = g_nb_local_data.csq;
        	
            p_packet->cmd = (NDC_UART_MSG_DEV_CCID);
            p_packet->packet_len = sizeof(rsp); 
            memcpy(p_packet->data, &rsp, p_packet->packet_len);
            
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_DEV_CCID; ICCID=%s\n", g_nb_local_data.iccid);
            break;
       case NDC_UART_MSG_DEV_IMSI:
			get_imsi();

			p_packet->cmd = (NDC_UART_MSG_DEV_IMSI);
			p_packet->packet_len = strlen(g_nb_local_data.imsi);
			memcpy(p_packet->data, g_nb_local_data.imsi, p_packet->packet_len);

			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_DEV_IMSI; IMSI=%s\n", g_nb_local_data.imsi);
       		break;
       case NDC_UART_MSG_NB_NET_STATE:
            p_packet->cmd = (NDC_UART_MSG_NB_NET_STATE);
            p_packet->packet_len = sizeof(g_nb_local_data.nb_net_state);
            memcpy(p_packet->data, (char*)&g_nb_local_data.nb_net_state, p_packet->packet_len);
            
            NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_NET_STATE; nb_net_state=%d\n", g_nb_local_data.nb_net_state);
            break;
        case NDC_UART_MSG_NB_VERS:
             p_packet->cmd = (NDC_UART_MSG_NB_VERS);
             p_packet->packet_len = 5;
             memcpy(p_packet->data, g_nb_local_data.nb_ver, p_packet->packet_len);
             NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_VERS; nb_ver=%s\n", g_nb_local_data.nb_ver);
             break;
         case NDC_UART_MSG_NB_WRITE_AGPS:
         {
			if(nb_http_task_queue)
			{
				http_queue_msg.message_id = HTTP_MSG_AGPS_ONLINE;
				xQueueSend(nb_http_task_queue, &http_queue_msg, portMAX_DELAY);
			}
         
			p_packet->cmd = (NDC_UART_MSG_NB_WRITE_AGPS);
			p_packet->packet_len = sizeof(g_nb_local_data.write_agps_state);
			memcpy(p_packet->data, (char*)&g_nb_local_data.write_agps_state, p_packet->packet_len);
			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_WRITE_AGPS");
         	break;
         }
         case NDC_UART_MSG_NB_GPS:
         {
			p_packet->cmd = (NDC_UART_MSG_NB_GPS);
			p_packet->packet_len = sizeof(g_nb_local_data.gps);
			memcpy(p_packet->data, (char*)&g_nb_local_data.gps, p_packet->packet_len);
			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_GPS; logitude=%s, latitude=%s\n", g_nb_local_data.gps.longitude, g_nb_local_data.gps.latitude);
         	break;
         }
         case NDC_UART_MSG_NB_GSV:
         {				       
         	p_packet->cmd = (NDC_UART_MSG_NB_GSV);
			p_packet->packet_len = sizeof(gsv_rsp_t);
			
			memcpy(p_packet->data, (char*)&g_nb_local_data.gsv, sizeof(gsv_rsp_t));
			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_GSV; states_in_view = %d\n", g_nb_local_data.gsv.gsv_states_in_view);
		 }
         break;
         case NDC_UART_MSG_NB_RFCAL:
			get_rfcal_flag();
         
			p_packet->cmd = (NDC_UART_MSG_NB_RFCAL);
			p_packet->packet_len = sizeof(g_nb_local_data.rfcal);
			memcpy(p_packet->data, (char*)&g_nb_local_data.rfcal, p_packet->packet_len);
			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_RFCAL; nb_flag=%d, gsm_flag=%d", g_nb_local_data.rfcal.nb_flag, g_nb_local_data.rfcal.gsm_flag);
         	break;
         
         case NDC_UART_MSG_NB_IMEI_SN:
			get_imei_sn_and_response();
         
			p_packet->cmd = (NDC_UART_MSG_NB_IMEI_SN);
			p_packet->packet_len = sizeof(g_nb_local_data.sn_imei);
			memcpy(p_packet->data, (char*)&g_nb_local_data.sn_imei, p_packet->packet_len);
			NORDIC_UART_LOGI("resp to nordc NDC_UART_MSG_NB_IMEI_SN; imei=%s; sn=%s;", g_nb_local_data.sn_imei.imei, g_nb_local_data.sn_imei.sn);
         	break;     
        default:
            goto error_msg;
        break;
    }

    p_packet->protocol_ver = SG_PROTOCOL_VER;
    p_packet->enryption_type = SG_ENCRYPTION_TYPE;
    p_packet->status = 0;
    memset(p_packet->token, 0, sizeof(p_packet->token));

    /*if(msg->para_len != 0) {
        p_packet->packet_len = msg->para_len;
        memcpy(p_packet->data, msg->param, p_packet->packet_len);
    }*/

    p_packet->data[p_packet->packet_len] = SG_PACKET_END1;
    p_packet->data[p_packet->packet_len + 1] = SG_PACKET_END2;
    p_packet->data[p_packet->packet_len + 2] = SG_PACKET_END3;
    p_packet->data[p_packet->packet_len + 3] = SG_PACKET_END4;
    p_packet->data[p_packet->packet_len + 4] = SG_PACKET_END5;

    hal_uart_send_dma(NORDIC_UART, resp_buf, (sizeof(sg_commut_data_t) + p_packet->packet_len + 5));
    //for(int i = 0; i < (sizeof(sg_commut_data_t) + p_packet->packet_len + 5); i++) {
    //    NORDIC_UART_LOGI("p_packet:%02x", resp_buf[i]);
    //}

    return;

error_msg:
{
    NORDIC_UART_LOGI("===>>>>recv cmd from nb is error!");
}

}


void rw_ndc_uart_data_task(void* param)
{
    ndc_uart_que_msg_t queue_message;
    uint32_t recv_cnt;
    uint32_t available_cnt;

    char buffer[1024*6] = {0};
    memset(buffer, 0, sizeof(buffer));
    //-------------------------------------------------------
    nb_uart_printf(NORDIC_UART, "rw_ndc_uart_data_task start\n");

    while(1)
    {
        if(xQueueReceive(ndc_uart_task_que, &queue_message, portMAX_DELAY))
        {
            //set_sleep_lock()
            if(queue_message.message_id == NORDIC_UART_READY_TO_READ)
            {
                available_cnt = hal_uart_get_available_receive_bytes(NORDIC_UART);
                if(available_cnt > sizeof(buffer))
                {
                    available_cnt = sizeof(buffer);
                }
                memset(buffer, 0, sizeof(buffer));
                recv_cnt = hal_uart_receive_dma(NORDIC_UART, buffer, available_cnt);
                ///hal_uart_send_dma(NORDIC_UART, buffer, recv_cnt);//for test
                ring_buffer_queue_arr(&nordic_uart_ring_buffer, buffer, recv_cnt);
                //------------------------------------------------------------------
                ring_buffer_size_t size = ring_buffer_num_items(&nordic_uart_ring_buffer);
                if(size > sizeof(buffer))
                {
                    size = sizeof(buffer);
                }
                if(size >= 46)
                {
                    memset(buffer, 0, sizeof(buffer));
                    ring_buffer_peek_arr(&nordic_uart_ring_buffer, buffer, size);

                    NORDIC_UART_LOGI("%s : recv frame length = %d", __FUNCTION__, size);
                    //hal_uart_send_dma(NORDIC_UART, buffer, size);//for test
                    proc_ndc_uart_msg(buffer, size);
                }

            }
            else //write to uart
            {
                NORDIC_UART_LOGI("-------------will respond to nordic ------------");
                resp_data_nb2ndc(&queue_message);
            }
            //realse_sleep_lock()
        }

    }

}

void nb_recv_socket_task(void *param)
{
    char socket_recv_buff[1024] = {0};
    int recv_buff_len = 0;
    socket_recv_queue_msg_t socket_recv_queue;

    nb_recv_socket_que = xQueueCreate(NB_RECV_SOCKET_QUEUE_SIZE, sizeof(socket_recv_queue_msg_t));
    if(nb_recv_socket_que == NULL)
    {
        NORDIC_UART_LOGE("create nb_recv_socket_task error\n");
    }

    while(1)
    {
        if(xQueueReceive(nb_recv_socket_que, &socket_recv_queue, portMAX_DELAY))
        {
            memset(socket_recv_buff, 0, sizeof(socket_recv_buff));
            memcpy(socket_recv_buff, (char*)socket_recv_queue.param, socket_recv_queue.data_len);
            recv_buff_len = socket_recv_queue.data_len;
            vPortFree(socket_recv_queue.param);//(nb_socket_recv_task_processing pvPortMalloc)

            socket_recv_msg_process(socket_recv_buff, recv_buff_len);
            //vPortFree(socket_recv_queue.param);
        }
    }
}

void nordic_uart_task_init(void)
{
    nordic_uart_init();
    nordic_eint_io_init();
	//-----------------------------------------------------------------
	memset((char*)&g_nb_local_data, 0x0, sizeof(nb_local_data_t));
	memcpy(g_nb_local_data.nb_ver, g_nb_sdk_ver, strlen(g_nb_sdk_ver));

    ndc_uart_task_que = xQueueCreate(NORDIC_UART_QUEUE_SIZE, sizeof(ndc_uart_que_msg_t));
    if(ndc_uart_task_que == NULL)
    {
        NORDIC_UART_LOGE("create ndc_uart_task_que error\n");
    }

    mutex_gps = xSemaphoreCreateMutex();
    if(mutex_gps == NULL)
    {
        NORDIC_UART_LOGE("create mutex_gps error\n");
    }
    mutex_lac = xSemaphoreCreateMutex();
    if(mutex_lac == NULL)
    {
        NORDIC_UART_LOGE("create mutex_lac error\n");
    }

    if(pdPASS != xTaskCreate(rw_ndc_uart_data_task, NORDIC_UART_TASK_NAME, NORDIC_UART_TASK_STACK_SIZE, NULL, NORDIC_UART_TASK_PRIO, NULL))
    {
        NORDIC_UART_LOGE("create rw_ndc_uart_data_task fail!");
    }

    if(pdPASS  != xTaskCreate(nb_recv_socket_task, NB_RECV_SOCKET_TASK_NAME, NB_RECV_SOCKET_TASK_STACK_SIZE, NULL, NB_RECV_SOCKET_TASK_PRIO,NULL))
    {
        NORDIC_UART_LOGE("create nb_recv_socket_task fail!");
    }

}

static int sg_veify_frame_data(const char *buf, uint16 len)
{
    int i = 0;
    sg_commut_data_t *p_packet = NULL;
    int packet_len = 0;

    for(i=0; i < len; i ++) {
        if(buf[i] == SG_PROTOCOL_VER) {
            /* find frame head */
            p_packet = (sg_commut_data_t *)&buf[i];
            packet_len = (p_packet->packet_len);

            if(packet_len + sizeof(sg_commut_data_t) == (len - i - 5)) {
                /* recv data len is right */
                NORDIC_UART_LOGI("recv data len is right, data len:%d", packet_len);

                if((p_packet->data[packet_len] == SG_PACKET_END1)
                    && (p_packet->data[packet_len + 1] == SG_PACKET_END2)
                    && (p_packet->data[packet_len + 2] == SG_PACKET_END3)
                    && (p_packet->data[packet_len + 3] == SG_PACKET_END4)) {
                    NORDIC_UART_LOGI("veify frame data success, data index:%d", i);
                    return i;
               } else {
                    return NB_RECV_ERROR;
               }
            } else if((packet_len + sizeof(sg_commut_data_t) > len)
                && packet_len + sizeof(sg_commut_data_t) < 1024) {
                /* one frame not recv compete */
                NORDIC_UART_LOGI("veify NB_RECV_WAIT");
                return NB_RECV_WAIT;
            } else {
                /* find next frame head */
                NORDIC_UART_LOGI("veify continue");
                continue;
            }
        }

		if(i == len - 1)
		{
            return NB_RECV_ERROR;
        }
    }

    return NB_RECV_WAIT;

}

static SG_NB_COMMUT_TYPE_E analytical_data(sg_commut_data_t *p_packet, SG_COMMUT_CMD_E *msg_id)
{
    SG_NB_COMMUT_TYPE_E status = NB_COMMUT_TYPE_UNKNOW;

    switch(p_packet->cmd) 
    {
        case DEVICE_HEARTBEAT:
        case DEVICE_SOFTWARE_VERSION_QUERY:
        case DEVICE_HELLO:
        case DEVICE_BIND:
        case DEVICE_UNBIND:
        case USER_INFO_GET:
        case HEART_UPLOAD:
        case LOCATION_UPLOAD:
        {
            NORDIC_UART_LOGI("recv ndc data:  SOCKET CMD =%X", ntohl(p_packet->cmd));
            status = NB_COMMUT_TYPE_SOCKET;
            *msg_id = (p_packet->cmd);
        }
        break;
       //HTTP CMD-------------------------------------------------
       case CMD_GET_SERV_IP:
       case CMD_DEV_UPDATE_TIME:
       case CMD_DEV_VERS_UPDATE:
       case CMD_GET_QRCODE:
       case CMD_DEV_PARA_INIT:
       {
			NORDIC_UART_LOGI("recv ndc data: HTTP CMD = %X", ntohl(p_packet->cmd));
			status = NB_COMMUT_TYPE_HTTP;
			*msg_id = (p_packet->cmd);
       }
       break;
       //LOCAL CMD------------------------------------------------      
       case CMD_NB_STATE:
       case CMD_NB_WRITE_AGPS:
       case CMD_NB_RFCAL:
       case CMD_NB_GPS:
       case CMD_NB_GSV:
       case CMD_NB_VERS:
       case CMD_NB_IMEI_SN:
       case CMD_NB_NET_STATE:
       case CMD_DEV_CCID:
       {
			NORDIC_UART_LOGI("recv ndc data: LOCAL CMD = %X", ntohl(p_packet->cmd));
			status = NB_COMMUT_TYPE_LOCAL;
			*msg_id = (p_packet->cmd);
       }
       break;
       default:
            status = NB_COMMUT_TYPE_UNKNOW;
       break;
    }

    return status;
}

/* recv nordic uart data and send to socket task */
void proc_ndc_uart_msg(void* message, uint16 len)
{
    int status = 0;
    HTTP_QUE_MSG_E msg_id = HTTP_MSG_UNKNOW;
    SG_NB_COMMUT_TYPE_E nb_type = NB_COMMUT_TYPE_UNKNOW;
    
    ndc_uart_que_msg_t ndc_que_msg;
    http_que_msg_t http_queue_msg;

    sg_commut_data_t *p_packet = NULL;

    /*NORDIC_UART_LOGI("==>>recv proc_ndc_uart_msg:");
    for(int i = 0; i < len; i ++) {
        NORDIC_UART_LOGI("%02x", ((char *)message)[i]);
    }
    NORDIC_UART_LOGI("==>>recv proc_ndc_uart_msg end\n");*/

    status = sg_veify_frame_data(message, len);
    if(status >= NB_RECV_SUCCESS) {
        p_packet = (sg_commut_data_t *)(message + status);
        NORDIC_UART_LOGI("veify frame comoete, cmd:%X, start proc data", p_packet->cmd);
    } else if(status == NB_RECV_WAIT) {
        NORDIC_UART_LOGI("veify return");
        return;
    } else {
        NORDIC_UART_LOGI("veify frame data error!");
        goto proc_end;
    }

    nb_type = analytical_data(p_packet, &msg_id);
    if(nb_type == NB_COMMUT_TYPE_SOCKET) 
    {
    	int pack_len = sizeof(sg_commut_data_t) + (p_packet->packet_len) + 5;// head + data + tail

		#if 0
		NORDIC_UART_LOGI("sg_send_data : CMD=%04X", ntohl(p_packet->cmd));
		NORDIC_UART_LOGI("sg_send_data : head_len = %d, data_len = %d\n", sizeof(sg_commut_data_t), ntohl(p_packet->packet_len));
		NORDIC_UART_LOGI("sg_send_data : sg_msg_len = %d", msg_len);
		char *tp = (char*)p_packet;
		for(int i=0; i<len/10; i++)
		{
			NORDIC_UART_LOGI("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", tp[0+10*i], tp[1+10*i],tp[2+10*i],tp[3+10*i],tp[4+10*i],tp[5+10*i],tp[6+10*i],tp[7+10*i],tp[8+10*i],tp[9+10*i]);
		}
		#endif
		
		#ifdef MTK_TELECOM_CTWING_SUPPORT
		{	
			// parse --> repacket --> send	
			sg_send_data( (void*)p_packet, pack_len);	
		}
		#elif defined(NB_SOCKET)
			// parse --> repacket --> send
			parse_pack_send(p_packet, pack_len);
        #else
			NORDIC_UART_LOGI("not def CTWING && NB_SOCKET");
        #endif
        
    } 
    else if(nb_type == NB_COMMUT_TYPE_HTTP) 
    {
        #ifdef NB_HTTP
        if(nb_http_task_queue != NULL) 
        {
            http_queue_msg.message_id = msg_id;
            xQueueSend(nb_http_task_queue, &http_queue_msg, portMAX_DELAY);
        }
        #endif
        goto proc_end;
    }
    else if(nb_type == NB_COMMUT_TYPE_LOCAL) 
    {
        if(ndc_uart_task_que != NULL) 
        {
            ndc_que_msg.message_id = msg_id;
            ndc_que_msg.para_len = 0;
            xQueueSend(ndc_uart_task_que, &ndc_que_msg, portMAX_DELAY);
        }
    } 
    else 
    {
        NORDIC_UART_LOGI("veify cmd data error!");
        goto proc_end;
    }

proc_end:
{
	char errr_data[1024] = {0};
    ring_buffer_dequeue_arr(&nordic_uart_ring_buffer, errr_data, len);
}

    //if(ring_buffer_num_items(&nordic_uart_ring_buffer) >= 46)//最小的一帧数据有46个字节
    //{
    //    queue_msg.message_id = NORDIC_UART_READY_TO_READ;
    //    if(ndc_uart_task_que)
    //    {
    //        xQueueSend(ndc_uart_task_que, &queue_msg, portMAX_DELAY);
    //    }
   // }

}

int addKeyValue(char *dst, char *key, char *value)
{
	int byte_size = 0;
    char *data = dst;
    int *p_key_len = (int*)data;
    char *p_key = (char*)(p_key_len+1);
    memcpy(p_key, key, strlen(key));
    *p_key_len = htonl(strlen(key));
    byte_size += sizeof(int) + strlen(key);

    int *p_value_len = (int*)(p_key+strlen(key));
    char *p_value = (char*)(p_value_len+1);
    memcpy(p_value, value, strlen(value));
    *p_value_len = htonl(strlen(value));
    byte_size += sizeof(int) + strlen(value);

    return byte_size;
}

void parse_pack_send(sg_commut_data_t *p_packet, int len)
{
	//parse repack and send
	//uint8_t send_buf[1024] = {0}; //reuse the old buffer, no use to create a new buffer
	int pack_data_len = 0;
	uint8_t pack_flag = 1;
	switch(p_packet->cmd)
	{	
		//data部分组成: 00 00 00 03(DATA.KEY.LENGTH)4B 45 59(DATA.KEY) 00 00 00 05(DATA.VALUE.LENGTH) 56 41 4C 55 45(DATA.VALUE)
		// key is str, value is str or json
		case DEVICE_HEARTBEAT:
		{
			//get value
			sg_walk_t *p_req = (sg_walk_t*)p_packet->data;

			NORDIC_UART_LOGI("nordic send DEVICE_HEARTBEAT: count=%d, time=%d\n", p_req->count, p_req->time);
			//repack 
			//"walk" = {"count":1000,"time":1428551739620}
			char *key = "walk";
			int *p_key_length = (int*)p_packet->data;
			char *p_key = (char*)(p_key_length+1);
			*p_key_length = htonl(strlen(key));
			memcpy(p_key, key, strlen(key));

			pack_data_len += sizeof(int) + strlen(key);

			int *p_value_length = p_key + strlen(key);
			char *p_value = (char*)(p_value_length+1);

			//---create json
			char temp_buf[50] = {0};
			sprintf(temp_buf, "{\"count\":%d,\"time\":%d000}", p_req->count, p_req->time);
			*p_value_length = htonl(strlen(temp_buf));
			memcpy(p_value, temp_buf, strlen(temp_buf));
			pack_data_len += sizeof(int) + strlen(temp_buf);
			//send
		}
		break;
		case DEVICE_HELLO:
		{		
			NORDIC_UART_LOGI("nordic send DEVICE_HELLO\n");
			//repack				
			char *key = "data";
			int *p_key_length = (int *)p_packet->data;
			char *p_key = (char*)(p_key_length+1);
			*p_key_length = htonl(strlen(key));
			memcpy(p_key, key, strlen(key));

			pack_data_len += sizeof(int) + strlen(key);
			
			int* p_value_length = p_key + strlen(key);
			
			//{"imei":"891818020411721","imsi":"460042539201301"}
			cJSON *hello = cJSON_CreateObject();
			if(hello)
			{
				cJSON_AddStringToObject(hello, "imei", g_nb_local_data.sn_imei.imei);
				cJSON_AddStringToObject(hello, "imsi", g_nb_local_data.imsi);
				char *out = cJSON_Print(hello);

				*p_value_length = htonl(strlen(out));
				char *p_value = (char*)(p_value_length + 1);
				memcpy(p_value, out, strlen(out));
			
				pack_data_len += sizeof(int) + strlen(out);

				cJSON_free(out);
				cJSON_Delete(hello);
			}
			//send
		}
		break;
		case DEVICE_BIND:
		{
			/* 	key 	value
				userId "123456"
				status '1'
			*/
			char *key1 = "userId";
			char *key2 = "status";

			//get value (little end)
			dev_bind_req_t *req = (dev_bind_req_t*)p_packet->data;
			char userId[10+1] = {0};
			uint16_t status = req->status;
			memcpy(userId, req->userId, sizeof(req->userId));

			NORDIC_UART_LOGI("nordic send DEVICE_BIND: userId=%s, status=%d\n", userId, status);
			//repack (big end)
			int *p_key1_length = (int*)p_packet->data;
			char *p_key1 = (char*)(p_key1_length+1);
			memcpy(p_key1, key1, strlen(key1));
			*p_key1_length = htonl(strlen(key1));	

			pack_data_len += sizeof(int)+strlen(key1);

			int *p_value1_length = (int*)(p_key1 + strlen(key1));
			char *p_value1 = (char*)(p_value1_length+1);
			memcpy(p_value1, userId, strlen(userId));
			*p_value1_length = htonl(strlen(userId));

			pack_data_len += sizeof(int) + strlen(userId);

			int *p_key2_length = (int*)(p_value1 + strlen(userId));
			char *p_key2  = (char*)(p_key2_length+1);
			memcpy(p_key2, key2, strlen(key2));
			*p_key2_length = htonl(strlen(key2));

			pack_data_len += sizeof(int) + strlen(key2);
			
			int *p_value2_length = (int*)(p_key2 + strlen(key2));
			char *p_value2 = (char*)(p_value2_length+1);
			p_value2[0] = (status==1?'1':'0');
			*p_value2_length = htonl(1);
			
			pack_data_len += sizeof(int) + 1;
			//send
		}
		break;
		case DEVICE_UNBIND:
		{
			NORDIC_UART_LOGI("nordic send DEVICE_UNBIND: \n");
			pack_data_len = p_packet->packet_len;
		}
		break;
		case DEVICE_SOFTWARE_VERSION_UPLOAD:
		{
			//data部分组成: 00 00 00 03(DATA.KEY.LENGTH)4B 45 59(DATA.KEY) 00 00 00 05(DATA.VALUE.LENGTH) 56 41 4C 55 45(DATA.VALUE)
			// key is str, value is str or json
			/* 	key 	value
				time 	"1234567890"
				version "v0101v0101"
				imsi	""
			*/
			char *key1 = "time";
			char *key2 = "version";
			char *key3 = "imsi";

			NORDIC_UART_LOGI("nordic send DEVICE_SOFTWARE_VERSION_UPLOAD: \n");
			//parse and get value
			sg_version_t *p_version = (sg_version_t*)p_packet->data;
			int time = p_version->time;
			char version[10+1] = {0};
			memcpy(version, p_version->version, strlen(p_version->version));
			//repack
			//------time = 1581749224000
			int *p_key1_len = (int*)p_packet->data;
			char *p_key1 = (char*)(p_key1_len+1);
			memcpy(p_key1, key1, strlen(key1));
			*p_key1_len = htonl(strlen(key1));
			pack_data_len += sizeof(int)+strlen(key1);

			int *p_value1_len = (int*)(p_key1 + strlen(key1));
			char *p_value1 = (char*)(p_value1_len+1);
			sprintf(p_value1, "%d000", time);
			*p_value1_len = htonl(strlen(p_value1));
			pack_data_len += sizeof(int) + strlen(p_value1);
			//-----version = v0101v0101
			int *p_key2_len = (int*)(p_value1 + strlen(p_value1));
			char *p_key2 = (char*)(p_key2_len + 1);
			memcpy(p_key2, key2, strlen(key2));
			*p_key2_len = htonl(strlen(key2));
			pack_data_len += sizeof(int)+strlen(key2);

			int *p_value2_len = (int*)(p_key2+strlen(key2));
			char *p_value2 = (char*)(p_value2_len+1);
			memcpy(p_value2, version, strlen(version));
			*p_value2_len = htonl(strlen(version));
			pack_data_len += sizeof(int) + strlen(version);
			//-----imsi = "xxxxxxx" //
			int *p_key3_len = (int*)(p_value2 + strlen(version));
			char *p_key3 = (char*)(p_key3_len+1);
			memcpy(p_key3, key3, strlen(key3));
			*p_key3_len = htonl(strlen(key3));
			pack_data_len += sizeof(int) + strlen(key3);

			int *p_value3_len = (int*)(p_key3 + strlen(key3));
			char *p_value3 = (char*)(p_value3_len+1);
			memcpy(p_value3, g_nb_local_data.imsi, strlen(g_nb_local_data.imsi));
			*p_value3_len = htonl(strlen(g_nb_local_data.imsi));
			pack_data_len += sizeof(int) + strlen(g_nb_local_data.imsi);

			// send
		}
		break;
		case HEART_UPLOAD:
		{	
			// key		value
			//"data"  [{"count":76,"time":1428551739620}, {"count":75,"time": 1428551739621}, {"count":78,"time":1428551739622}]

			//parse (little end)
			//------get data from nordic
			uint16_t heart_rate_cnts = (p_packet->packet_len)/sizeof(heart_upload_req_t);
			
			uint8_t *heart_upload_req_buf = (uint8_t*)pvPortMalloc(heart_rate_cnts * sizeof(heart_upload_req_t));//malloc
			if(heart_upload_req_buf != NULL)
			{
				memset(heart_upload_req_buf, 0x0, heart_rate_cnts*sizeof(heart_upload_req_t));
				memcpy(heart_upload_req_buf, p_packet->data, heart_rate_cnts * sizeof(heart_upload_req_t));
			}
			
			NORDIC_UART_LOGI("nordic send HEART_UPLOAD: heart_rate_cnts = %d\n", heart_rate_cnts);
			
			//pack (big end)
			//-------"data"
			int *key_len = (int*)p_packet->data;
			char *key = "data";
			char *p_key = (char*)(key_len+1);
			
			*key_len = htonl(strlen(key));
			memcpy(p_key, key, strlen(key));

			pack_data_len += sizeof(int) + strlen(key);
			//-------json
			int *value_len = (int*)(p_key+strlen(key));
			char *p_value = (char*)(value_len+1);
			
			heart_upload_req_t *p_req = (heart_upload_req_t*)heart_upload_req_buf;
			#if 0
				cJSON *boot_array = cJSON_CreateArray();
				if(boot_array == NULL) 
				{
					goto HEART_UPLOAD_SEND;
				}		
				for(int i=0; i<heart_rate_cnts; i++)
				{
					cJSON *obj = cJSON_CreateObject();
					if(obj)
					{
						//long long temp_time = (long long)(p_req[i].time) * 1000;//this cjson not support longlong data
						int temp_time = p_req[i].time;
						cJSON_AddNumberToObject(obj, "count", p_req[i].count);
						cJSON_AddNumberToObject(obj, "time", temp_time);
						cJSON_AddItemToArray(boot_array, obj);
					}
				}
				char *json_array = cJSON_Print(boot_array);
				cJSON_Delete(boot_array);
			#else
				//[{"count":76,"time":1428551739620}, {"count":75,"time": 1428551739621}, {"count":78,"time":1428551739622}]
				char *json_array = (char*)pvPortMalloc(1024*6); //malloc
				memset(json_array, 0x0, 1024*6);
				//create json
				json_array[0] = '[';
				char *p_obj = &json_array[1];
				for(int i=0; i<heart_rate_cnts; i++)
				{
					char temp_obj[50] = {0}; 
					char temp_time[20] = {0};
        			itoa(p_req[i].time, temp_time, 10);
					sprintf(temp_obj, "{\"count\":%d,\"time\":%s000},", p_req[i].count, temp_time); //%lld not support
					memcpy(p_obj, temp_obj, strlen(temp_obj));
        			p_obj += strlen(temp_obj);
				}
				json_array[strlen(json_array)-1] = ']';				
			#endif
			
			if(json_array)
			{
				*value_len = htonl(strlen(json_array));
				NORDIC_UART_LOGI("json_array_len = %d\n", strlen(json_array));
				//NORDIC_UART_LOGI("json_array = %s\n", json_array);
				//NORDIC_UART_LOGI("time = %d , %d\n", p_req[0].time, p_req[1].time);
				
				if(strlen(json_array) > 6*1024 - 46)
				{
					NORDIC_UART_LOGE(" json_array too long\n");
					goto HEART_UPLOAD_SEND;
				}
				
				memcpy(p_value, json_array, strlen(json_array));						
				pack_data_len += sizeof(int) + strlen(json_array);					
			}
					
			//send
			HEART_UPLOAD_SEND:
			if(heart_upload_req_buf != NULL)
			{
				cJSON_free(json_array);			//free
				vPortFree(heart_upload_req_buf);//free
				heart_upload_req_buf = NULL;
			}
		}
		break;
		case LOCATION_UPLOAD:
		case WEATHER_QUERY:
		{
			//hal_uart_send_dma(NORDIC_UART, (char*)p_packet, (sizeof(sg_commut_data_t) + p_packet->packet_len + 5));// for test
			NORDIC_UART_LOGI("nordic send LOCATION_UPLOAD: \n");
			//get value from nordic
			sg_wifi_t wifi[WIFI_FIX_CNT] = {0};
			sg_walk_t walk = {0};
			sg_battery_t battery = {0};
			
			char *p_data = p_packet->data;
			memcpy(&wifi[0], p_data, sizeof(sg_wifi_t)*WIFI_FIX_CNT);
			p_data += sizeof(sg_wifi_t)*WIFI_FIX_CNT;
			memcpy(&walk, p_data, sizeof(sg_walk_t));
			p_data += sizeof(walk);
			memcpy(&battery, p_data, sizeof(sg_battery_t));
			NORDIC_UART_LOGI("token = %s", p_packet->token);
			NORDIC_UART_LOGI("walk count = %d, battery value = %d, time=%d\n", walk.count, battery.value, battery.time);
			//get value from 2621
			//temp no data
			sg_mobile_t *p_mobile = (sg_mobile_t*)pvPortMalloc(sizeof(sg_mobile_t));//malloc
			memset(p_mobile, 0x0, sizeof(sg_mobile_t));

			sg_gps_t *p_gps = &(g_nb_local_data.gps);
			
			//repack
			//--------key "wifi" 
			int *p_key_wifi_len = p_packet->data;
			char *key_wifi = "wifi";
			char *p_key_wifi = (char*)(p_key_wifi_len+1);

			*p_key_wifi_len = htonl(strlen(key_wifi));
			memcpy(p_key_wifi, key_wifi, strlen(key_wifi));

			pack_data_len += sizeof(int) + strlen(key_wifi);
			//-------- value wifiJsonArray
			int *p_value_wifi_len = (int*)(p_packet->data + pack_data_len);
			char *p_value_wifi = (char*)(p_value_wifi_len+1);
			
			cJSON *wifi_array = cJSON_CreateArray();
			if(wifi_array == NULL)
			{
				goto LOCATION_UPLOAD_ERROR;
			}
			cJSON *wifi_obj = cJSON_CreateObject();
			if(wifi_obj == NULL)
			{
				cJSON_Delete(wifi_array);
				goto LOCATION_UPLOAD_ERROR;
			}
			for(int i=0; i<WIFI_FIX_CNT; i++)
			{
				char macx[4+1] = {0};
				char macnamex[8+1] = {0};
				char signalx[7+1] = {0};
				
				sprintf(macx, "mac%d", i+1);
				sprintf(macnamex, "macname%d", i+1);
				sprintf(signalx, "signal%d", i+1);
				
				cJSON_AddStringToObject(wifi_obj, macx, wifi[i].mac);
				cJSON_AddStringToObject(wifi_obj, macnamex, wifi[i].mac_name);
				cJSON_AddNumberToObject(wifi_obj, signalx, wifi[i].signal);
			}
			double tmp_wifi_time = (double)battery.time;
			cJSON_AddNumberToObject(wifi_obj, "time", tmp_wifi_time);
			cJSON_AddItemToArray(wifi_array, wifi_obj);
			char *wifi_json = cJSON_Print(wifi_array);
			NORDIC_UART_LOGI("wifi_json = %s\n", wifi_json);
			
			*p_value_wifi_len = htonl(strlen(wifi_json));
			memcpy(p_value_wifi, wifi_json, strlen(wifi_json));

			pack_data_len += sizeof(int) + strlen(wifi_json);

			cJSON_free(wifi_json);
			cJSON_Delete(wifi_array);
			
			//------key "mobile"
			int *p_key_mobile_len = (int*)(p_packet->data + pack_data_len);
			char* key_mobile = "mobile";
			char *p_key_mobile = (char*)(p_key_mobile_len+1);
			*p_key_mobile_len = htonl(strlen(key_mobile));
			memcpy(p_key_mobile, key_mobile, strlen(key_mobile));

			pack_data_len += sizeof(int) + strlen(key_mobile);
			//-----value mobileJsonArray
			int *p_value_mobile_len = (int*)(p_packet->data + pack_data_len);
			char *p_value_mobile = (char*)(p_value_mobile_len+1);

			cJSON *mobile_array = cJSON_CreateArray();
			if(mobile_array == NULL)
			{
				goto LOCATION_UPLOAD_ERROR;
			}
			cJSON *mobile_obj = cJSON_CreateObject();
			if(mobile_obj == NULL)
			{
				cJSON_Delete(mobile_array);
				goto LOCATION_UPLOAD_ERROR;
			}
			cJSON_AddStringToObject(mobile_obj, "serverIp", p_mobile->dev_ip);
			cJSON_AddStringToObject(mobile_obj, "network", p_mobile->network);
			cJSON_AddStringToObject(mobile_obj, "mcc", p_mobile->mcc);
			cJSON_AddStringToObject(mobile_obj, "mnc", p_mobile->mnc);
			double time_d = (double)g_nb_local_data.time;
			cJSON_AddNumberToObject(mobile_obj, "time", time_d);
			for(int i=0; i<LAC_FIX_CNT; i++)
			{
				char lacx[4+1] = {0};
				char cix[3+1] = {0};
				char rssix[5+1] = {0};

				sprintf(lacx, "lac%d", i+1);
				sprintf(cix, "ci%d", i+1);
				sprintf(rssix, "rssi%d", i+1);
				
				cJSON_AddNumberToObject(mobile_obj, lacx, p_mobile->lac_data[i].lac);
				cJSON_AddNumberToObject(mobile_obj, cix, p_mobile->lac_data[i].ci);
				cJSON_AddNumberToObject(mobile_obj, rssix, p_mobile->lac_data[i].rssi);
			}
			cJSON_AddItemToArray(mobile_array, mobile_obj);
			char *mobile_json = cJSON_Print(mobile_array);
			NORDIC_UART_LOGI("mobile_json = %s\n", mobile_json);
			
			*p_value_mobile_len = htonl(strlen(mobile_json));
			memcpy(p_value_mobile, mobile_json, strlen(mobile_json));

			pack_data_len += sizeof(int) + strlen(mobile_json);

			cJSON_free(mobile_json);
			cJSON_Delete(mobile_array);
			vPortFree(p_mobile);
			//-----key "gps"
			int *p_key_gps_len = (int*)(p_packet->data + pack_data_len);
            char *key_gps = "gps";
            char *p_key_gps = (char*)(p_key_gps_len + 1);

            *p_key_gps_len = htonl(strlen(key_gps));
            memcpy(p_key_gps, key_gps, strlen(key_gps));

            pack_data_len += sizeof(int) + strlen(key_gps);
            //-------value gpsJSON
			int *p_value_gps_len = (int*)(p_packet->data + pack_data_len);
            char *p_value_gps = (char*)(p_value_gps_len+1);

            cJSON *gps_array = cJSON_CreateArray();
            if(gps_array == NULL)
            {
                goto LOCATION_UPLOAD_ERROR;
            }
            cJSON *gps_obj = cJSON_CreateObject();
            if(gps_obj == NULL)
            {
            	cJSON_Delete(gps_array);
                goto LOCATION_UPLOAD_ERROR;
            }
            cJSON_AddNumberToObject(gps_obj, "latitude", p_gps->latitude);
            cJSON_AddNumberToObject(gps_obj, "longitude", p_gps->longitude);
            cJSON_AddNumberToObject(gps_obj, "time", p_gps->time);
            cJSON_AddItemToArray(gps_array, gps_obj);
            char *gps_json = cJSON_Print(gps_array);
            NORDIC_UART_LOGI("gps_json = %s\n", gps_json);

            *p_key_gps_len = htonl(strlen(gps_json));
            memcpy(p_value_gps, gps_json, strlen(gps_json));

            pack_data_len += sizeof(int) + strlen(gps_json);

            cJSON_free(gps_json);
            cJSON_Delete(gps_array);  
			//----"walk" = {"count":100,"time":1505890061000}
			//----key "walk"
            int *p_key_walk_len = (int*)(p_packet->data + pack_data_len);
            char *key_walk = "walk";
            char *p_key_walk = (char*)(p_key_walk_len + 1);

            *p_key_walk_len = htonl(strlen(key_walk));
            memcpy(p_key_walk, key_walk, strlen(key_walk));

            pack_data_len += sizeof(int) + strlen(key_walk);
			//----value {"count":100,"time":1505890061000}
            int *p_value_walk_len = (int*)(p_packet->data + pack_data_len);
            char *p_value_walk = (char*)(p_value_walk_len + 1);

			#if 0
            cJSON *walk_obj = cJSON_CreateObject();
            if(walk_obj == NULL)
            {
                goto LOCATION_UPLOAD_ERROR;
            }
            cJSON_AddNumberToObject(walk_obj, "count", walk.count);
            double tmp_time = (double)walk.time;
            cJSON_AddNumberToObject(walk_obj, "time", tmp_time);
            char *walk_json = cJSON_Print(walk_obj);
            cJSON_Delete(walk_obj);
            #else
            char *walk_json = (char*)pvPortMalloc(50);
            memset(walk_json, 0x0, 50);
            sprintf(walk_json, "{\"count\":%d,\"time\":%d000}", walk.count, walk.time);
            #endif
            NORDIC_UART_LOGI("walk_json = %s\n", walk_json);

            *p_value_walk_len = htonl(strlen(walk_json));
            memcpy(p_value_walk, walk_json, strlen(walk_json));

            pack_data_len += sizeof(int) + strlen(walk_json);

            cJSON_free(walk_json); 
			//----"battery" = {"value":29,"time":1505890061000}
			//----key "battery"
            int *p_key_bat_len = (int*)(p_packet->data + pack_data_len);
            char *key_bat = "battery";
            char *p_key_bat = (char*)(p_key_bat_len + 1);

            *p_key_bat_len = htonl(strlen(key_bat));
            memcpy(p_key_bat, key_bat, strlen(key_bat));

            pack_data_len += sizeof(int) + strlen(key_bat);
            //----value  {"value":29,"time":1505890061000}
            int *p_value_bat_len = (int*)(p_packet->data + pack_data_len);
            char *p_value_bat = (char*)(p_value_bat_len+1);
			#if 0
            cJSON *bat_obj = cJSON_CreateObject();
            if(bat_obj == NULL)
            {
                goto LOCATION_UPLOAD_ERROR;
            }
            cJSON_AddNumberToObject(bat_obj, "value", battery.value);
            double bat_time = (double)battery.time;
            cJSON_AddNumberToObject(bat_obj, "time", bat_time);
            char *bat_json = cJSON_Print(bat_obj);
            cJSON_Delete(bat_obj);
            #else 
            char *bat_json = (char*)pvPortMalloc(50);
            memset(bat_json, 0x0, 50);
            sprintf(bat_json, "{\"value\":%d,\"time\":%d000}", battery.value, battery.time);
            #endif
            NORDIC_UART_LOGI("bat_json = %s\n", bat_json);

            *p_value_bat_len = htonl(strlen(bat_json));
            memcpy(p_value_bat, bat_json, strlen(bat_json));

            pack_data_len += sizeof(int) + strlen(bat_json);

            cJSON_free(bat_json);
			//repack end------------------------------------------------------	
				
			//send
			//LOCATION_UPLOAD_SEND:
			NORDIC_UART_LOGI("token = %s", p_packet->token);
			NORDIC_UART_LOGI("pack_data_len = %d\n", pack_data_len);
			break;
			LOCATION_UPLOAD_ERROR:
			pack_flag = 0;
		}
		break;
		case USER_BASE_INFO_UPLOAD:
		{
			NORDIC_UART_LOGI("nordic send USER_BASE_INFO_UPLOAD: \n");
			/*
				key			value
				birthday 	2018-09-28
				sex			1
				height		175
				weight		60
			*/
			char *key1 = "birthday";
			char *key2 = "sex";
			char *key3 = "height";
			char *key4 = "weight";

			//parse and get values
			//sg_user_info_t *user_info = (sg_user_info_t*)p_packet->data;
			sg_user_info_t user_info = {0};
			memcpy(&user_info, p_packet->data, sizeof(sg_user_info_t));

			//repack
			#if 0
			//.......birthday = 2018-09-28
			int *p_key1_len = (int*)p_packet->data;
			char *p_key1 = (char*)(p_key1_len+1);
			memcpy(p_key1, key1, strlen(key1));
			*p_key1_len = htonl(strlen(key1));
			pack_data_len += sizeof(int)+strlen(key1);

			int *p_value1_len = (int*)(p_key1+strlen(key1));
			char *p_value1 = (char*)(p_value1_len+1);
			memcpy(p_value1, user_info.birthday, strlen(user_info.birthday));
			*p_value1_len = htonl(strlen(user_info.birthday));
			pack_data_len += sizeof(int) + strlen(user_info.birthday);
			//........sex = 1
			int *p_key2_len = (int*)(p_value1 + strlen(user_info.birthday));
			char *p_key2 = (char*)(p_key2_len+1);
			memcpy(p_key2, key2, strlen(key2));
			*p_key2_len = htonl(strlen(key2));
			pack_data_len += sizeof(int)+strlen(key2);

			int *p_value2_len = (int*)(p_key2 + strlen(key2));
			char *p_value2 = (char*)(p_value2_len+1);
			itoa(user_info.sex, p_value2, 10);//sprintf(p_value2, "%d", user_info.sex);
			*p_value2_len = htonl(strlen(p_value2));
			pack_data_len += sizeof(int) + strlen(p_value2);
			//.......height = 175
			int *p_key3_len = (int*)(p_value2+strlen(p_value2));
			char *p_key3 = (char*)(p_key3_len + 1);
			memcpy(p_key3, key3, strlen(key3));
			*p_key3_len = htonl(strlen(key3));
			pack_data_len += sizeof(int) + strlen(key3);

			int *p_value3_len = (int*)(p_key3+strlen(key3));
			char *p_value3 = (char*)(p_value3_len+1);
			itoa(user_info.height, p_value3, 10);
			*p_value3_len = htonl(strlen(p_value3));
			pack_data_len += sizeof(int) + strlen(p_value3);
			//.......weight = 60
			int *p_key4_len = (int*)(p_value3+strlen(p_value3));
			char *p_key4 = (char*)(p_key4_len+1);
			memcpy(p_key4, key4, strlen(key4));
			*p_key4_len = htonl(strlen(key4));
			pack_data_len += sizeof(int) + strlen(key4);

			int *p_value4_len = (int*)(p_key4+strlen(key4));
			char *p_value4 = (char*)(p_value4_len+1);
			itoa(user_info.weight, p_value4, 10);
			*p_value4_len = htonl(strlen(p_value4));
			pack_data_len += sizeof(int) + strlen(p_value4);
			#else 
				//value1 -->user_info.birsday
				char *value1 = user_info.birthday;
				pack_data_len += addKeyValue(p_packet->data, key1, value1);

				char value2[3+1] = {0};
				itoa(user_info.sex, value2, 10);
				pack_data_len += addKeyValue(p_packet->data + pack_data_len, key2, value2);

				char value3[3+1] = {0};
				itoa(user_info.height, value3, 10);
				pack_data_len += addKeyValue(p_packet->data + pack_data_len, key3, value3);

				char value4[3+1] = {0};
				itoa(user_info.weight, value4, 10);
				pack_data_len += addKeyValue(p_packet->data + pack_data_len, key4, value4);

			#endif

			//send
		}
		break;
		case HEALTH_UPLOAD:
		{
			NORDIC_UART_LOGI("nordic send HEALTH_UPLOAD: \n");

		}
		break;
		case SLEEP_UPLOAD:
		{
			NORDIC_UART_LOGI("nordic send SLEEP_UPLOAD: \n");
			// data = json
			//sprintf(json, "[{\"total\":%d000-%d000,...}]",time1， time2 );
			char *key = "data";

			//parse and get value
			sg_sleep_t sleep_data = {0};
			memcpy(&sleep_data, p_packet->data, sizeof(sg_sleep_t));
			
			//repack
			#if 0
			int *p_key_len = (int*)p_packet->data;
			char *p_key = (char*)(p_key_len+1);
			memcpy(p_key, key, strlen(key));
			*p_key_len = htonl(strlen(key));
			pack_data_len += sizeof(int)+strlen(key);

			int *p_value_len = (int*)(p_key+strlen(key));
			char *p_value = (char*)(p_value_len+1);
			sprintf(p_value, "[{\"total\":\"%d000-%d000\",\"deep\":%d,\"wake\":%d,\"light\":%d}]", sleep_data.time_begin,\
            sleep_data.time_end, sleep_data.time_deep, sleep_data.time_wake, sleep_data.time_light);
			*p_value_len = htonl(strlen(p_value));
			pack_data_len += sizeof(int)+strlen(p_value);
			#else
				char value[256] = {0};
				memset(value, 0, sizeof(value));
				sprintf(value, "[{\"total\":\"%d000-%d000\",\"deep\":%d,\"wake\":%d,\"light\":%d}]", sleep_data.time_begin,\
            	sleep_data.time_end, sleep_data.time_deep, sleep_data.time_wake, sleep_data.time_light);

				pack_data_len += addKeyValue(p_packet->data, key, value);
			#endif

			//send
		}
		break;
		case LUNAR_QUERY:
		{
			NORDIC_UART_LOGI("nordic send LUNAR_QUERY: \n");
			// key 	  value
			// date = 2017-02-15	
			char *key = "date";

			//parse and get value
			int unix_time = *(int*)p_packet->data;
			long int tmp = unix_time;
			struct tm time = *localtime((time_t*)&tmp);

			//repack
			#if 0
			int *p_key_len = (int*)p_packet->data;
			char *p_key = (char*)(p_key_len+1);
			memcpy(p_key, key, strlen(key));
			*p_key_len = htonl(strlen(key));
			pack_data_len += sizeof(int)+strlen(key);

			int *p_value_len = (int*)(p_key+strlen(key));
			char *p_value = (char*)(p_value_len+1);
			sprintf(p_value, "%d-%02d-%02d", time.tm_year+1900, time.tm_mon, time.tm_yday);
			*p_value_len = htonl(strlen(p_value));
			pack_data_len += sizeof(int) + strlen(p_value);
			#else 
			char value[15+1] = {0};
			sprintf(value, "%d-%02d-%02d", time.tm_year+1900, time.tm_mon, time.tm_yday);
			pack_data_len += addKeyValue(p_packet->data, key, value);
			#endif
			//send
		}
		break;
		case DEVICE_OPERATION:
		{
			NORDIC_UART_LOGI("nordic send DEVICE_OPERATION: \n");
		}
		break;
		case DEVICE_COERCE_UNBIND:
		{
			NORDIC_UART_LOGI("nordic send DEVICE_COERCE_UNBIND: \n");

		}
		break;
		default:
			NORDIC_UART_LOGE("%s---------nordic send CMD ERRIR\n", __FUNCTION__);
			pack_flag = 0;
		break;
	}

	//p_packet->protocol_ver = SG_PROTOCOL_VER;
    //p_packet->enryption_type = SG_ENCRYPTION_TYPE;
    //p_packet->status = 0;
	//memcpy(p_packet->token, g_nb_local_data.token, 32);// token get from nordic
	
    p_packet->cmd = htonl(p_packet->cmd);
	p_packet->packet_len = htonl(pack_data_len);
	if(pack_data_len > 1024*6 - sizeof(sg_commut_data_t) -5)
	{
		NORDIC_UART_LOGE("pack_date_len=%d;  buffer is overflow", pack_data_len);
		return;
	}
    p_packet->data[pack_data_len] = SG_PACKET_END1;
    p_packet->data[pack_data_len + 1] = SG_PACKET_END2;
    p_packet->data[pack_data_len + 2] = SG_PACKET_END3;
    p_packet->data[pack_data_len + 3] = SG_PACKET_END4;
    p_packet->data[pack_data_len + 4] = SG_PACKET_END5;     
    
    //hal_uart_send_dma(NORDIC_UART, (char*)p_packet, (sizeof(sg_commut_data_t) + pack_data_len + 5));// for test
    #ifdef NB_SOCKET
	if(pack_flag)
	{
		nb_socket_send((char*)p_packet, (sizeof(sg_commut_data_t) + pack_data_len + 5));
		NORDIC_UART_LOGI("-------------------->cmd = %d, token = %s", ntohl(p_packet->cmd), p_packet->token);
	}
	#endif
}

//nb socket recv message and send to nordic
void socket_recv_msg_process(void *msg, int len)
{
    char recv_flag = 0;
    sg_commut_data_t *p_recv_packet = (sg_commut_data_t*)msg;
	NORDIC_UART_LOGI("%s------CMD=%X, data_len=%d, recv_data=%s\n", __FUNCTION__, ntohl(p_recv_packet->cmd), ntohl(p_recv_packet->packet_len), p_recv_packet->data);
	
	//-----------------------------------------------------------------
	//parse --> repack --> send to nordic
	if(p_recv_packet->status == 0)
	{
		int cmd = ntohl(p_recv_packet->cmd);
		p_recv_packet->cmd = cmd;// to little end
		
		switch(cmd)
		{
			case DEVICE_HELLO:
			{
				/*	key 	value
					status  "0"
					userId  "1234"
				*/
				NORDIC_UART_LOGI("%s---------DEVICE_HELLO", __FUNCTION__);
				// get token
				memcpy(g_nb_local_data.token, p_recv_packet->token, 32);// token size is 32
				NORDIC_UART_LOGI("token = %s", g_nb_local_data.token);
				//parse
				//--------status
				int* p_key_len = (int*)p_recv_packet->data;
				int key_len = ntohl(*p_key_len);  //ntohl
				char *p_key = (char*)(p_key_len+1);
				
				NORDIC_UART_LOGI("key_len=%d, key=%s\n", key_len, p_key); //"userId"

				int *p_value_len = (int*)(p_key+key_len);
				int value_len = ntohl(*p_value_len); //ntohl
				char *p_value = (char*)(p_value_len+1);

				NORDIC_UART_LOGI("value_len=%d, value=%s\n", value_len, p_value);//"U8589"
				//-------userId
				int *p_key2_len = (int*)(p_value+value_len);
				int key2_len = ntohl(*p_key2_len);
				char *p_key2 = (char*)(p_key2_len+1);
				NORDIC_UART_LOGI("key2_len=%d, key2=%s\n", key2_len, p_key2);//"status"

				int *p_value2_len = (int*)(p_key2+key2_len);
				int value2_len = ntohl(*p_value2_len);
				char *p_value2 = (char*)(p_value2_len+1);
				NORDIC_UART_LOGI("value2_len=%d, value2=%s\n", value2_len, p_value2);//"0"
				
				//repack --> nordic(little end)
			
				dev_bind_req_t dev_bind = {0};
				memcpy(dev_bind.userId, p_value, strlen(p_value));
				dev_bind.status = atoi(p_value2);

				memset(p_recv_packet->data, 0x0, len-sizeof(sg_commut_data_t));
				memcpy(p_recv_packet->data, (char*)&dev_bind, sizeof(dev_bind));

				p_recv_packet->packet_len = sizeof(dev_bind_req_t);
				//send
			}
			break;
			case DEVICE_HEARTBEAT:
			{	
				/*
					key 	value
					status  "1"		//1 connect; 0 close  ?????
				*/
				NORDIC_UART_LOGI("%s---------DEVICE_HEARTBEAT\n", __FUNCTION__);
	
				//parse
				
				
				int *p_key_len = (int*)p_recv_packet->data;
				char *p_key = (char*)(p_key_len+1);
				int key_len = ntohl(*p_key_len);
				NORDIC_UART_LOGI("key_len = %d, key = %s\n", key_len, p_key);

				int *p_value_len = (int*)(p_key+key_len);
				char *p_value = (char*)(p_value_len+1);
				int value_len = ntohl(*p_value_len);
				NORDIC_UART_LOGI("value_len = %d, value = %s\n", value_len, p_value);

				#if 0 //send to server //"walk" = {"count":1000,"time":1428551739620}
				cJSON *obj = cJSON_Parse(p_value);
				heart_beat_rsp_t rsp;
				if(obj)
				{
					cJSON *item_count = cJSON_GetObjectItem(obj, "count");
					cJSON *item_time = cJSON_GetObjectItem(obj, "time");
					rsp.count = item_count->valueint;
					rsp.time = int(item_time->valuedouble/1000);
					NORDIC_UART_LOGI("count = %d, time = %d\n", rsp.count, rsp.time);
					
					cJSON_Delete(obj);
				}
				#else // recv form server //"status" = "1"
				int rsp = p_value[0]=='1'?1:0;
				#endif
				
				//repack --> nordic
				memcpy(p_recv_packet->data, (char*)&rsp, sizeof(rsp));
				p_recv_packet->packet_len = sizeof(rsp);
			}
			break;
			case DEVICE_BIND:
			{
				NORDIC_UART_LOGI("%s---------DEVICE_BIND", __FUNCTION__);
				p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case DEVICE_UNBIND:
			{
				NORDIC_UART_LOGI("%s---------DEVICE_UNBIND", __FUNCTION__);
				p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case LOCATION_TRRIGE:
			{
				NORDIC_UART_LOGI("%s---------LOCATION_TRRIGE", __FUNCTION__);
				p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case LOCATION_UPLOAD:
			{
				NORDIC_UART_LOGI("%s---------LOCATION_UPLOAD", __FUNCTION__);
				p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
				
			}
			break;
			case HEART_UPLOAD:
			{
				NORDIC_UART_LOGI("%s---------HEART_UPLOAD", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case USER_BASE_INFO_UPLOAD:
			{
				NORDIC_UART_LOGI("%s---------USER_BASE_INFO_UPLOAD", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case SLEEP_UPLOAD:
			{
				NORDIC_UART_LOGI("%s---------SLEEP_UPLOAD", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
			}
			break;
			case WEATHER_QUERY:
			{
				NORDIC_UART_LOGI("%s---------WEATHER_QUERY", __FUNCTION__);
				p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
				//data = json

				//parse and get value
				int *p_key_len = (int*)p_recv_packet->data;
				char *p_key = (char*)(p_key_len + 1);
				NORDIC_UART_LOGI("key_len=%d, key=%s\n", ntohl(*p_key_len), p_key);

				int *p_value_len = (int*)(p_key+ntohl(*p_key_len));
				char *p_value = (char*)(p_value_len + 1);
				NORDIC_UART_LOGI("value_len=%d, value=%s\n", ntohl(*p_value_len), p_value);

				sg_weather_t weather[3] = {0};
				cJSON *array_json = cJSON_Parse(p_value);
				if(array_json == NULL)
				{
					NORDIC_UART_LOGE("array_json parse error");
					return;
				}
				int array_size = cJSON_GetArraySize(array_json);
				for(int i=0; i<array_size; i++)
				{
					cJSON *weather_json = cJSON_GetArrayItem(array_json, i);
					if(weather_json)
					{
						cJSON *item_date = cJSON_GetObjectItem(weather_json, "date");
						if(item_date && item_date->type == cJSON_String)
						{
							//memcpy(weather[i].date, item_date->valuestring, strlen(item_date->valuestring));
							strcpy(weather[i].date, item_date->valuestring);
							NORDIC_UART_LOGE("date = %s\n", weather[i].date);
						}
						cJSON *item_weather = cJSON_GetObjectItem(weather_json, "weather");
						if(item_weather && item_weather->type == cJSON_String)
						{
							strcpy(weather[i].weather, item_weather->valuestring);
							NORDIC_UART_LOGE("weather = %s\n", weather[i].weather);
						}
						cJSON *item_temp = cJSON_GetObjectItem(weather_json, "temperature");
						if(item_temp && item_temp->type == cJSON_String)
						{
							strcpy(weather[i].temperature, item_temp->valuestring);
							NORDIC_UART_LOGE("temperature = %s\n", weather[i].temperature);
						}
						cJSON *item_wind = cJSON_GetObjectItem(weather_json, "wind");
						if(item_wind && item_wind->type == cJSON_String)
						{
							strcpy(weather[i].wind, item_wind->valuestring);
							NORDIC_UART_LOGE("wind = %s\n", weather[i].wind);
						}
						cJSON *item_dress = cJSON_GetObjectItem(weather_json, "dress");
						if(item_dress && item_dress->type == cJSON_String)
						{
							strcpy(weather[i].dress, item_dress->valuestring);
							NORDIC_UART_LOGE("dress = %s\n", weather[i].dress);
						}
						cJSON *item_curtemp = cJSON_GetObjectItem(weather_json, "curtemperature");
						if(item_curtemp && item_curtemp->type == cJSON_String)
						{
							strcpy(weather[i].cur_temp, item_curtemp->valuestring);
							NORDIC_UART_LOGE("cur_temp = %s\n", weather[i].cur_temp);
						}
						cJSON *item_air = cJSON_GetObjectItem(weather_json, "airQuality");
						if(item_air && item_air->type == cJSON_String)
						{
							strcpy(weather[i].airQuality, item_air->valuestring);
							NORDIC_UART_LOGE("airQuality = %s\n", weather[i].airQuality);
						}
						cJSON *item_city = cJSON_GetObjectItem(weather_json, "city");
						if(item_city && item_city->type == cJSON_String)
						{
							strcpy(weather[i].city, item_city->valuestring);
							NORDIC_UART_LOGE("city = %s\n", weather[i].city);
						}
					}
				}
				cJSON_Delete(array_json);

				//repack and send to nordic
				
			}
			break;
			case LUNAR_QUERY:
			{
				NORDIC_UART_LOGI("%s---------LUNAR_QUERY", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);
				//data = json

				//parse and get value
				int *p_key_len = (int*)p_recv_packet->data;
				char *p_key = (char*)(p_key_len+1);
				NORDIC_UART_LOGI("key_len=%d, key=%s\n", ntohl(*p_key_len), p_key);

				int *p_value_len = (int*)(p_key+ntohl(*p_key_len));
				char *p_value = (char*)(p_value_len+1);
				NORDIC_UART_LOGI("value_len=%d, value=%s\n", ntohl(*p_value_len), p_value);

				sg_lunar_t lunar = {0};
				cJSON *lunar_json = cJSON_Parse(p_value);
				if(lunar_json == NULL)
				{
					NORDIC_UART_LOGE("lunar_json parse error");
					return;
				}
				cJSON *item_date = cJSON_GetObjectItem(lunar_json, "date");
				if(item_date && item_date->type == cJSON_String)
				{
					//memcpy(lunar.date, item_date->valueString, strlen(item_date->valueString));
					strcpy(lunar.date, item_date->valuestring);
					NORDIC_UART_LOGI("date =%s", lunar.date);
				}
				cJSON *item_week = cJSON_GetObjectItem(lunar_json, "week");
				if(item_week && item_week->type == cJSON_String)
				{
					strcpy(lunar.week, item_week->valuestring);
					NORDIC_UART_LOGI("week =%s", lunar.week);
				}
				cJSON *item_lunar = cJSON_GetObjectItem(lunar_json, "lunar");
				if(item_lunar && item_lunar->type == cJSON_String)
				{
					strcpy(lunar.lunar, item_lunar->valuestring);
					NORDIC_UART_LOGI("lunar =%s", lunar.lunar);
				}
				cJSON *item_ganzhi = cJSON_GetObjectItem(lunar_json, "ganzhi");
				if(item_ganzhi && item_ganzhi->type == cJSON_String)
				{
					strcpy(lunar.ganzhi, item_ganzhi->valuestring);
					NORDIC_UART_LOGI("ganzhi =%s", lunar.ganzhi);
				}
				cJSON *item_zodiac = cJSON_GetObjectItem(lunar_json, "zodiac");
				if(item_zodiac && item_zodiac->type == cJSON_String)
				{
					strcpy(lunar.zodiac, item_zodiac->valuestring);
					NORDIC_UART_LOGI("zodiac =%s", lunar.zodiac);
				}
				cJSON *item_fitavoid = cJSON_GetObjectItem(lunar_json, "fitavoid");
				if(item_fitavoid && item_fitavoid->type == cJSON_String)
				{
					strcpy(lunar.fitavoid, item_fitavoid->valuestring);
					NORDIC_UART_LOGI("fitavoid =%s", lunar.fitavoid);
				}
				cJSON *item_time = cJSON_GetObjectItem(lunar_json, "time");
				if(item_time && item_time->type == cJSON_Number)
				{
					lunar.time = (int)(item_time->valuedouble/1000);
					NORDIC_UART_LOGI("time =%s", lunar.time);
				}
				cJSON_Delete(lunar_json);
				//repack
				p_recv_packet->packet_len = sizeof(sg_lunar_t);
				memcpy(p_recv_packet->data, &lunar, sizeof(sg_lunar_t));

				//send to nordic
			}
			break;
			case DEVICE_OPERATION:
			{
				NORDIC_UART_LOGI("%s---------DEVICE_OPERATION", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);

			}
			break;
			case DEVICE_COERCE_UNBIND:
			{
				NORDIC_UART_LOGI("%s---------DEVICE_COERCE_UNBIND", __FUNCTION__);
				//p_recv_packet->packet_len = ntohl(p_recv_packet->packet_len);

			}
			break;

			default:
				NORDIC_UART_LOGE("%s---------RECV CMD ERROR", __FUNCTION__);
			break;
		}

	}
    
    //p_recv_packet->protocol_ver = SG_PROTOCOL_VER;
    //p_recv_packet->enryption_type = SG_ENCRYPTION_TYPE;
    //p_recv_packet->status = 0;
     
    p_recv_packet->data[p_recv_packet->packet_len] = SG_PACKET_END1;
    p_recv_packet->data[p_recv_packet->packet_len + 1] = SG_PACKET_END2;
    p_recv_packet->data[p_recv_packet->packet_len + 2] = SG_PACKET_END3;
    p_recv_packet->data[p_recv_packet->packet_len + 3] = SG_PACKET_END4;
    p_recv_packet->data[p_recv_packet->packet_len + 4] = SG_PACKET_END5;
	//--------------------------------------------------------------------------------------------
    int pack_len = sizeof(sg_commut_data_t) + (p_recv_packet->packet_len) + 5;//head + data + tail
    hal_uart_send_dma(NORDIC_UART, (uint8_t*)p_recv_packet, pack_len);
}

unsigned char sum_cal2(char* array, uint len)
{
    uint i=0;
    unsigned char sum = 0;

    for(i=0;i<len;i++)
    {
        sum += array[i];
    }

    return sum;
}

void nb_state_set_and_get(nordic_get_and_set_nb_state_t state)
{
    switch(state.type)
    {
        case NB_STATE_SET:
        {
            switch(state.state)
            {
                case CLOSE_NB:
                {
                    nb_poweroff();
                }
                break;
                case CLOSE_GSM:
                {
                    //AT+MSNBONLYMODE=0 (powergating mode, close 2g)
                    ril_request_enter_nbiot_only_mode(RIL_EXECUTE_MODE, 0, NULL, NULL);//close 2g
                }
                break;
                case ENTER_DUAL_MODE:
                {
                    nb_enter_dual_mode();
                }
                break;
                case CLOSE_UBLOX:
                {

                }
                break;
                case OPEN_UBLOX:
                {

                }
                break;
                default:
                break;
            }
        }
        break;
        case NB_STATE_GET:
        {

        }
        break;
        default:
        break;
    }
}



