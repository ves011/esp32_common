
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
#include "driver/timer.h"
#include "cmd_wifi.h"
#include "tcp_server.h"

#define TIMER_DIVIDER         		(16)  //  Hardware timer clock divider
#define TIMER_SCALE           		(TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

#define TIMER_GROUP					TIMER_GROUP_0
#define TIMER_INDEX					TIMER_0

esp_netif_ip_info_t local_ip_info;
uint32_t valid_token;

/*
 * further events to be defined here
 */

static const char *TAG = "TCP server";
int client_in;
static const char *close_connection_msg = "\\1";

TaskHandle_t mon_task_handle, cmd_task_handle, server_task_handle, bcl_task_handle;

static int socket_inuse;
static int listen_sock;
static int listen_port;

evt_message_list_t *message_q;

bool IRAM_ATTR client_timer_callback(void *args)
	{
    BaseType_t high_task_awoken = pdFALSE;

    /* Now just send the signal to the main program task */
    if(mon_task_handle)
    	xTaskNotifyFromISR(mon_task_handle, TIMEOUT_EVENT, eSetBits, &high_task_awoken);
    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
	}

void cmd_task(void *pvParameters)
	{
    int len;
    char rx_buffer[128];
	message_t message, messageW;
    socket_inuse = (int)pvParameters;
    ESP_LOGI(TAG, "cmd_task socket %d", socket_inuse);

    //resume monitor task
    if(mon_task_handle)
    	{
    	ESP_LOGI(TAG, "cmd_task - resume mon task");
    	xTaskNotify(mon_task_handle, CLIENT_IN_EVENT, eSetBits);
    	do
			{
			len = recv(socket_inuse, &message, sizeof(message_t), 0);
			if (len < 0)
				{
				ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
				break;
				}
			else if (len == 0)
				{
				ESP_LOGW(TAG, "Connection closed");
				if(mon_task_handle)
					xTaskNotify(mon_task_handle, CLIENT_OUT_EVENT, eSetBits);
				}
			else
				{
				inet_ntoa_r(message.header.sender_ip, rx_buffer, sizeof(rx_buffer) - 1);
				ESP_LOGI(TAG, "Received %d bytes / message id: %d / peer: %s", len, message.header.messageID, rx_buffer);
				if (message.header.messageID == ADC_VALUE_REQUEST)
					{
					messageW.header.messageID = ADC_VALUE_RESPONSE;
					messageW.header.sender_ip = local_ip_info.ip;
					messageW.header.sender_port = DEFAULT_TCP_SERVER_PORT;
					messageW.header.token = valid_token;
					messageW.messageVal.sval[0] = 0xaa55;
					len = send(socket_inuse, &messageW, sizeof(message_t), 0);
					}
				}
			} while (len > 0);
    	}
    if(socket_inuse > 0)
    	close(socket_inuse);
    socket_inuse = 0;
    cmd_task_handle = NULL;
    vTaskDelete(NULL);
	}

void tcp_server_task(void *pvParameters)
	{
    char addr_str[128];
	esp_netif_t *netif;
	esp_netif_ip_info_t ip_info;

    server_task_args_t *sta;
    sta = (server_task_args_t *)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    int i;
    struct sockaddr_storage dest_addr;
    if (sta->addr_family == AF_INET)
    	{
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(sta->port);
        ip_protocol = IPPROTO_IP;
    	}

    listen_sock = socket(sta->addr_family, SOCK_STREAM, ip_protocol);
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

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    	{
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", sta->addr_family);
        goto CLEAN_UP;
    	}
    ESP_LOGI(TAG, "Socket bound, port %d", sta->port);

    err = listen(listen_sock, 1);
    if (err != 0)
    	{
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    	}

    while (1)
    	{
        ESP_LOGI(TAG, "Socket listening");
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        	{
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        	}
        ESP_LOGI(TAG, "server task socket %d", sock);
        // Set tcp keepalive option
        int no_delay = 1;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET)
        	{
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        	}

    	ESP_LOGI(TAG,
	    	"Socket accepted ip address: %s:%d", addr_str, ntohs(((struct sockaddr_in *)&source_addr)->sin_port)) ;
        if(client_in == 0)
        	{
        	netif = NULL;
        	do
        		{
        		netif = esp_netif_next(netif);
        		if (!netif)
	        		break;
        		err = esp_netif_get_ip_info(netif, &ip_info);
        		} while (netif);
			if (!netif)
				{
				
				}
        	xTaskCreate(cmd_task, "command task", 4096, (void*)sock, 5, &cmd_task_handle);
        	//wait for client_in = 1 which is set by cmd task
        	for(i = 0; i < 10; i++)
        		{
        		if(client_in == 1)
        			break;
        		usleep(10000);
        		}
        	/*
        	 * to handle error case when  i == 10
        	 */
        	}
        else
        	{
        	int written;
        	int len = strlen(close_connection_msg);
        	int to_write = len;
        	while (to_write > 0)
        		{
        	    written = send(sock, close_connection_msg + (len - to_write), to_write, 0);
        	    if (written < 0)
        	    	{
        	        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        	        break;
        	        }
        	    to_write -= written;
        	    }
        	ESP_LOGI(TAG, "written bytes %d", written);
        	close(sock);
        	}
        //do_retransmit(sock);
        //shutdown(sock, 0);
        //close(sock);
    	}

