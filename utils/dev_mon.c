/*
 * dev_mon.c
 *
 *  Created on: Mar 9, 2025
 *      Author: viorel_serbu
 */

#include <string.h>
#include <stdio.h>
#include "project_specific.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "common_defines.h"
#include "mqtt_ctrl.h"
#include "tcp_log.h"
#include "cmd_wifi.h"
#include "utils.h"
#include "dev_mon.h"

QueueHandle_t dev_mon_queue = NULL;
uint32_t device_state;
static uint64_t int_tick;
static uint32_t wifi_connected = 0;

static char *TAG = "MONTASK";
#define MAX_EVT_HANDLERS		10
/*
timer_handler matrix - timer_handler[i][j]
max 10 events: i
max 10 handlers per event: j
each 1 sec tick dev_mon_task() is looking if there is a handler on ith line of the matrix
i = tick %10 
j = index in timer_handler[i]* of a non null function
*/
static void (*timer_handler[10][MAX_EVT_HANDLERS])(msg_t *msg);

static void mon_timer_callback(void* arg)
	{
	msg_t msg;
	int_tick++;
	msg.source = MSG_TIMER_MON;
	msg.val = int_tick;
	xQueueSendFromISR(dev_mon_queue, &msg, NULL);
	}
//void wifi_con_retry(msg_t *msg)
//	{
//	wifi_reconnect();
//	}
int add_handler(int ith, void *func)
	{
	int i;
	for(i = 0; i < MAX_EVT_HANDLERS; i++)
		{
		if(timer_handler[ith][i] == func)
			return ESP_OK;
		}
	if(i == MAX_EVT_HANDLERS) // add handler on first available place
		{
		for(i = 0; i < MAX_EVT_HANDLERS; i++)
			{
			if(timer_handler[ith][i] == NULL)
				{
				timer_handler[ith][i] = func;
				ESP_LOGE(TAG, "install handler on line %d / %d", ith, i);
				return ESP_OK;
				}
			}
		}
	ESP_LOGE(TAG, "Cannot install handler on line %d", ith);
	return ESP_FAIL;
	}
int remove_handler(int ith, void *func)
	{
	for(int i = 0; i < MAX_EVT_HANDLERS; i++)
		{
		if(timer_handler[ith][i] == func)
			{
			timer_handler[ith][i] = NULL;
			ESP_LOGI(TAG, "removed handler on line %d / %d", ith, i);
			return ESP_OK;
			}
		}
	ESP_LOGI(TAG, "No handler found on line %d", ith);
	return ESP_FAIL;
	}
void dev_mon_task(void *pvParameters)
	{
	state_message_t msg;
	/*
	int_tick = 0;
	int i, j;
	for(int i = 0; i < 10; i++)
		for(j = 0; j < MAX_EVT_HANDLERS; j++)
			timer_handler[i][j] = NULL;
	*/
	esp_timer_create_args_t mon_timer_args = 
		{
    	.callback = &mon_timer_callback,
        .name = "mon_timer"
    	};
	dev_mon_queue = xQueueCreate(10, sizeof(msg_t));
	if(!dev_mon_queue)
		{
		ESP_LOGE(TAG, "Cannot create dev_mon_queue");
		esp_restart();
		}
	esp_timer_handle_t mon_timer;
	ESP_ERROR_CHECK(esp_timer_create(&mon_timer_args, &mon_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(mon_timer, 1000000)); // 1sec period
	while(1)
		{
		if(xQueueReceive(dev_mon_queue, &msg, portMAX_DELAY))
			{
			switch(msg.message)
				{
				case MSG_WIFI:
					wifi_connected = msg.val.ival[0];
					if(wifi_connected)
						{
						device_state |= WIFI_CONNECTED;
						if(COMM_PROTO == MQTT_PROTO)
							{
							mqtt_start();
							}
						if(dev_conf.cs == CONSOLE_TCP)
							{ 
							if(COMM_PROTO == MQTT_PROTO)
								{
								if(device_state & MQTT_CONNECTED)
									tcp_log_init(CONSOLE_MQTT);
								}
							else
								tcp_log_init(CONSOLE_TCP);
							}
						}
					break;
				default:
					break;
				}	
			}
		}
	}