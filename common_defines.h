/*
 * common_defines.h
 *
 *  Created on: Dec 5, 2021
 *      Author: viorel_serbu
 */

#ifndef COMMON_COMMON_DEFINES_H_
#define COMMON_COMMON_DEFINES_H_

/** @brief control devices type */
#include <stdint.h>

// device states
#define WIFI_CONNECTED					0x0001
#define MQTT_STARTED					0x0002
#define MQTT_CONNECTED					0x0004
#define TCP_LOG_ACTIVE					0x0008

//message sent when wfi connection changes
//iva[0] has connection status
#define MSG_WIFI						1

#define AGATE_CONTROLLER				(1) //auto gate
#define PUMP_CONTROLLER					(2) //
#define OTA_CONTROLLER					(3) // OTA factory app
#define WESTA_CONTROLLER				(4) // weather station controller
#define WP_CONTROLLER					(6) //combined pump & water controller
#define ESP32_TEST						(7)
#define REMOTE_CTRL						(8)
#define NAVIGATOR						(9)
#define FLOOR_HC						(10)
#define PUMP_CONTROLLER_SA				(11) // pump controller stand alone: BT communication, no mqtt broker, no local wifi
#define GENERIC_BT_GATTC				(12)
#define WMON_CONTROLLER					(13)
#define JOYSTICK						(14)
#define THERMOSTAT						(15)
#define DS18B20_ALIGNEMNT				(16)

#define USER_TASK_PRIORITY			(5)

#define	MQTT_PROTO 					(1)
#define	TCP_PROTO					(2)
#define BLE_PROTO					(4)

#define TCP_QUEUE_SIZE				50

typedef enum
	{
	CONSOLE_OFF,
	CONSOLE_ON,
	CONSOLE_TCP,
	CONSOLE_MQTT,
	CONSOLE_BTLE,
	} console_state_t;
	
// NVS device configuration entries 
#define NVS_CFG_NS					"devcfg"
#define NVSSTASSID					"wifi.stassid"
#define NVSSTAPASS					"wifi.stapass"
#define NVSDEVID					"devID"
#define NVSDEVNAME					"devname"
#define NVSCONSTATE					"console"
//#define NVSCACERT					"ca-crt"
//#define NVSDEVCERT				"client-crt"
//#define NVSDEVKEY					"client-key"
#define NVSAPHOSTNAME				"ap.hostname"
#define NVSAPSSID					"ap.ssid"
#define NVSAPPASS					"ap.pass"
#define NVSAPIP						"ap.ip"
#define MAXNVSKEYVALEN				32

//NVS certificates entries
#define NVS_CERT_NS					"certificates"
#define NVSCACRT					"ca-crt"
#define NVSCLCRT					"client-crt" 
#define NVSCLKEY					"client-key"

#define PIN_ON						(1)
#define PIN_OFF						(0)
#define PIN_DONT_TOUCH				(2)

#define TIMER_DIVIDER         		(80)  //  Hardware timer clock divider -> 1us resolution
#define TIMER_BASE_CLK				(APB_CLK_FREQ)
#define TIMER_SCALE           		(TIMER_BASE_CLK / TIMER_DIVIDER /1000)  // convert counter value to mseconds
/*
#define CLIENT_CONNECT_WDOG_G		(TIMER_GROUP_0)
#define CLIENT_CONNECT_WDOG_I		(TIMER_0)
*/
#define BASE_PATH					"/var"
#define PARTITION_LABEL				"user"
//#define CONSOLE_FILE				"conoff.txt"
//#define DEVCONF_FILE				"devconf.txt"
#define HISTORY_FILE				"/sys/history.txt"
#define RGCAL_FILE					"rgcal.txt"
#define PTST_FILE					"ptst.txt"
#define SETTINGS_FILE				"settings.txt"
/*
#define CONSOLE_TXT					"console state: "
#define NAME_TXT					"device name: "
#define ID_TXT						"device ID: "
#define STA_SSID_TXT				"STA SSID: "
#define STA_PWD_TXT					"STA SSID pass: "
#define AP_SSID_TXT					"AP SSID: "
#define AP_PWD_TXT					"AP SSID pass: "
#define AP_HOSTNAME_TXT				"AP hostname: "
#define AP_IP_TXT					"AP IP :"
*/

#define DEFAULT_CONSOLE_STATE		CONSOLE_ON
#define DEFAULT_DEVICE_NAME			"gnetdev"
#define DEFAULT_DEVICE_ID			81
#define DEFAULT_STA_SSID			""
#define DEFAULT_STA_PASS			""
#define DEFAULT_AP_SSID				"OTA-dev"
#define DEFAULT_AP_PASS				"OTA-devpass"
#define DEFAULT_AP_HOSTNAME			"ota-dev.local"
#define DEFAULT_AP_IP				(1<<24) | (252 <<16) | (168 << 8) | 192 //192.168.252.1
//#define DEFAULT_AP_A				192
//#define DEFAULT_AP_B				168
//#define DEFAULT_AP_C				253
//#define DEFAULT_AP_D				1