CLEAN_UP:
	if(listen_sock > 0)
		close(listen_sock);
	listen_sock = 0;
	listen_port = 0;
    /*
    if(mon_task_handle)
    	{
    	xTaskNotify(mon_task_handle, STOP_TCP_SERVER, eSetBits);
    	}
    if(cmd_task_handle)
    	{
    	vTaskDelete(cmd_task_handle);
    	cmd_task_handle = NULL;
    	}
    */
    server_task_handle = NULL;
    vTaskDelete(NULL);
	}

void monitor_task(void *pvParameters)
	{
	uint32_t notificationVal, err;
	timer_config_t config = {	.divider = TIMER_DIVIDER,
	        					.counter_dir = TIMER_COUNT_UP,
								.counter_en = TIMER_PAUSE,
								.alarm_en = TIMER_ALARM_EN,
								.auto_reload = true,}; // default clock source is APB
	timer_init(TIMER_GROUP, TIMER_INDEX, &config);
	timer_set_counter_value(TIMER_GROUP, TIMER_INDEX, 0);
	timer_set_alarm_value(TIMER_GROUP, TIMER_INDEX, CLIENT_CONNECTION_WINDOW * TIMER_SCALE);
	client_timer_info_t *client_timer_info = calloc(1, sizeof(client_timer_info_t));
	client_timer_info->timer_group = TIMER_GROUP;
	client_timer_info->timer_idx = TIMER_INDEX;
	client_timer_info->auto_reload = false;
	client_timer_info->alarm_interval = CLIENT_CONNECTION_WINDOW;
	timer_enable_intr(TIMER_GROUP, TIMER_INDEX);
	timer_isr_callback_add(TIMER_GROUP, TIMER_INDEX, client_timer_callback, client_timer_info, 0);
	while(1)
		{
		ESP_LOGI(TAG, "mon task waiting for notifications");
		xTaskNotifyWait(0, ULONG_MAX, &notificationVal, portMAX_DELAY);
		ESP_LOGI(TAG, "mon task notification %x", notificationVal);
		if(notificationVal & CLIENT_IN_EVENT)
			{
			timer_set_counter_value(TIMER_GROUP, TIMER_INDEX, 0);
			err = timer_start(TIMER_GROUP, TIMER_INDEX);
			ESP_LOGI(TAG, "CLIENT_IN_EVENT - timer start %x", err);
			client_in = 1;
			}
		if(notificationVal & TIMEOUT_EVENT)
			{
			err = timer_pause(TIMER_GROUP, TIMER_INDEX);
			close(socket_inuse);
			ESP_LOGI(TAG, "TIMEOUT_EVENT - socket close");
			client_in = 0;
			}
		if(notificationVal & CLIENT_OUT_EVENT)
			{
			err = timer_pause(TIMER_GROUP, TIMER_INDEX);
			//close(socket_inuse);
			ESP_LOGI(TAG, "CLIENT_OUT_EVENT");
			client_in = 0;
			}
		if(notificationVal & STOP_TCP_SERVER)
			{
			err = timer_pause(TIMER_GROUP, TIMER_INDEX);
			timer_disable_intr(TIMER_GROUP, TIMER_INDEX);
			timer_isr_callback_remove(TIMER_GROUP, TIMER_INDEX);
			if(client_timer_info)
				free(client_timer_info);
			if(socket_inuse > 0)
				close(socket_inuse);
			if(listen_sock > 0)
				close(listen_sock);
			socket_inuse = listen_sock = 0;
			ESP_LOGI(TAG, "STOP_TCP_SERVER event");
			client_in = 0;
			break;
			}
		if (notificationVal & NEW_MESSAGE_IN_QUEUE)
			{
			evt_message_list_t *tmp, *tmpq = message_q;
			while (tmpq)
				{
				ESP_LOGI(TAG, "event ID: %d IP: " IPSTR ":%d", 
								tmpq->message->header.messageID, 
								IP2STR(&tmpq->message->header.sender_ip), tmpq->message->header.sender_port);
				if (tmpq->message->header.messageID == IDENT_REQUEST)
					{
					esp_netif_t *netif;
					message_t message;
					struct sockaddr_in srv_addr;
					int sendsock;
					netif = NULL;
					do
						{
						netif = esp_netif_next(netif);
						if (!netif)
							break;
						err = esp_netif_get_ip_info(netif, &local_ip_info);
						if (err == ESP_OK)
							{
							message.header.messageID = IDENT_ACK;
							message.header.sender_ip = local_ip_info.ip;
							message.header.sender_port = DEFAULT_TCP_SERVER_PORT;
							message.header.token = tmpq->message->header.token;
							valid_token = tmpq->message->header.token;
							sendsock = socket(AF_INET, SOCK_STREAM, 0);
							if (sendsock < 0)
								{
								ESP_LOGI(TAG, "Error creating socket");
								break;
								}
							bzero((char *) &srv_addr, sizeof(srv_addr));
							srv_addr.sin_port = htons(tmpq->message->header.sender_port);
							srv_addr.sin_family = AF_INET;
							srv_addr.sin_addr.s_addr = tmpq->message->header.sender_ip.addr;
							if (connect(sendsock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
								{
								ESP_LOGI(TAG, "Error connect");
								break;
								}
							int written = send(sendsock, &message, sizeof(message), 0);
							if (written < 0)
								{
								ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
								}
							if (sendsock > 0)
								close(sendsock);
							}
						} while (netif);
					}
				//delete processed message
				tmp = tmpq->next;
				free(tmpq->message);
				free(tmpq);
				tmpq = tmp;
				}
			message_q = tmpq;
			}
		}
	mon_task_handle = NULL;
	vTaskDelete(NULL);
	}

int tcp_server(int argc, char **argv)
	{
	tcp_server_args.port->ival[0] = 0;
	int nerrors = arg_parse(argc, argv, (void **) &tcp_server_args);
	if (nerrors != 0)
		{
	    arg_print_errors(stderr, tcp_server_args.end, argv[0]);
	    return 1;
	    }
	if(!strcmp(tcp_server_args.op->sval[0], "start"))
		{
		if(!isConnected())
			{
			printf("No network connection. Server cannot start\nConnect first.\n");
			return 0;
			}
		if(tcp_server_args.port->ival[0] == 0)
			{
			printf("missing port value required by <start>\ntcp_server /? for list of parameters\n");
			return 0;
			}
		if(server_task_handle == NULL && mon_task_handle == NULL)
			{
			server_task_args_t sta;
			sta.port = tcp_server_args.port->ival[0];
			sta.addr_family = AF_INET;
			xTaskCreate(tcp_server_task, "tcp_server", 4096, &sta, USER_TASK_PRIORITY, &server_task_handle);
			xTaskCreate(monitor_task, "monitor task", 4096, NULL, 5, &mon_task_handle);
			}
		else
			{
			printf("tcp_server already running\nListening on port %d", listen_port);
			return 0;
			}
		//xTaskCreate(dumb_task, "dumb task", 4096, NULL, 5, &dumb_task_handle);
		}
	else if(!strcmp(tcp_server_args.op->sval[0], "stop"))
		{
		xTaskNotify(mon_task_handle, STOP_TCP_SERVER, eSetBits);
		//vTaskDelete(mon_task_handle);
		//close(listen_sock);

		}
	else if(!strcmp(tcp_server_args.op->sval[0], "status"))
		{
		if(server_task_handle == NULL && mon_task_handle == NULL)
			printf("tcp_server not running\n");
		else
			printf("tcp_server running, listening on port %d\n", listen_port);
		return 0;
		}
	else if(!strcmp(tcp_server_args.op->sval[0], "/?"))
		{
		printf("tcp_server <start|stop|status> [port]\n");
		return 0;
		}
	else
		{
		printf("tcp_server: [%s] unknown option\n", tcp_server_args.op->sval[0]);
		}
	return 0;
	}

void register_tcp_server(void)
	{
	tcp_server_args.port = arg_int0(NULL, NULL, "<xxxx>", "Port to listen");
	tcp_server_args.op = arg_str1(NULL, NULL, "<action>", "start|stop|status");
	tcp_server_args.end = arg_end(2);

	mon_task_handle = cmd_task_handle = server_task_handle = NULL;
	socket_inuse = listen_sock = 0;

	const esp_console_cmd_t tcp_server_cmd =
	    {
	    .command = "tcp_server",
        .help = "TCP server start|stop|status [port]",
        .hint = NULL,
        .func = &tcp_server,
        .argtable = &tcp_server_args
    	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&tcp_server_cmd) );
	}

