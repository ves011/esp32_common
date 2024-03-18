/*
 * common_defines.h
 *
 *  Created on: Dec 5, 2021
 *      Author: viorel_serbu
 */

#ifndef COMMON_COMMON_DEFINES_H_
#define COMMON_COMMON_DEFINES_H_

/** @brief control devices type */
#define AGATE_CONTROLLER				1 //auto gate
#define PUMP_CONTROLLER					2 //pump controller
#define OTA_CONTROLLER					3 // OTA factory app
#define WESTA_CONTROLLER				4 // weather station controller
#define WATER_CONTROLLER				5 // irrigation and water actuators control
#define WP_CONTROLLER					6 //combined pump & water controller

#include "project_specific.h"

#define USER_TASK_PRIORITY			5

#define CONSOLE_TCP					2
#define CONSOLE_ON					1
#define CONSOLE_OFF					0


#define PIN_ON						(1)
#define PIN_OFF						(0)
#define PIN_DONT_TOUCH				(2)

#define TIMER_DIVIDER         		(80)  //  Hardware timer clock divider -> 1us resolution
#define TIMER_BASE_CLK				APB_CLK_FREQ
#define TIMER_SCALE           		(TIMER_BASE_CLK / TIMER_DIVIDER /1000)  // convert counter value to mseconds

#define CLIENT_CONNECT_WDOG_G		TIMER_GROUP_0
#define CLIENT_CONNECT_WDOG_I		TIMER_0

#define BASE_PATH					"/var"
#define PARTITION_LABEL				"user"
#define CONSOLE_FILE				"conoff.txt"
#define HISTORY_FILE				"/sys/history.txt"
#define MAX_NO_FILES				(5)
#define PARAM_READ					(1)
#define PARAM_WRITE					(2)
#define PARAM_V_OFFSET				(1)
#define PARAM_LIMITS				(2)
#define PARAM_OPERATIONAL			(3)
#define PARAM_PNORM					(4)
#define PARAM_PROGRAM				(5)
#define PARAM_CONSOLE				(10)

//sources for UI interface
#define K_ROT				10
#define K_ROT_LEFT			11
#define K_ROT_RIGHT			12
#define K_PRESS				20
#define K_DOWN				21
#define K_UP				22
#define INACT_TIME			30
#define PUMP_VAL_CHANGE		40
#define PUMP_OP_ERROR		50
#define WATER_VAL_CHANGE	60
#define WATER_OP_ERROR		70

#define DEVICE_TOPIC_Q				"gnetdev/query"
#define DEVICE_TOPIC_R				"gnetdev/response"


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
	#define DEV_NAME				"Controler irigatie&pompa"
#endif

#define LOG_SERVER					"proxy.gnet"
#define LOG_SERVER_PORT				6001

#define FACTORY_PART_NAME			"factory"
#define OTA_PART_NAME				"ota_0"

typedef struct
		{
		uint32_t source;
		uint32_t val;
		int m_val[6];
		}msg_t;


#endif /* COMMON_COMMON_DEFINES_H_ */
