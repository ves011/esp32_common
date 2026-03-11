/*
 * dev_mon.h
 *
 *  Created on: Mar 9, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_UTILS_DEV_MON_H_
#define ESP32_COMMON_UTILS_DEV_MON_H_

extern QueueHandle_t dev_mon_queue;

void dev_mon_task(void *pvParameters);


#endif /* ESP32_COMMON_UTILS_DEV_MON_H_ */
