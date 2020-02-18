#ifndef __FRAME_DEFINE_H__
#define __FRAME_DEFINE_H__

//temp----------------------------
#ifndef int8
typedef signed char        int8;
typedef signed short       int16;
typedef signed int         int32;
typedef signed long long   int64;
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int        uint32;
typedef unsigned long long uint64;
#endif
//---------------------------------

#define SG_PROTOCOL_VER         (0x01)
#define SG_ENCRYPTION_TYPE      (0)
#define SG_PACKET_END1          (0x23)
#define SG_PACKET_END2          (0x23)
#define SG_PACKET_END3          (0x5F)
#define SG_PACKET_END4          (0x2A)
#define SG_PACKET_END5          (0x2A)

#pragma pack(0x1)

typedef struct sg_commut_data
{
    uint8_t protocol_ver;
    uint8_t enryption_type;
    int status;
    char token[32];
    int cmd;
    int packet_len;
    char data[0];
}sg_commut_data_t;

typedef enum NB_COMMUT_TYPE
{
    NB_COMMUT_TYPE_UNKNOW = -1,
    NB_COMMUT_TYPE_SOCKET,
    NB_COMMUT_TYPE_HTTP,
    NB_COMMUT_TYPE_LOCAL,
    NB_COMMUT_TYPE_CTWING,
}SG_NB_COMMUT_TYPE_E;

typedef enum NB_COMMUT_STATUS
{
    NB_RECV_WAIT = -2,
    NB_RECV_ERROR = -1,
    NB_RECV_SUCCESS,
}SG_NB_COMMUT_STATUS_E;

/* global CMD */
typedef enum SG_COMMUT_CMD
{
    DEVICE_HEARTBEAT                = 62001, //F231 /* Device heart beat */
    DEVICE_SOFTWARE_VERSION_QUERY   = 60101, /* Fevice sfotware version query */
    DEVICE_HELLO                    = 60201, //0000EB29 /* Greeting the server after the device is powered on for the first time (unbound) */
    DEVICE_BIND                     = 60202, //EB2A /* Device binding confirmation */
    DEVICE_UNBIND                   = 60103, /* Device unbundling */
    USER_INFO_GET                   = 60501, /* Get user information */
    USER_INFO_PUSH                  = 60502, /* User information change push */
    USER_BASE_INFO_UPLOAD           = 60503, /* Upload user watch basic information upload */
    LOCATION_UPLOAD                 = 60403, // EBF3/* (Position, steps, power) report */
    LOCATION_QUERY                  = 60901, /* (location, step, power) query */
    LOCATION_TRRIGE					= 60905, //EDE9
    HEART_QUERY                     = 60902, /* heart rate data query */
    HEART_UPLOAD                    = 60302, //EB8E /* Heart rate data upload */
    SLEEP_QUERY                     = 62003, /* Sleep data query */
    SLEEP_UPLOAD                    = 60305, /* Sleep data upload */
    WEATHER_QUERY                   = 60608, /* The watch actively queries the weather data (1.each time it is turned on; 2.every three hours) */
    LUNAR_QUERY                     = 60609, /* The watch actively obtains the lunar calendar data (1.each time it is turned on; 2.every three hours) */
    DEVICE_OPERATION                = 60306, /* Report the key operation of the watch */
    DEVICE_PARAM_SET                = 60610, /* Device parameter setting (new) */
    DEVICE_PARAM_GET                = 62005, /* Device parameter query (new) */
    CONTROL_SWATCH_SERVICE          = 60607, /* Control watch service */
    HEALTH_TEST                     = 60606, /* The server actively tests the health data */
    DEVICE_COERCE_UNBIND            = 29901, /* The device requested to unbind */
    DEVICE_SOFTWARE_VERSION_UPLOAD  = 60105, /* Device software version upload (with IMSI) */
    HEALTH_UPLOAD                   = 60702, /* Health data upload (depending on project) */
    LOCATION_POLICY_SET             = 60405, /* Position strategy adjustment */

    /* Lcacl cmd, define by cenon (7F xx 00 00)*/
    CMD_GET_SERV_IP                 = 2130771968, /* 7F 01 Get server ip */
    CMD_DEV_UPDATE_TIME             = 2130837504, /* 7F 02 Device update time */
    CMD_DEV_VERS_UPDATE             = 2130903040, /* 7F 03 Device version update */
    CMD_GET_QRCODE                  = 2130968576, /* 7F 04 Device get QRcode */
    CMD_DEV_PARA_INIT               = 2131034112, /* 7F 05 Device param init */

	CMD_NB_GSV                      = 2146893824, /* 7F F7 00 00 Get factory GPS status */
	CMD_NB_STATE                    = 2146959360, /* 7F F8 00 00 Get/Set NB status */
    CMD_NB_WRITE_AGPS               = 2147024896, /* 7F F9 00 00 Write aGPS */
    CMD_NB_RFCAL                    = 2147090432, /* 7F FA 00 00 Get RF calibration state */
    CMD_NB_GPS                      = 2147155968, /* 7F FB 00 00 Get GPS state */
    CMD_NB_VERS                     = 2147221504, /* 7F FC 00 00 Get NB net state */
    CMD_NB_IMEI_SN                  = 2147287040, /* 7F FD 00 00 Get NB net state */
    CMD_NB_NET_STATE                = 2147352576, /* 7F FE 00 00 Get NB net state */
    CMD_DEV_CCID                    = 2147418112, /* 7F FF 00 00 Get sim CCID */
    CMD_DEV_IMSI					= 2147418113, /* 7F FF 00 01 Get IMSI */
}SG_COMMUT_CMD_E;

