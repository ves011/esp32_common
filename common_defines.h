/*
 * common_defines.h
 *
 *  Created on: Dec 5, 2021
 *      Author: viorel_serbu
 */

#ifndef COMMON_COMMON_DEFINES_H_
#define COMMON_COMMON_DEFINES_H_

/** @brief control devices type */
#define AGATE_CONTROLLER				(1) //auto gate
#define PUMP_CONTROLLER					(2) //
#define OTA_CONTROLLER					(3) // OTA factory app
#define WESTA_CONTROLLER				(4) // weather station controller
#define WP_CONTROLLER					(6) //combined pump & water controller
#define ESP32_TEST						(7)
#define REMOTE_CTRL						(8)
#define NAVIGATOR						(9)


//#include "project_specific.h"

#define USER_TASK_PRIORITY			(5)

#define	MQTT_PROTO 					(1)
#define	TCPSOCK_PROTO				(2)

#define TCP_QUEUE_SIZE				50

typedef enum
	{
	CONSOLE_OFF,
	CONSOLE_ON,
	CONSOLE_TCP,
	CONSOLE_MQTT,
	} console_state_t;

#define PIN_ON						(1)
#define PIN_OFF						(0)
#define PIN_DONT_TOUCH				(2)

#define TIMER_DIVIDER         		(80)  //  Hardware timer clock divider -> 1us resolution
#define TIMER_BASE_CLK				(APB_CLK_FREQ)
#define TIMER_SCALE           		(TIMER_BASE_CLK / TIMER_DIVIDER /1000)  // convert counter value to mseconds

#define CLIENT_CONNECT_WDOG_G		(TIMER_GROUP_0)
#define CLIENT_CONNECT_WDOG_I		(TIMER_0)

#define BASE_PATH					"/var"
#define PARTITION_LABEL				"user"
#define CONSOLE_FILE				"conoff.txt"
#define HISTORY_FILE				"/sys/history.txt"
#define RGCAL_FILE					"rgcal.txt"
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
#define INACT_TIME			(0x40)
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

#define LOG_SERVER					"proxy.gnet"
#define LOG_SERVER_PORT				6001

#define FACTORY_PART_NAME			"ota_0"
#define OTA_PART_NAME				"ota_1"

typedef struct
		{
		uint32_t source;
		uint32_t val;
		union
			{
			int m_val[6];
			uint64_t ts;
			}vts;
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
	uint64_t ts;
	uint32_t cmd_id;
	union
		{
		char payload[192];
		uint8_t u8params[192];
		uint32_t u32params[48];
		position_t position;
		}p;
	} socket_message_t;
	
#endif /* COMMON_COMMON_DEFINES_H_ */
