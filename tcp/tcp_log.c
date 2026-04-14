/*
 * tcp_log.c
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */

#include <string.h>
#include <sys/param.h>
//#include "freertos/projdefs.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_rom_sys.h"
#include "project_specific.h"
#include "common_defines.h"
#if (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
	#include "mqtt_ctrl.h"
#endif
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO
	#include "tcp_server.h"
#endif
#if (COMM_PROTO & BLE_PROTO) == BLE_PROTO
	#include "esp_gatts_api.h"
	#include "ble_server.h"
	#include "btcomm.h"
#endif
#include "utils.h"
#include "tcp_log.h"

static int tcp_log_enable = 0;
static struct sockaddr_in srv_addr;
static char log_user[80];
TaskHandle_t tcp_log_task_handle = NULL;
QueueHandle_t tcp_log_evt_queue = NULL;
static void tcp_log_task(void *pvParameters);
static int reconnect(int *sock);
static int log_level = 1;
#define MSG_QUEUE_SIZE		5
#define MAX_LOG_LINE_SIZE	512
// Control marker for terminating the task (first 2 bytes)
#define TCP_LOG_CTL0 0x01
#define TCP_LOG_CTL1 0x02

int tcp_log_init(int level)
	{
	int ret = ESP_FAIL;
	int log_port;
	char *log_server;
#if TEST_BUILD == 1
	log_port = LOG_PORT_DEV;
	log_server = LOG_SERVER_DEV;
#else
	log_port = LOG_PORT_DEV;
	log_server = LOG_SERVER;
#endif
	snprintf(log_user, sizeof(log_user), "%s%02d", dev_conf.dev_name, dev_conf.dev_id);
	if(level >= CONSOLE_OFF && level <= CONSOLE_MQTT)
		log_level = level;
	if(log_level < CONSOLE_TCP)
		{
		// stop tcp_log task and delete tcp_log_evt_queue
		if(tcp_log_task_handle)
			{
			char message[MAX_LOG_LINE_SIZE] = {0};
			int i;
			message[0] = TCP_LOG_CTL0;
			message[1] = TCP_LOG_CTL1;
			(void)xQueueSend(tcp_log_evt_queue, message, ( TickType_t ) 10);
			for(i = 0; i < 10; i++)
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
			ret = ESP_OK;
		}
	else
		{
		if(tcp_log_enable == 1)
			ret = ESP_OK;
		else
			{
			//if(isConnected(NULL))
				{
				struct hostent *hent = NULL;
				bzero((char *) &srv_addr, sizeof(srv_addr));
				hent = gethostbyname(log_server);
				if(hent && hent->h_addr_list && hent->h_addr_list[0])
					{
					srv_addr.sin_port = htons(log_port);
					srv_addr.sin_family = AF_INET;
					srv_addr.sin_addr.s_addr = *(u_long *) hent->h_addr_list[0];
					if(!tcp_log_evt_queue)
						tcp_log_evt_queue = xQueueCreate(MSG_QUEUE_SIZE, MAX_LOG_LINE_SIZE);
					if(tcp_log_evt_queue)
						{
						if(!tcp_log_task_handle)
							{
							// IMPORTANT:
							// tcp_log_task stack intentionally limited to 2048 bytes if repl task.
							// Larger stacks (>~2300 bytes) cause ESP-MQTT disconnects
							// due to scheduling/keepalive timing interactions.
							// Logging is console-triggered only (low frequency).
	#ifdef WITH_CONSOLE
		#define SSIZE		2048
	#else
		#define SSIZE		4098
	#endif						
							xTaskCreate(tcp_log_task, "TCP_log_task", SSIZE, NULL, USER_TASK_PRIORITY, &tcp_log_task_handle);
							if(tcp_log_task_handle)
								{
								tcp_log_enable = 1;
								ret = ESP_OK;
								}
							else
								{
								vQueueDelete(tcp_log_evt_queue);
								tcp_log_evt_queue = NULL;
								ret = ESP_FAIL;
								}
							}
						else
							{
							tcp_log_enable = 1;
							ret = ESP_OK;
							}
						}
					else
						ret = ESP_FAIL;
					}
				else
					ret = ESP_FAIL;
				}
			//else
			//	ret = ESP_FAIL;
			}
		}
	//esp_rom_printf("tcplog init: ret = %d, tcp_log_enable = %d\n", ret, tcp_log_enable);
	return ret;
	}


void tcp_log_message(const char *message)
	{
	//tcp_log_init() should be removed from here and place in the housekeeping task
    if (message && log_level >= CONSOLE_TCP &&
    	tcp_log_init(-1) == ESP_OK)
    	{
	    char buf[MAX_LOG_LINE_SIZE] = {0};
		size_t used = strlcpy(buf, log_user, sizeof(buf));
  		used += strlcpy(buf + used, ":: ", sizeof(buf) - used);
	    strlcpy(buf + used, message, sizeof(buf) - used);
	    (void)xQueueSend(tcp_log_evt_queue, buf, (TickType_t)10);
		}
	}

static void tcp_log_task(void *pvParameters)
	{
	char buf[MAX_LOG_LINE_SIZE];
	int sendsock = -1;

	while(1)
		{
		if(xQueueReceive(tcp_log_evt_queue, buf, portMAX_DELAY) != pdTRUE)
			continue;
		if((uint8_t)buf[0] == TCP_LOG_CTL0 && (uint8_t)buf[1] == TCP_LOG_CTL1) // terminate task message
			{
			tcp_log_task_handle = NULL;
			tcp_log_enable = 0;
			if(sendsock >= 0) 
				close(sendsock);
			vTaskDelete(NULL);
			}
		if(log_level == CONSOLE_TCP)
			{
			size_t len = strnlen(buf, sizeof(buf));
			if (len > 0 && len < sizeof(buf) - 1 && buf[len - 1] != '\n') 
				{
            	buf[len++] = '\n';
            	buf[len] = '\0';
        		}
		
			if (sendsock < 0) 
				{
            	if (reconnect(&sendsock) < 0) 
                	continue;
	            }

			size_t written = 0;
			int sent = 0;
			size_t total = strnlen(buf, sizeof(buf));
			
			while (written < total)
				{
			    sent = send(sendsock, buf + written, total - written, 0);
				if (sent > 0)
			        written += (size_t)sent;
				else
					{
					if(sent < 0)
						{
						esp_rom_printf("TCP_log_task: send error %d\n", errno);
				    	if (errno != EWOULDBLOCK && errno != EAGAIN)
				        	{
				    		if(reconnect(&sendsock) < 0)
				    			sendsock = -1;
							}
						}
				    break;
				    }
				}
			}
#if COMM_PROTO & MQTT_PROTO			
		else if(log_level == CONSOLE_MQTT)
			publish_MQTT_client_log(buf);
#endif		
		}
	}

static int reconnect(int *sock)
	{
	int ret = ESP_FAIL;    
	if (!sock) 
		return ret;

    if (*sock >= 0) 
    	{
        close(*sock);
        *sock = -1;
	    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0)
    	{
	    // ---- send timeout (bounds send()) ----
	    struct timeval snd_to = { .tv_sec = 1, .tv_usec = 0 };
	    (void)setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &snd_to, sizeof(snd_to));
	
	    // ---- enable TCP keepalive ----
	    int ka = 1;
	    (void)setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
	
	    // ---- keepalive tuning (seconds) ----
	    int keep_idle  = 10;
	    int keep_intvl = 5;
	    int keep_cnt   = 3;
	    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE,  &keep_idle,  sizeof(keep_idle));
	    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
	    (void)setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT,   &keep_cnt,   sizeof(keep_cnt));
	
		if (connect(s, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) 
	        close(s);
		else
			{
			*sock = s;
			ret = ESP_OK;		
			}
		}
    return ret;
	}


