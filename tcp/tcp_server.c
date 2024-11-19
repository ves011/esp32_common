/*
 * tcp_server.c
 *
 *  Created on: Oct 20, 2024
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "bignum_mod.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "common_defines.h"
#include "cmd_wifi.h"

#include "tcp_server.h"

TaskHandle_t tcp_server_handle, comm_client_handle = NULL, comm_cmd_handle = NULL;//mon_task_handle, cmd_task_handle, server_task_handle, bcl_task_handle;
static const char *TAG = "TCP server";

static uint64_t commTS;

static int listen_sock;
static int listen_port;
ip_addr_t 	connect_dev_ip;
uint16_t	connected_dev_port;
ip_addr_t	log_server_ip;
uint16_t	log_server_port;
int server_sock;

QueueHandle_t msg2remote_queue = NULL;

void comm_client(void *pvParams)
	{
	struct sockaddr_storage dest_addr;
    int written = 0;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    int sockfd;
    char addr_str[80];
	socket_message_t qmsg, *message = (socket_message_t *)pvParams;

	//ip of the connected device = message.params[0] ... [3]
	//listening port of the connected device =  message.params[4] ... [5]
	//ip address of the log server  = message.params[6] ... [9]
	//listening port of the log server =  message.params[10] ... [11]
	IP4_ADDR(&connect_dev_ip.u_addr.ip4, message->params[0], message->params[1], message->params[2], message->params[3]);
	connected_dev_port = ((message->params[4] << 8) & 0xff00) | message->params[5];
	IP4_ADDR(&log_server_ip.u_addr.ip4, message->params[6], message->params[6], message->params[8], message->params[9]);
	log_server_port = ((message->params[10] << 8) & 0xff00) | message->params[11];
	ESP_LOGI(TAG, "%d.%d.%d.%d %d %d / %d.%d.%d.%d %d %d",
			message->params[0], message->params[1], message->params[2], message->params[3], message->params[4], message->params[5],
			message->params[6], message->params[6], message->params[8], message->params[9], message->params[10], message->params[11]);
	ESP_LOGI(TAG, "%lx %x / %lx %x", connect_dev_ip.u_addr.ip4.addr, connected_dev_port,
			log_server_ip.u_addr.ip4.addr, log_server_port);
	ESP_LOGI(TAG, "Init message received ts: %llu", commTS);
	dest_addr_ip4->sin_addr.s_addr = connect_dev_ip.u_addr.ip4.addr;
	dest_addr_ip4->sin_family = AF_INET;
	dest_addr_ip4->sin_port = connected_dev_port;
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sockfd < 0)
		{
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		esp_restart();
		}
	addr_str[0] = 0;
	inet_ntoa_r(dest_addr_ip4->sin_addr, addr_str, sizeof(addr_str) - 1);
	ESP_LOGI(TAG,
			"try to connect to: %s:%d", addr_str, ntohs(dest_addr_ip4->sin_port)) ;
	if (connect(sockfd, (struct sockaddr *)dest_addr_ip4, sizeof(struct sockaddr_in)) != 0)
		{
		ESP_LOGI(TAG, "cannot connect %d", errno);
		vTaskDelete(NULL);
		return;
		}
	if(sockfd > 0)
		{
		qmsg.ts = esp_timer_get_time();
		qmsg.cmd_id = CONACK;
		written = send(sockfd, &qmsg,  sizeof(socket_message_t), 0);
		close(sockfd);
		}
	ESP_LOGI(TAG, "Client connection written: ts - %llu / cmd-id - %lu / %d", qmsg.ts, qmsg.cmd_id, written);
	if(!msg2remote_queue)
		{
		msg2remote_queue = xQueueCreate(5, sizeof(socket_message_t));
		if(!msg2remote_queue)
			{
			ESP_LOGI(TAG, "cannot create client message queue");
			esp_restart();
			}
		}

	while(1)
		{
		if(xQueueReceive(msg2remote_queue, &qmsg, portMAX_DELAY))
			{
			if(qmsg.cmd_id == TERMINATESELF)
				{
				xQueueReset(msg2remote_queue);
				ESP_LOGI(TAG, "TCP client exit");
				vTaskDelete(NULL);
				return;
				}
			sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			if (sockfd < 0)
				{
				ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
				esp_restart();
				}
			if (connect(sockfd, (struct sockaddr *)dest_addr_ip4, sizeof(struct sockaddr_in)) != 0)
				{
				ESP_LOGI(TAG, "cannot connect %d", errno);
				continue;
				}
			written = send(sockfd, &qmsg,  sizeof(socket_message_t), 0);
			if(written == -1)
				ESP_LOGI(TAG, "cannot send message %d", errno);
			else
				ESP_LOGI(TAG, "client message sent cmd_id = %lu", qmsg.cmd_id) ;
			close(sockfd);
			}
		}
	}

void tcp_server()
	{
	char addr_str[128];
    int ip_protocol = 0;
    int keepAlive = 0;
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
		int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err == 0)
			{
			ESP_LOGI(TAG, "Socket bound, port %d", DEFAULT_TCP_SERVER_PORT);
			err = listen(listen_sock, 1);
			if (err == 0)
				{
				while (1) // accept loop
					{
					ESP_LOGI(TAG, "Socket listening");
					struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
					socklen_t addr_len = sizeof(source_addr);
					server_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
					if(server_sock >= 0)
						{
						ESP_LOGI(TAG, "server task socket %d", server_sock);
						// Set tcp keepalive option
						int no_delay = 1;
						setsockopt(server_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
						setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));
						struct timeval tv;
						tv.tv_sec = 3;
						tv.tv_usec = 0;
						setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
						getsockname(server_sock, (struct sockaddr *)&localbind, &len);
						inet_ntoa_r(localbind.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
						ESP_LOGI(TAG, "local bind %s:%d", addr_str, ntohs(localbind.sin_port));
						// Convert ip address to string
						if (source_addr.ss_family == PF_INET)
							{
							addr_str[0] = 0;
							inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
							ESP_LOGI(TAG,
								"Socket accepted ip address: %s:%d", addr_str, ntohs(((struct sockaddr_in *)&source_addr)->sin_port)) ;

							message.ts = 1;
							message.cmd_id = CONACK;
							written = send(server_sock, &message,  sizeof(socket_message_t), 0);
							if (written < 0)
								{
								ESP_LOGE(TAG, "Error occurred when sending ok: errno %d", errno);
								close(server_sock);
								}
							else
								{
								ESP_LOGI(TAG, "OK sent, written bytes %d", written);
								//waiting for init message
								len = recv(server_sock, &message, sizeof(socket_message_t), MSG_WAITALL);
								if(len < 0)
									{
									ESP_LOGI(TAG, "no init message received within %llu secs", tv.tv_sec);
									close(server_sock);
									}
								else
									{
									if(message.cmd_id == COMMINIT)
										{
										if(comm_client_handle)
											{
											socket_message_t m;
											m.cmd_id = TERMINATESELF;
											xQueueSend(msg2remote_queue, &m, portMAX_DELAY);
											}
										xTaskCreate(comm_client, "TCP client", 8192, &message, 5, &comm_client_handle);
										if(!comm_client_handle)
											{
											ESP_LOGE(TAG, "Unable to start communication client task");
											esp_restart();
											}
										commTS = message.ts;

										tv.tv_sec = 0;
										tv.tv_usec = 0;
										setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

										ESP_LOGI(TAG, "Comm OK. Starting communication loop");
										//clear bat level
										//bat_level = 200;
										while(1) //communication loop
											{
											len = recv(server_sock, &message, sizeof(socket_message_t), MSG_WAITALL);
											if (len < 0)
												{
												ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
												close(server_sock);
												break;
												}
											else if (len == 0)
												{
												close(server_sock);
												break;
												}
											else
												{
													/*
												//ESP_LOGI(TAG, "msg received len: %d / ts: %llu / msg ID: %lu / params[0]: %d", len, message.ts, message.cmd_id, (int8_t)message.params[0]);
												switch(message.cmd_id)
													{
#if STEERING_TYPE == ST_STEPPER
													case STCALIBRATE:
														st_calibrate();
														message.cmd_id = STCALRESPONSE;
														message.ts = esp_timer_get_time();
														message.params[0] = steering_ok;
														if(steering_ok)
															{
															message.params[1] = steps_max;
															message.params[2] = steps_mid;
															}
														message.params[4] = bat_level;
														written = send(server_sock, &message,  sizeof(socket_message_t), 0);
														if (written < 0)
															{
															ESP_LOGE(TAG, "Error occurred when cal response: errno %d", errno);
															close(server_sock);
															break;
															}
														//ESP_LOGI(TAG, "msg sent ts: %llu / msg ID: %lu", message.ts, message.cmd_id);
														break;
	#endif
													case BATLEVEL:
														int hall_mv, bat_mv;
														get_adc_values(&hall_mv, &bat_mv);
														ESP_LOGI(TAG, "hall_mv = %d, bat_mv = %d, bat level = %d", hall_mv, bat_mv, bat_level);
														message.cmd_id = BATLEVELRESPONSE;
														message.ts = esp_timer_get_time();
														message.params[0] = bat_level;
														written = send(server_sock, &message,  sizeof(socket_message_t), 0);
														if (written < 0)
															{
															ESP_LOGE(TAG, "Error occurred when bat level response: errno %d", errno);
															close(server_sock);
															break;
															}
														//ESP_LOGI(TAG, "msg sent ts: %llu / msg ID: %lu / bl: %d", message.ts, message.cmd_id, message.params[0]);
														break;
													case STSTEP:
														ret = pdTRUE;
														if(steering_ok)
															{
															if(abort_st == 0)
																abort_st = 1;
															ret = xQueueSend(steering_cmd_queue, &message, portMAX_DELAY);
															}
														message.cmd_id = STSTEPRESPONSE;
														message.ts = esp_timer_get_time();
														message.params[0] = ret;
														message.params[1] = steering_ok;
														written = send(server_sock, &message,  sizeof(socket_message_t), 0);
														if (written < 0)
															{
															ESP_LOGE(TAG, "Error occurred when ststep response: errno %d", errno);
															close(server_sock);
															break;
															}
														//ESP_LOGI(TAG, "msg sent ts: %llu / msg ID: %lu", message.ts, message.cmd_id);
														break;
													case PTDC:
														ret = pdTRUE;
														if(steering_ok)
															{
															if(abort_pt == 0)
																abort_pt = 1;
															ret = xQueueSend(pt_cmd_queue, &message, portMAX_DELAY);
															}
														message.cmd_id = PTDCRESPONSE;
														message.ts = esp_timer_get_time();
														message.params[0] = ret;
														message.params[1] = steering_ok;
														written = send(server_sock, &message,  sizeof(socket_message_t), 0);
														if (written < 0)
															{
															ESP_LOGE(TAG, "Error occurred when ptdc response: errno %d", errno);
															close(server_sock);
															break;
															}
														//ESP_LOGI(TAG, "msg sent ts: %llu / msg ID: %lu", message.ts, message.cmd_id);
														break;
													default:
														ESP_LOGI(TAG, "unknown message ID: %lu", message.cmd_id);
													}
													*/
												}
											}
										}
									}
								}
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
	tcp_server_handle = NULL;
	vTaskDelete(NULL);
	}
