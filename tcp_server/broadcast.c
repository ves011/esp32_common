/*
 * broadcast.c
 *
 *  Created on: Nov 23, 2021
 *      Author: viorel_serbu
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
//#include "nvs_flash.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "esp_netif_ip_addr.h"
#include "driver/timer.h"
#include "cmd_wifi.h"
#include "tcp_server.h"
#include "broadcast.h"


int listen_bc_sock;
int bc_port;

#define BCTAG "broadcast"


void bcast_listener_task(void *pvParameters)
	{
	int port = *(int *)pvParameters;
	int n, rcvb;
	message_t message;
	struct sockaddr_storage dest_addr;
	struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
	struct sockaddr_in serv_addr, cli_addr;
	dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr_ip4->sin_family = AF_INET;
	dest_addr_ip4->sin_port = htons(port);
	listen_bc_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(listen_bc_sock < 0)
		{
		ESP_LOGI(BCTAG, "Error creating socket %d", errno);
		}
	else
		{
		bzero((char *)&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(port);
		if(bind(listen_bc_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			{
			ESP_LOGI(BCTAG, "Error on bind %d", errno);
			}
		else
			{
			ESP_LOGI(BCTAG, "broadcast listener on port %d",port);
			while(1)
				{
				n= sizeof(cli_addr);
				rcvb = recvfrom(listen_bc_sock, &message, sizeof(message_t), 0, (struct sockaddr *)&cli_addr,(unsigned *)&n);
				if(rcvb > 0)
					{
					esp_netif_ip_info_t ip;
					ip.ip.addr = cli_addr.sin_addr.s_addr;
					ESP_LOGI(BCTAG,"brodcast message received from " IPSTR ":%d", IP2STR(&ip.ip), ntohs(cli_addr.sin_port));
					/*for (n = 0; n < rcvb; n++)
						{
						sprintf(buf + n, "%x", message.raw_message[n]);
						}
					buf[n] = 0;*/
					ESP_LOGI(BCTAG, "message ID %d", message.header.messageID);
					if (message.header.messageID == IDENT_REQUEST)
						{
						message_t *msgevt = calloc(sizeof(message_t), 1);
						if (!msgevt)
							ESP_LOGI(BCTAG, "Cannot alocate memory for message");
						else
							{
							msgevt->header.messageID = message.header.messageID;
							msgevt->header.sender_ip = message.header.sender_ip;
							msgevt->header.sender_port = message.header.sender_port;
							msgevt->header.token = message.header.token;
							ESP_LOGI(BCTAG, "event ID: %d IP: " IPSTR ":%d", msgevt->header.messageID, IP2STR(&msgevt->header.sender_ip), msgevt->header.sender_port);
							evt_message_list_t *tmp, *tmpq = message_q;
							tmp = calloc(sizeof(evt_message_list_t), 1);
							tmp->next = NULL;
							tmp->message = msgevt;
							if (tmpq == NULL)
								message_q = tmp;
							else
								{
								while (tmpq->next)
									tmpq = tmpq->next;
								tmpq->next = tmp;
								tmp->message = msgevt;
								}
							if (mon_task_handle)
								xTaskNotify(mon_task_handle, NEW_MESSAGE_IN_QUEUE, eSetBits);
							}
						}
					}
				else
					{
					ESP_LOGI(BCTAG, "Connection closed");
					break;
					}
				}
			}
		}
	bcl_task_handle = NULL;
	vTaskDelete(NULL);
	}