//===================================
//SG struct
typedef struct 
{
	char userId[10+1];
	int32 status;
}dev_bind_req_t;

typedef struct
{
	uint8_t count;
	int time;
}heart_upload_req_t;

typedef struct
{
	char ccid[20];
    int32 csq;
}ccid_csq_rsp_t;

typedef struct
{
    uint8 gsv_states_in_view;
    struct
    {
        uint8 gsv_sate_id;
        uint8 gsv_sate_snr;
    }gsv_states_info[3];

}gsv_rsp_t;

typedef struct 
{
	int count;
	int time;
}sg_walk_t;

typedef struct
{
	double latitude;
	double longitude;
	uint8_t NS;
	uint8_t EW;
	int time;
}sg_gps_t;

typedef struct
{
	uint8 value;
	int time;
}sg_battery_t;

#define LAC_FIX_CNT 5
typedef struct 
{
	char dev_ip[64+1];
	char network[5+1];
	char mcc[8+1];
	char mnc[8+1];
	int time;
	struct
	{
		int32_t lac;
		int32_t ci;
		int32_t rssi;
	}lac_data[LAC_FIX_CNT];
}sg_mobile_t;

#define WIFI_FIX_CNT 5
typedef struct
{
	char mac[32+1];
	char mac_name[30+1];
	int32_t  signal;
}sg_wifi_t;

typedef struct 
{
    int time;// to long int
    char version[10+1];
}sg_version_t;

typedef struct
{
    char birthday[10+1];//
    uint8_t sex;
    uint8_t height;
    uint8_t weight;
}sg_user_info_t;

typedef struct
{
    int time_begin;
    int time_end;
    int time_deep;
    int time_wake;
    int time_light;
}sg_sleep_t;

typedef struct
{
    char date[10+1];
    char week[9+1];
    char lunar[18+1];
    char ganzhi[50+1];//?
    char zodiac[6+1];
    char fitavoid[50+1];//?
    int time;
}sg_lunar_t;

#define WEATHER_CNT 3
typedef struct 
{
    char date[10+1];
    char weather[20+1];//?
    char temperature[30+1];//?
    char wind[20+1];//?
    char dress[20+1];//?
    char cur_temp[20+1];//?
    char airQuality[20+1];//
    char city[20+1];//?
}sg_weather_t;


#if 0  //jiule struct
//#define DATA_MAX_LENGTH	(1024+512)
//define in c file
//uint8 communication_send_data[DATA_MAX_LENGTH];
//uint8 communication_recv_data[DATA_MAX_LENGTH];

typedef enum{
    ALARM_SOS	=	0x0001,
    ALARM_02	=	0x0002,
    ALARM_03	= 	0x0003,
    ALARM_05	=	0x0005,

    TIME_CAL	=	0x0007,
    NORMAL		=	0x0008,

    UPDATE		=   0x000a,//10

    HEART_BEAT	=	0x000d,//13

    SLEEP_DATA	=	0x0016,//22
    QUERY_BASEDATA = 0x0017,//23
    REAL_TIME_DATA = 0x0018,//24

    MOTION		=	0x001f,//31

    ALARM_22	= 	0x0022,//34
    ALARM_23	= 	0x0023,//35
    ALARM_25	= 	0x0025,//37

    PRIVACY_SWITCH = 0x002e,//46

    QUERY_BLOODPRESSURE = 0x0030,//48

    BATTERY_FULL	= 0x0031,//49

    ALARM_3A	= 	0x003a,//58

//--------------------------------
    NET_CONNECT =   0x1000,//
    GET_SIM		=   0x2000,
    GET_IMEI	=	0x2001,
    GET_CSQ		=   0x3000,
    GET_GPS		=   0x4000,
    GET_WIFI_MAC=	0x4001,
    GET_GSV		=	0x4002,
    SWITCH_NET  = 	0x5000,
    GET_CURRENT_NET=0x6000,
    SDK_VER     =   0x7000,
    WRITE_AGPS	=	0x8000,
    RFCAL_FLAG  =   0x9000,
    NB_STATE	=	0xa000,
}CommandWord;

