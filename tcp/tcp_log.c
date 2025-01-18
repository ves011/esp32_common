/*
 * tcp_log.c
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */

#include <string.h>
#include <sys/param.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_spiffs.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "lwip/sockets.h"
#include "driver/i2c_master.h"
#include "project_specific.h"
#include "common_defines.h"
#if COMM_PROTO == MQTT_PROTO
	#include "mqtt_client.h"
	#include "mqtt_ctrl.h"
#endif
#include "external_defs.h"
#include "tcp_log.h"

static int tcp_log_enable = 0;
static 	struct sockaddr_in srv_addr;
static char log_user[32];
TaskHandle_t tcp_log_task_handle = NULL;
QueueHandle_t tcp_log_evt_queue = NULL;
static void tcp_log_task(void *pvParameters);
#define MSG_QUEUE_SIZE		5
#define MAX_LOG_LINE_SIZE	1024

int tcp_log_init()
	{
	int ret = ESP_FAIL;
	int log_port;
	char *log_server;
#ifdef TEST_BUILD
	log_port = LOG_PORT_DEV;
	log_server = LOG_SERVER_DEV;
#else
	log_port = LOG_PORT;
#endif
#if COMM_PROTO == MQTT_PROTO
	strcpy(log_user, USER_MQTT)
#else
	strcpy(log_user, DEV_NAME);
#endif
	//printf("\ntcp_log_init %d", console_state);
	if(console_state == CONSOLE_TCP)
		{
		struct hostent *hent = NULL;
		bzero((char *) &srv_addr, sizeof(srv_addr));
		hent = gethostbyname(log_server);
		if(hent)
			{
			srv_addr.sin_port = htons(log_port);
			srv_addr.sin_family = AF_INET;
			srv_addr.sin_addr.s_addr = *(u_long *) hent->h_addr_list[0];
			}
		}
	if(console_state < CONSOLE_TCP)
		{
		if(tcp_log_task_handle)
			{
			char message[4];
			message[0] = 0x01;
			message[1] = 0x02;
			message[3] = 0;
			xQueueSend(tcp_log_evt_queue, message, ( TickType_t ) 10);
			}
		for(int i = 0; i < 10; i++)
			{
			if(tcp_log_task_handle == NULL)
				break;
			vTaskDelay(10 / portTICK_PERIOD_MS);
			}
		if(tcp_log_task_handle == NULL)
			{
			if(tcp_log_evt_queue)
				vQueueDelete(tcp_log_evt_queue);
			tcp_log_evt_queue = NULL;
			tcp_log_enable = 0;
			ret = ESP_OK;
			}
		}
	else
		{
		if(tcp_log_enable == 1)
			ret = ESP_OK;
		else
			{
			if(!tcp_log_evt_queue)
				tcp_log_evt_queue = xQueueCreate(MSG_QUEUE_SIZE, MAX_LOG_LINE_SIZE);
			if(tcp_log_evt_queue)
				{
				if(!tcp_log_task_handle)
					{
					xTaskCreate(tcp_log_task, "TCP_log_task", 4096, NULL, USER_TASK_PRIORITY, &tcp_log_task_handle);
					if(tcp_log_task_handle)
						{
						ret = ESP_OK;
						tcp_log_enable = 1;
						}
					else
						{
						vQueueDelete(tcp_log_evt_queue);
						tcp_log_evt_queue = NULL;
						tcp_log_enable = 0;
						}
					}
				else
					tcp_log_enable = 1;
				}
			else
				tcp_log_enable = 0;
			}
		}
	//printf("\ntcp_log_enable: %d, tcp_log_evt_queue: %p, tcp_log_task_handle: %p\n", tcp_log_enable, tcp_log_evt_queue, tcp_log_task_handle);
	return ret;
	}

int tcp_log_message(char *message)
	{
	if(!tcp_log_enable)
		tcp_log_init();
	if(tcp_log_enable)
		{
		if(console_state == CONSOLE_MQTT)
			xQueueSend(tcp_log_evt_queue, message, ( TickType_t ) 10);
		else if(console_state == CONSOLE_TCP)
			{
			char buf[1024];
			//if !tcp_log_enable try to init first
			if(!tcp_log_enable)
				tcp_log_init(TRANSPORT_TCP);
			if(tcp_log_enable)
				{
				strcpy(buf, log_user);
				strcat(buf, ":: ");
				strncat(buf, message, MAX_LOG_LINE_SIZE - 3 - strlen(log_user) );
				xQueueSend(tcp_log_evt_queue, buf, ( TickType_t ) 10);
				}
			}
		}
	return 1;
	}

static void tcp_log_task(void *pvParameters)
	{
	char buf[1024];
	int sendsock = socket(AF_INET, SOCK_STREAM, 0);
	/*
	if(connect(sendsock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
		{
		tcp_log_task_handle = NULL;
		tcp_log_enable = 0;
		vTaskDelete(NULL);
		}
		*/
	while(1)
		{
		xQueueReceive(tcp_log_evt_queue, buf, portMAX_DELAY);
		if(buf[0] == 0x01 && buf[1] == 0x02) // terminate task message
			{
			tcp_log_task_handle = NULL;
			tcp_log_enable = 0;
			vTaskDelete(NULL);
			}
#if COMM_PROTO == MQTT_PROTO
		if(console_state == CONSOLE_MQTT)
			{
			publish_MQTT_client_log(buf);
			}
		else
#endif
			{
			if(buf[strlen(buf) - 1] != '\n')
			strcat(buf, "\n");
			int written = 0;
			int sent = 0;
			int retry = 0;
			if (sendsock >= 0)
				{
				while(written < strlen(buf) && retry < 3)
					{
					sent = send(sendsock, buf + written, strlen(buf), 0);
					if (sent < 0)
						{
						close(sendsock);
						sendsock = socket(AF_INET, SOCK_STREAM, 0);
						if(sendsock < 0)
							{
							retry++;
							continue;
							}
						if(connect(sendsock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
							{
							retry++;
							continue;
							}
						}
					else
						written += sent;
					}
				}
			}
		}
	}