int send_bcast_message(int port, char *message)
	{
	esp_err_t err;
	mode_t mode;
	esp_netif_t *netif;
	esp_netif_ip_info_t ip_info;
	wifi_ap_record_t ap_info;
	struct sockaddr_in dest_addr;
	err = esp_wifi_get_mode(&mode);
	int send_sock, n;
	if(err == ESP_ERR_WIFI_NOT_INIT)
		{
		ESP_LOGI(BCTAG, "No connection. WiFi not initialized");
		return 0;
		}
	if(mode == WIFI_MODE_STA)
		{
		err = esp_wifi_sta_get_ap_info(&ap_info);
		if(err != ESP_OK)
			{
			ESP_LOGI(BCTAG, "%s", esp_err_to_name(err));
			return 0;
			}
		netif = NULL;
		do
			{
			netif = esp_netif_next(netif);
			if(!netif)
				break;
			err = esp_netif_get_ip_info(netif, &ip_info);
			if(err == ESP_OK)
				{
				if(strcmp(esp_netif_get_desc(netif), "sta") == 0)
					break;
/*
				ESP_LOGI(WIFITAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(netif),
								IP2STR(&ip_info.ip),
								IP2STR(&ip_info.netmask),
								IP2STR(&ip_info.gw));
*/
				}

			} while (netif);
		if(netif == NULL)
			{
			ESP_LOGI(BCTAG, "No \"STA\" adapter found");
			return 1;
			}
		// create bcast address on the connected network
		ip_info.ip.addr = ip_info.ip.addr | (~ip_info.netmask.addr);
		dest_addr.sin_addr.s_addr = ip_info.ip.addr;
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(port);
		if((send_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			{
			ESP_LOGE(BCTAG, "Cannot create socket %d - %s", errno, esp_err_to_name(err));
			return 1;
			}
		n = 1;
		if(setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &n, sizeof(n)) == -1)
			{
			ESP_LOGE(BCTAG, "Error setting BROADCAST option %d - %s", errno, esp_err_to_name(err));
			return 1;
			}
		memset(dest_addr.sin_zero, '\0', sizeof(dest_addr.sin_zero));
		n = sendto(send_sock, message, strlen(message), 0, (const struct sockaddr *)(&dest_addr), sizeof(dest_addr));
		if(n < 0)
			{
			ESP_LOGE(BCTAG, "Error sending message %d - %s", errno, esp_err_to_name(err));
			return 1;
			}
		close(send_sock);
		}
	else
		{
		ESP_LOGI(BCTAG, "device in AP mode");
		return 1;
		}
	return 0;
	}

int bcast(int argc, char **argv)
	{
	int port;
	bcast_args.port->ival[0] = 0;
	bcast_args.message->count = 0;
	int nerrors = arg_parse(argc, argv, (void **) &bcast_args);
	if (nerrors != 0)
		{
	    arg_print_errors(stderr, bcast_args.end, argv[0]);
		return 1;
		}
	if(bcast_args.port->ival[0] > 0)
		port = bcast_args.port->ival[0];
	else
		port = CTRL_DEVICE_BCAST_PORT;
	if(!strcmp(bcast_args.op->sval[0], "send"))
		{
		if(bcast_args.message->count == 0)
			{
			printf("Nothing to send!\n");
			return 0;
			}
		send_bcast_message(port, (char *)bcast_args.message->sval[0]);
		}
	else if(!strcmp(bcast_args.op->sval[0], "listen"))
		{
		if(!isConnected())
			{
			printf("No network connection. BC Listener cannot start\nConnect first.\n");
			return 0;
			}
		if(bcl_task_handle)
			{
			printf("broadcast listener already running. Port: %d\n", bc_port);
			return 0;
			}
		bc_port = port;
		xTaskCreate(bcast_listener_task, "bcast_listener", 4096, &bc_port, USER_TASK_PRIORITY, &bcl_task_handle);
		//start broadcast listener on specified port
		}
	else if(!strcmp(bcast_args.op->sval[0], "stop"))
		{
		//stop broadcast listener
		if(!bcl_task_handle)
			printf("broadcast listener not running\n");
		else
			{
			if(listen_bc_sock > 0)
				{
				close(listen_bc_sock);
				listen_bc_sock = 0;
				}
			}
		}
	else if(!strcmp(bcast_args.op->sval[0], "status"))
		{
		if(bcl_task_handle)
			printf("broadcast listener running. Port: %d\n", bc_port);
		else
			printf("broadcast listener not running\n");
		}
	else
		{
		printf("Unknown option\n");
		return 1;
		}

	return 0;
	}

void register_bcast(void)
	{
	bcast_args.port = arg_int0("p", "port", "<port>", "Port to send/listen");
	bcast_args.op = arg_str1(NULL, NULL, "<action>", "send|listen|status");
	bcast_args.message = arg_str0("m", "message", "message", "message to send");
	bcast_args.end = arg_end(2);

	bcl_task_handle = NULL;
	listen_bc_sock = 0;
	const esp_console_cmd_t bcast_cmd =
	    {
	    .command = "bcast",
        .help = "bcast send|listen|stop|status [port] [message] ",
        .hint = NULL,
        .func = &bcast,
        .argtable = &bcast_args
    	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&bcast_cmd) );
	}

