/*
 * rot_enc.h
 *
 *  Created on: Mar 3, 2024
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_UTILS_ROT_ENC_H_
#define ESP32_COMMON_UTILS_ROT_ENC_H_

#define SOURCE_ROT			1
#define SOURCE_KEY			2
#define SOURCE_TIMER		3

#define PUSH_TIME_SHORT		100000			//300 msec
#define PUSH_TIME_LONG		2000000		// 2 sec

#define KEY_PRESS_SHORT		1
#define KEY_PRESS_LONG		2

void init_rotenc();



#endif /* ESP32_COMMON_UTILS_ROT_ENC_H_ */
