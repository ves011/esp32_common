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

//#define ACTIVE_CONTROLLER			AGATE_CONTROLLER
//#define ACTIVE_CONTROLLER			PUMP_CONTROLLER
#define ACTIVE_CONTROLLER			OTA_CONTROLLER
#define CTRL_DEV_ID					1

#define USER_TASK_PRIORITY			5

#define CONSOLE_MQTT				2
#define CONSOLE_ON					1
#define CONSOLE_OFF					0


#define PIN_ON						(1)
#define PIN_OFF						(0)
#define PIN_DONT_TOUCH				(2)

#if ACTIVE_CONTROLLER == HEATER_CONTROLLER
	#define NOOFHEATERS					(2)
	#define TEMP_MAX_DEVICES        	(8)
#endif

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

/*
 * topics base definition and USR_MQTT
 * prefix + 2 digit ctrl_id
 * pump + 01 for pump with CTRL_DEV_ID = 1
 */

#if ACTIVE_CONTROLLER  == OTA_CONTROLLER
	#define TOPIC_STATE				"ota/state"
	#define TOPIC_MONITOR			"NA"
	#define TOPIC_CMD				"ota/cmd"
	#define TOPIC_CTRL				"ota/ctrl"
	#define TOPIC_SYSTEM			"ota/system"
	#define USER_MQTT				"OTA"
	#define DEV_NAME				"OTA"
#endif

#if ACTIVE_CONTROLLER == AGATE_CONTROLLER
	#define TOPIC_STATE				"gate01/state"
	#define TOPIC_MONITOR			"gate01/monitor"
	#define TOPIC_CMD				"gate01/cmd"
	#define TOPIC_CTRL				"gate01/ctrl"
	#define TOPIC_SYSTEM			"gate01/system"
	#define USER_MQTT				"gate01"
	#define DEV_NAME				"Poarta Auto"
#endif

#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	#define TOPIC_STATE				"pump01/state"
	#define TOPIC_MONITOR			"pump01/monitor"
	#define TOPIC_CMD				"pump01/cmd"
	#define TOPIC_CTRL				"pump01/ctrl"
	#define TOPIC_SYSTEM			"pump01/system"
	#define USER_MQTT				"pump01"
	#define DEV_NAME				"Pompa Foraj"
#endif

#define LOG_SERVER					"proxy.gnet"
#define LOG_SERVER_PORT				6001



#endif /* COMMON_COMMON_DEFINES_H_ */
