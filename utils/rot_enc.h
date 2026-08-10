/*
 * rot_enc.h
 *
 *  Created on: Mar 3, 2024
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_UTILS_ROT_ENC_H_
#define ESP32_COMMON_UTILS_ROT_ENC_H_

#include "freertos/idf_additions.h"
#define SOURCE_ROT			1
#define SOURCE_KEY			2
#define SOURCE_TIMER		3
#define ROT_INACTIVITY_TIMER	4

#define PUSH_TIME_SHORT		  100000			//100 msec
#define PUSH_TIME_LONG		 2000000			// 2 sec
#define PUSH_TIME_LONGLONG	10000000			// 10 sec
#define PUSH_KEY_REPEAT		  500000			// 300 msec
#define ROT_INACTIVITY_TIME	30			    	//30 sec


//#define KEY_PRESS_SHORT		1
//#define KEY_PRESS_LONG		2

#define KEY_PRESSED			1
#define KEY_RELEASED		0

//sources for UI interface
#define K_ROT				(0x10)
#define K_ROT_LEFT			(0x11)
#define K_ROT_RIGHT			(0x12)
#define K_KEY				(0x20)
#define K_PRESS				(0x21)
#define K_DOWN				(0x22)
#define K_UP				(0x23)
#define K_REPEAT			(0x24)


/*
 * handles events from a rotary encoder device
 * s1, s2 -> sense rotation of knob
 * -- provides events for
 * --- left turn
 * --- right turn
 * key -> knob press
 *  -- provides events for
 *  ---- key down
 *  ---- key up
 *  ---- short time pressed
 *  ---- long time pressed
 */

void init_rotenc(QueueHandle_t ui_cmd_q);



#endif /* ESP32_COMMON_UTILS_ROT_ENC_H_ */