typedef enum
{
    REVERSE_LOCATION = 0x003a,
    ELEC_FENCE	= 0x003C,
    TRAJECTORY_TRACKINT = 0x003E,
    COMMON	= 0x0040,
}HeartBeat_Packet_type;

//========================================================
//====帧头
typedef struct  communication_packet_tag
{
    uint16  Start;       //0x5a5a
    uint8   SAddress;    //0x02
    uint8   DAddress;    //0x01
    uint16  Length;
    uint16  CmdWord;
    uint8   CmdParam[0];

}communication_packet, *p_communication_packet;

//===帧尾
typedef struct communication_packet_tail_tag
{
    uint32 timeStamp;
    uint8  sum;
    uint16 tail;    //0x0d0a
}communication_packet_tail, *p_communication_packet_tail;

//====================================================================
//=====================================================================
//-----时间校准�?
//----send to socket
typedef struct time_cal_param_packet_send_tag
{
    uint8 ccid[20];
    //uint8 sdk_ver[20];
    uint8 sdk_ver[10]; // [5]nordic sdk ver && [5] nb sdk ver
    uint8 custom[6];
    uint8 custom2;
    uint8 packet_tail[0];
}time_cal_param_packet_send, *p_time_cal_param_packet_send;

//---recv from socket
typedef struct time_cal_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 gender;
    uint8 age;
    uint8 height;
    uint8 weight;
    uint8 first_contact[16];
    uint8 second_contact[16];
    uint8 sdk_ver[10];//20->10

    uint8 watch_state[4];
    uint8 interval_mv;
    uint8 interval_fence;
    uint16 interval_heart;
    uint8 custom[20];

    uint8 packet_tail[0];
}time_cal_param_packet_recv, *p_time_cal_param_packet_recv;

//-------------------------------------------------------------------------
//-----报警数据�?
//---send
typedef struct alarm_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 data[0];
}alarm_param_packet_send, *p_alarm_param_packet_send;
#endif

//--------gps
typedef struct gps_group_data_tag
{
    uint8 gps_group_count;
    uint8 gps_datas[0];
}gps_group_data, *p_gps_group_data;

typedef struct gps_data_tag
{
    //uint8 data[25];
    uint8 rmc_longitude[10];
    uint8 rmc_EW;
    uint8 rmc_latitude[9];
    uint8 rmc_NS;
    uint32_t timeStamp;

    uint8 next[0];
}gps_data, *p_gps_data;

//-------wifi --------------------???
typedef struct wifi_group_data_tag
{
    uint8 wifi_group_count;
    uint8 data[0];
}wifi_group_data, *p_wifi_group_data;

typedef struct wifi_peer_group_data_tag
{
    //uint32 timeStamp;
    uint8 count;//一组数据中wifi个数
    uint8 wifi_data[0];
}wifi_peer_group_data, *p_wifi_peer_group_data;

typedef struct wifi_data_tag
{
    uint8 macAddr[6];
    uint8 strength;
    //uint32 timeStamp;

    uint8 next[0];
}wifi_data, *p_wifi_data;

typedef struct
{
    uint32_t timeStamp;
    uint8_t next[0];
}peer_group_timeStamp_t;

//--------lac------------------???
typedef struct lac_group_data_tag
{
    uint8 lac_group_count;
    uint8 lac_group_data[0];
}lac_group_data, *p_lac_group_data;


typedef struct lac_peer_group_data_tag
{
    uint8 lac_count;
    uint8 lac_data[0];
}lac_peer_group_data;

typedef struct lac_data_tag
{
    char mcc[3];//uint8
    char mnc[2];//
    uint16 lac;//char lac[2];//
    uint32 cellid;//char cellid[4];//cellid[2];//
    char rxlev;
    //uint32 timeStamp;

    uint8 next[0];
}lac_data, *p_lac_data;


//-------异常体征数据
typedef struct alarm_abnormal_data_tag
{
    uint8 data[20];
    uint8 packet_tail[0];//连接包尾数据
}alarm_abnormal_data, *p_alarm_abnormal_data;