/*
static void tcp_server_task(void *pvParameters)
	{
	esp_netif_ip_info_t ip_info;
	struct sockaddr_storage dest_addr;
	server_task_args_t *sta;
    sta = (server_task_args_t *)pvParameters;
	int err = esp_netif_get_ip_info(esp_netif_ap, &ip_info);
	if(err != ESP_OK)
		{
		ESP_LOGE(TAG, "No IP found for AP interface");
		vTaskDelete(NULL);
	    return;
		}
	struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
	dest_addr_ip4->sin_addr.s_addr = htonl(ip_info.ip.addr);
	dest_addr_ip4->sin_family = AF_INET;
	dest_addr_ip4->sin_port = htons(sta->port);
	listen_sock = socket(sta->addr_family, SOCK_STREAM, 0);
    if (listen_sock < 0)
    	{
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    	}
    listen_port = sta->port;
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ESP_LOGI(TAG, "Socket created");
	}

static void monitor_task(void *pvParameters)
	{
		
	}
int tcp_server()
	{
	if(!isConnected())
		{
		ESP_LOGE(TAG, "No network connection. Server cannot start\nConnect first.\n");
		esp_restart();
		}
	server_task_args_t sta;
	sta.port = DEFAULT_TCP_SERVER_PORT;
	sta.addr_family = AF_INET;
	xTaskCreate(tcp_server_task, "tcp_server", 4096, &sta, USER_TASK_PRIORITY, &server_task_handle);
	if(server_task_handle)
		{
		xTaskCreate(monitor_task, "monitor task", 4096, NULL, 5, &mon_task_handle);
		if(mon_task_handle)
			return 0;
		else
 			ESP_LOGE(TAG, "Cannot start monitor_task");
		}
	else 
		ESP_LOGE(TAG, "Cannot start tcp_server_task");

	return 1;
	}
*/
