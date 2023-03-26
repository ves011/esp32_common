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
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_spiffs.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "mqtt_client.h"
#include "common_defines.h"
#include "external_defs.h"
#include "tcp_log.h"

static int tcp_log_enable;

#include "common_defines.h"
#include "tcp_log.h"
static 	struct sockaddr_in srv_addr;
static TaskHandle_t tcp_log_task_handle;
static xQueueHandle tcp_log_evt_queue;
static void tcp_log_task(void *pvParameters);
#define MSG_QUEUE_SIZE		20
int tcp_log_init(void)
	{
	int ret = ESP_FAIL;
	tcp_log_enable = 0;
	int sendsock;
	struct hostent *hent = NULL;
	bzero((char *) &srv_addr, sizeof(srv_addr));
	sendsock = socket(AF_INET, SOCK_STREAM, 0);
	if(sendsock >= 0)
		{
		hent = gethostbyname(LOG_SERVER);
		if(hent)
			{
			srv_addr.sin_port = htons(LOG_PORT);
			srv_addr.sin_family = AF_INET;
			srv_addr.sin_addr.s_addr = *(u_long *) hent->h_addr_list[0];
			tcp_log_evt_queue = xQueueCreate(MSG_QUEUE_SIZE, 1024);
			if(tcp_log_evt_queue)
				{
				if(xTaskCreate(tcp_log_task, "TCP_log_task", 4096, NULL, USER_TASK_PRIORITY, &tcp_log_task_handle) == pdPASS)
					{
					ret = ESP_OK;
					tcp_log_enable = 1;
					}
				else
					vQueueDelete(tcp_log_evt_queue);
				}
			}
		close(sendsock);
		}
	return ret;
	}
int tcp_log_message(char *message)
	{
	char buf[1024];
	if(tcp_log_enable)
		{
		strcpy(buf, USER_MQTT);
		strcat(buf, ":: ");
		strncat(buf, message, 1023 - 2 - strlen(USER_MQTT) );
		xQueueSend(tcp_log_evt_queue, buf, ( TickType_t ) 10);
		}
	return 0;
	}
static void tcp_log_task(void *pvParameters)
	{
	char buf[1024];
	while(1)
		{
		xQueueReceive(tcp_log_evt_queue, buf, portMAX_DELAY);
		int sendsock = socket(AF_INET, SOCK_STREAM, 0);
		int written = 0;
		if (sendsock >= 0)
			{
			if(connect(sendsock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) >= 0)
				{
				while(written < strlen(buf))
					{
					int sent = send(sendsock, buf + written, strlen(buf), 0);
					if (sent < 0)
						break;
					else
						written += sent;
					}
				}
			close(sendsock);
			}
		}
	}