//-------------------------------------------------------------------
//----normal数据�?
typedef struct normal_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 data_group_count;
    uint8 data[0];
}normal_param_packet_send, *p_normal_param_packet_send;

typedef struct watch_collect_data_tag
{
    uint8 blood_oxygen;
    uint8 heart_rate;
    uint8 breath;
    uint8 health_rate;
    uint8 activity[3];
    uint16 Kilometres;
    uint16 Calorie;
    uint8 state;
    uint16 effective_sleep_time;
    uint8 acc;
    uint32 unixTime;

    uint8 electric_quantity;
    uint8 signal_db;

    uint8 next[0];
}watch_collect_data,*p_watch_collect_data;


//---------------------------------------------------------------------
//----回复数据�?报警、睡眠数据回�?
typedef struct frame_response_recv_tag
{
    uint8 ccid[20];
    uint8 state;
    uint8 custom[20];

    uint8 packet_tail[0];
}frame_response_recv, *p_frame_response_recv;



//-------------------------------------------------------------------
//----心跳包数据帧
//----send
typedef struct heart_beat_param_packet_send_tag
{
    uint8 ccid[20];
    uint16 heart_beat_type;
    uint8 data[0];
}heart_beat_param_packet_send,*p_heart_beaet_param_packet_send;

// gps wifi lac

//隐私�?自定义数�?
typedef struct heart_beat_other_param_packet_send_tag
{
    uint8 privacy_state;
    uint8 custom[20];
    uint8 packet_tail[0];
}heart_beat_other_param_packet_send;

//------------------------
//---recv
typedef struct heart_beat_param_packet_recv_tag
{
    uint8 ccid[20];
    int8  temperature;
    int8  temper_high;
    int8  temper_low;
    int8  weather_detail;
    uint16 pm2_5;
    uint8 custom[20];

    uint8 packet_tail[0];
}heart_beat_param_packet_recv;

//-------------------------------------------------------------------
//------运动数据�?
//------send
typedef struct motion_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 motion_type;
    uint16 motion_time;
    uint16 motion_distance;
    uint16 calorie;
    uint16 speed;
    uint8 health_data[4];

    uint8 gps_count;
    uint8 gps_data[0];
}motion_param_packet_send;

//---???
typedef struct motion_begin_time_tag
{
    uint32 begin_time;

    uint8 packet_tail[0];
}motion_begin_time;

//--------recv
typedef struct motion_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 state;
    uint8 custom[20];

    uint8 packet_tail[0];
}motion_param_packet_recv;


//------------------------------------------------------------------
//------隐私开关数据帧 PRIVACY_SWITCH
//------send
typedef struct privacy_switch_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 switch_state;
    uint8 custom[20];
    uint8 packet_tail[0];
}privacy_switch_param_packet_send;


//-----recv
typedef struct privacy_switch_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 state;
    uint8 custom[20];

    uint8 packet_tail[0];
}privacy_switch_param_packet_recv;


//-----------------------------------------------------------------
//-------实时采集原始数据�?
//--------send
typedef struct real_time_data_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 real_time_raw_date[900];
    uint8 custom[20];

    uint8 packet_tail[0];
}real_time_data_param_packet_send;


//--------recv
typedef struct real_time_data_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 state;
    uint8 custom[20];

    uint8 packet_tail[0];
}real_time_data_param_packet_recv;


//----------------------------------------------------------------
//-------请求基准数据�?QUERY_BASEDATA
typedef struct query_base_data_param_packet_send_tag
{
    uint8 ccid[20];

    uint8 packet_tail[0];
}query_base_data_param_packet_send;


typedef struct query_base_data_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 base_data[800];
    uint8 custom[20];

    uint8 packet_tail[0];
}query_base_data_param_packet_recv;


//------------------------------------------------------------------
//------睡眠数据帧SLEEP_DATA
//---------send
typedef struct sleep_data_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 sleep_data[154];
    uint8 custom[20];

    uint8 packet_tail[0];
}sleep_data_param_packet_send;

//-----------recv
typedef struct sleep_data_param_packet_recv_tag
{
    uint8 ccid[20];
    uint8 state;
    uint8 custom[20];

    uint8 packet_tail[0];
}sleep_data_param_packet_recv;

//------------------------------------------------------------------
//------血压查询帧 QUERY_BLOODPRESSURE
//---------send
typedef struct query_blood_pressure_param_packet_send_tag
{
    uint8 ccid[20];
    uint8 custom[4];

    uint8 packet_tail[0];
}query_blood_pressure_param_packet_send;

