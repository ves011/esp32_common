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

#include "project_specific.h"

#define USER_TASK_PRIORITY			5

#define CONSOLE_TCP					2
#define CONSOLE_ON					1
#define CONSOLE_OFF					0


#define PIN_ON						(1)
#define PIN_OFF						(0)
#define PIN_DONT_TOUCH				(2)

#define TIMER_DIVIDER         		(80)  //  Hardware timer clock divider -> 1us resolution
#define TIMER_SCALE           		(TIMER_BASE_CLK / TIMER_DIVIDER /1000)  // convert counter value to mseconds

#define CLIENT_CONNECT_WDOG_G		TIMER_GROUP_0
#define CLIENT_CONNECT_WDOG_I		TIMER_0

#define BASE_PATH					"/var"
#define PARTITION_LABEL				"user"
#define CONSOLE_FILE				"conoff.txt"
#define PARAM_READ					(1)
#define PARAM_WRITE					(2)
#define PARAM_V_OFFSET				(1)
#define PARAM_LIMITS				(2)
#define PARAM_OPERATIONAL			(3)
#define PARAM_CONSOLE				(10)

#define DEVICE_TOPIC_Q				"gnetdev/query"
#define DEVICE_TOPIC_R				"gnetdev/response"


#if ACTIVE_CONTROLLER  == OTA_CONTROLLER
	#define DEV_NAME				"OTA"
#endif

#if ACTIVE_CONTROLLER == AGATE_CONTROLLER
	#define DEV_NAME				"Poarta Auto"
#endif

#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	#define DEV_NAME				"Pompa Foraj"
#endif

#define LOG_SERVER					"proxy.gnet"
#define LOG_SERVER_PORT				6001

#define FACTORY_PART_NAME			"factory"
#define OTA_PART_NAME				"ota_0"



#endif /* COMMON_COMMON_DEFINES_H_ */