#define MAX_NO_FILES				(5)
#define PARAM_READ					(1)
#define PARAM_WRITE					(2)
#define PARAM_V_OFFSET				(1)
#define PARAM_LIMITS				(2)
#define PARAM_OPERATIONAL			(3)
#define PARAM_PNORM					(4)
#define PARAM_PROGRAM				(5)
#define PARAM_RGCAL					(6)
#define PARAM_CONSOLE				(10)


//sources for UI interface
#define K_ROT				(0x10)
#define K_ROT_LEFT			(0x11)
#define K_ROT_RIGHT			(0x12)
#define K_KEY				(0x20)
#define K_PRESS				(0x21)
#define K_DOWN				(0x22)
#define K_UP				(0x23)
#define K_REPEAT			(0x24)
#define INACT_TIME			(0x40)
#define CLOCK_TICK_1M		(0x41)
#define PUMP_VAL_CHANGE		(0x80)
#define PUMP_OP_ERROR		(0x81)

#define WATER_VAL_CHANGE	(0x100)
#define WATER_OP_ERROR		(0x101)
#define WATER_DV_OP			(0x102)
#define WATER_DV_CHANGE		(0x103)

#define BOOT_MSG			(0x200)

#define DEVICE_TOPIC_Q				"gnetdev/query"
#define DEVICE_TOPIC_R				"gnetdev/response"
#define DEVICE_TOPIC_L				"gnetdev/log"


/* -------------------------------------------------
 * Connectivity bits in event group
 *  -------------------------------------------------*/
/** Event group bit indicating Wi-Fi connectivity */
#define WIFI_CONNECTED_BIT  BIT0
/** Event group bit indicating IP address acquired */
#define IP_CONNECTED_BIT    BIT1
/** Event group bit indicating MQTT connection established */
#define MQTT_CONNECTED_BIT  BIT2


#define PI					3.14159265358979

/*
//definitions moved to project_specific.h
#if ACTIVE_CONTROLLER  == OTA_CONTROLLER
	#define DEV_NAME				"OTA"
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
	#define DEV_NAME				"Poarta Auto"
#elif ACTIVE_CONTROLLER == PUMP_CONTROLLER
	#define DEV_NAME				"Pompa Foraj"
#elif ACTIVE_CONTROLLER == WESTA_CONTROLLER
	#define DEV_NAME				"Statia meteo"
#elif ACTIVE_CONTROLLER == WATER_CONTROLLER
	#define DEV_NAME				"Controler irigatie"
#elif ACTIVE_CONTROLLER == WP_CONTROLLER
	#define DEV_NAME				"Pompa & Irigatie"
#elif ACTIVE_CONTROLLER == ESP32_TEST
	#define DEV_NAME				"esp32 test"
#endif
*/

#define FACTORY_PART_NAME			"ota_0"
#define OTA_PART_NAME				"ota_1"

typedef struct
		{
		uint32_t source;
		uint32_t val;
		union
			{
			int m_val[8];
			uint64_t ts;
			}vts;
		union
			{
			uint32_t uval[16];
			int ival[16];
			float fval[16];
			}ifvals;
		}msg_t;

typedef struct
	{
	uint32_t fix;
	uint32_t fix_mode;
	float latitude;
	float longitude;
	float altitude;
	} position_t;
	
typedef struct
	{
	int state;
	int status;
	int pmin;
	int press;
	int press_mv;
	int p0offset;
	int cmax;
	int current;
	int acs_offset;
	int q1000;
	int tw1000;
	} pump_mon_val_t;
	
typedef struct
	{
	uint64_t ts;
	uint32_t cmd_id;
	uint32_t pad;		//to keep 8 bytes alignment
	union
		{
		char payload[192];
		uint8_t u8params[192];
		uint32_t u32params[48];
		position_t position;
		pump_mon_val_t pv;
		}p;
	} socket_message_t;
	
typedef struct
	{
	console_state_t cs;
	int dev_id;
	char dev_name[40];
	char sta_ssid[40];
	char sta_pass[40];
	char ap_hostname[40];
	char ap_ssid[40];
	char ap_pass[40];
	uint32_t ap_ip;
	/*
	uint8_t ap_a;
	uint8_t ap_b;
	uint8_t ap_c;
	uint8_t ap_d;
	*/
	} dev_config_t;
	
typedef struct
	{
	int message;
	union
		{
		char strval[40];
		int ival[10];
		} val;
	} state_message_t;
	
#endif /* COMMON_COMMON_DEFINES_H_ */