//---------recv
typedef struct query_blood_pressure_param_packet_recv_tag
{
    uint8 ccid[20];
    uint16 blood_pressure_value;
    uint8 custom[6];

    uint8 packet_tail[0];
}query_blood_pressure_param_packet_recv;

//-----------------------------------------------------------------
//--------send
typedef struct
{
    uint8 ccid[20];
    uint8 charge_full_state;
    uint8 custom[6];

    uint8 packet_tail[0];
}charge_full_send_t;
//--------recv
typedef struct
{
    uint8 ccid[20];
    uint8 custom[21];

    uint8 packet_tail[0];
}charge_full_recv_t;

//------------------------------------------------------------------
//------over








//-----------------------------------------------------------------
//=================================================================
//----自定义命令（nordic�?621交互数据�?--------------------------

//send--------------------------
typedef struct nordic_net_connect_tag
{
    uint8 state;

    uint8 packet_tail[0];
}nordic_net_connect_t;
//reponse
typedef struct nordic_net_connect_response_tag
{
    uint8 state;
    uint8 packet_tail[0];
}nordic_net_connect_response_t;


//send--------------------------------
typedef struct nordic_get_sim_tag
{
    uint8 state;

    uint8 packet_tail[0];
}nordic_get_sim_t;
//response
typedef struct nordic_get_sim_response_tag
{
    char ccid[20];
    int32 csq;

    uint8 packet_tail[0];
}nordic_get_sim_response_t;
//send---------------------------------
typedef struct
{
    uint8 state;

    uint8 packet_tail[0];
}nordic_get_imei_t;
//response
typedef struct
{
    uint8 sn[15+1];
    uint8 imei[15+1];

    uint8 packet_tial[0];
}nordic_get_imei_response_t;
//send--------------------------------
typedef struct nordic_get_csq_tag
{
    uint8 state;

    uint8 packet_tail[0];
}nordic_get_csq_t;
//response
typedef struct nordic_get_csq_response_tag
{
    uint32 csq; //break into char csq; char net_type;//(nb,gsm) char signal_level;//(high,low) char custom;

    uint8 packet_tail[0];
}nordic_get_csq_response_t;

//send--------------------------------
typedef struct nordic_get_gps_tag
{
    uint8 state;

    uint8 packet_tail[0];
}nordic_get_gps_t;
//response
typedef struct nordic_get_gps_reponse_tag
{
    //some data
    uint8 rmc_longitude[10];
    uint8 rmc_EW;
    uint8 rmc_latitude[9];
    uint8 rmc_NS;
    //snr
    uint8 snr;

    uint8 packet_tail[0];
}nordic_get_gps_response_t;
//send-----------------------------------
typedef struct
{
    uint8 state;
    uint8 packet_tail[0];
}nordic_get_gsv_t;
//reponse
typedef struct
{
    uint8 gsv_states_in_view;
    struct
    {
        uint8 gsv_sate_id;
        uint8 gsv_sate_snr;
    }gsv_states_info[3];

    uint8 packet_tail[0];
}nordic_get_gsv_response_t;


//send--------------------------------------
typedef  struct
{
    uint8 type;

    uint8 packet_tail[0];
}nordic_switch_net_t;
//response=send

//send---------------------------------------
typedef  struct
{
    uint8 type;

    uint8 packet_tail[0];
}nordic_get_current_net_t;
//response=send

//send---------------------------------------
typedef struct
{
    uint8 type;

    uint8 packet_tail[0];
}nordic_get_sdk_ver_t;
//response
typedef struct
{
    char sdk_ver[5];//nb sdk version

    uint8 packet_tail[0];
}sdk_ver_response_t;
//-------------------------------------------
//send
typedef struct
{
    uint8 type;

    uint8 packet_tail[0];
}write_agps_t;
//response=send

//send
typedef struct
{
    uint8 type;

    uint8 packet_tail[0];
}nordic_get_rfcal_flag_t;
//response
typedef struct
{
    uint8 nb_flag;
    uint8 gsm_flag;
    uint16 nb_rfcal_flag;
    uint16 gsm_rfcal_flag;

    uint8 packet_tail[0];
}rfcal_flag_response_t;



//send
typedef struct
{
    uint8 type; //set or read
    uint8 state; //nb or ublox
    uint8 custom[50];

    uint8 packet_tail[0];
}nordic_get_and_set_nb_state_t;
//response  2625 state
typedef struct
{
    uint8 state_type; //nb state or ublox state
    uint8 result;
    uint8 custom[50];

    uint8 packet_tail[0];
}nb_state_rsp_t;


#pragma pack()


#endif
