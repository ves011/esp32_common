/*
 * tcp_server.c
 *
 *  Created on: Dec 5, 2024
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "bignum_mod.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "common_defines.h"
#include "cmd_wifi.h"
#include "process_message.h"
#include "tcp_server.h"

static const char *TAG ="tcp_server";
static int commstate = IDLE;
static int send_error = 0;

TaskHandle_t process_message_task_handle = NULL, send_task_handle = NULL;
QueueHandle_t tcp_receive_queue = NULL, tcp_send_queue = NULL;

static void process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	while(1)
		{
		if(xQueueReceive(tcp_receive_queue, &msg, portMAX_DELAY))
			{
			process_message(msg);
			//ESP_LOGI(TAG, "Message received cmd_id %lu / %lu", msg.cmd_id, msg.p.u32params[0]);
			}
		}
	}

static void send_task(void *pvParameters)
	{
	socket_message_t msg;
	int written, ret;
	int sock = ((tParams_t *)pvParameters)->socket;
	xQueueReset(tcp_send_queue);
	ESP_LOGI(TAG, "send_task started, socket: %d", sock);
	send_error = 0;
	while(1)
		{
		if(xQueueReceive(tcp_send_queue, &msg, portMAX_DELAY))
			{
			
			written = 0, ret = 0;
			while(written < sizeof(socket_message_t))
				{
	        	ret = send(sock, (uint8_t *)(&msg + written), sizeof(socket_message_t) - written, 0);
				if (ret < 0) 
	            	{
					send_error = 1;
	                ESP_LOGE(TAG, "Error occurred during sending message: errno %d", errno);
	                break;
	            	}
				else
					written += ret;
				}
			if(ret <= 0)
				{
				// need to handle here error while sending
				// or its enough recv() error handling in tcp_server_task ???
				}
			}
		}
	}

void tcp_server(void *pvParameters)
	{
	int listen_sock;
	int server_sock;
	char addr_str[128];
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;
    struct sockaddr_in localbind;
    int written, len, ret;
    socket_message_t message;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(DEFAULT_TCP_SERVER_PORT);
    ip_protocol = IPPROTO_IP;
		
    listen_sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
    if (listen_sock >= 0)
    	{
    	int opt = 1;
		setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		ESP_LOGI(TAG, "Socket created");
		ret = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (ret == 0)
			{
			ESP_LOGI(TAG, "Socket bound, port %d", DEFAULT_TCP_SERVER_PORT);
			ret = listen(listen_sock, 1);
			if (ret == 0)
				{
				while (1) // accept loop
					{
					commstate = IDLE;
					ESP_LOGI(TAG, "Socket listening");
					struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
					socklen_t addr_len = sizeof(source_addr);
					server_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
					if(server_sock >= 0)
						{ 
						ESP_LOGI(TAG, "server task socket %d", server_sock);
						int no_delay = 1;
						setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));

						int keepAlive = 1;
						int keepalive_intvl = 1;
						int keepalive_count = 3;
						int keepalive_idle = 1;
						setsockopt(server_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof (int) );
						setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(int));
						setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(int));
						setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof(int));

						getsockname(server_sock, (struct sockaddr *)&localbind, (socklen_t *)&len);
						inet_ntoa_r(localbind.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
						ESP_LOGI(TAG, "local bind %s:%d", addr_str, ntohs(localbind.sin_port));
						// Convert ip address to string
						if (source_addr.ss_family == PF_INET) // only IPV4
							{
							addr_str[0] = 0;
							inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
							ESP_LOGI(TAG,
								"Socket accepted ip address: %s:%d", addr_str, ntohs(((struct sockaddr_in *)&source_addr)->sin_port)) ;
							while(1) //communication loop
								{
								len = recv(server_sock, &message, sizeof(socket_message_t), MSG_WAITALL);
								if (len < 0)
									{
									ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
									close(server_sock);
									break;
									}
								else if (len == 0) //socket closed by client
									{
									ESP_LOGI(TAG, "socket closed by peer");
									close(server_sock);
									break;
									}
								else //process message based on cmd_id && commstate
									{
									if(commstate == IDLE)
										{
										if(message.cmd_id == COMMINIT && message.ts == TSINIT)
											{
											ESP_LOGI(TAG, "COMMINIT received");
											if(xTaskCreate(process_message_task, "process_message", 8192, NULL, 4, &process_message_task_handle) == pdPASS)
												{
												tParams_t tParams;
												tParams.socket = server_sock;
												if(xTaskCreate(send_task, "send_message_task", 8192, &tParams, 4, &send_task_handle) == pdPASS)
													{
													message.ts = esp_timer_get_time();
													message.cmd_id = COMMACK;
													len = 0;
													while(len < sizeof(socket_message_t))
														{
										            	ret = send(server_sock, (uint8_t *)(&message + len), sizeof(socket_message_t) - len, 0);
										            	if (ret < 0) 
											            	{
											                ESP_LOGE(TAG, "Error occurred during sending COMMACK: errno %d", errno);
											                break;
											            	}
											            else
															len += ret;
														}
													if(ret < 0)
														{
														break;	
														}
													commstate = CONNECTED;
													ESP_LOGI(TAG, "COMMACK sent comm state == CONNECTED");
													}
												else 
													{
													ESP_LOGE(TAG, "Unable to start send_message_task");
													esp_restart();
													}
												}
											else 
												{
												ESP_LOGE(TAG, "Unable to start process_message_task");
												esp_restart();
												}
											}
										else // message received but commstate != IDLE
											 // send back COMNACK
											{
											message.ts = 0xffffffffffffffffULL;
											message.cmd_id = COMMNACK;
											written = 0;
											while(written < sizeof(socket_message_t))
												{
								            	ret = send(server_sock, (uint8_t *)(&message + written), sizeof(socket_message_t) - written, 0);
								            	if (ret < 0) 
									            	{
									                ESP_LOGE(TAG, "Error occurred during sending COMMNACK: errno %d", errno);
									                break;
									            	}
									            else
													written += ret;
												}
											if(ret < 0)
												{
												break;	
												}
											}
										}
									else //commstate == CONNECTED
										 //just put the message in the queue
										{
										int nmsg = uxQueueMessagesWaiting(tcp_receive_queue);
										if(nmsg >= TCP_QUEUE_SIZE)
											{
											xQueueReset(tcp_receive_queue);
											ESP_LOGI(TAG, "receive queue reset %d", nmsg);
											}
										//posting the message, triggers process_message task
										xQueueSend(tcp_receive_queue, &message, 0);
										}
									}
								}
							}
						if(process_message_task_handle)
							{
							vTaskDelete(process_message_task_handle);
							process_message_task_handle = NULL;
							}
						if(send_task_handle)
							{
							vTaskDelete(send_task_handle);
							send_task_handle = NULL;
							}
						}
					else
						{
						ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
						break;
						}
					}
				}
			else
				{
				ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
				}
			}
		else
			{
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
			}
    	}
    else
    	{
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    	}
	vTaskDelete(NULL);
	}

void register_tcp_server()
	{
	tcp_receive_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_receive_queue)
    	{
		ESP_LOGE(TAG, "Unable to create tcp_receive_queue");
		esp_restart();
		}
	tcp_send_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_send_queue)
    	{
		ESP_LOGE(TAG, "Unable to create tcp_send_queue");
		esp_restart();
		}
	if(xTaskCreate(tcp_server, "TCP server", 8192, NULL, 5, NULL) != pdPASS)
		{
		ESP_LOGE(TAG, "Cannot create tcp_server_task");
		esp_restart();
		}	
	}

