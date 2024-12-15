/*
 * tcp_client.c
 *
 *  Created on: Dec 5, 2024
 *      Author: viorel_serbu
 */

#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cmd_types.h"
#include "project_specific.h"
#include "common_defines.h"
#include "external_defs.h"
#include "tcp_server.h"
#include "tcp_client.h"

static const char *TAG = "tcp_client";	
extern QueueHandle_t msg_queue;
QueueHandle_t tcp_receive_queue = NULL, tcp_send_queue = NULL;
TaskHandle_t process_message_task_handle = NULL, send_task_handle = NULL;
int commstate;
static void process_message(socket_message_t msg);

static void process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	int sock = ((tParams_t *)pvParameters)->socket;
	while(1)
		{
		if(xQueueReceive(tcp_receive_queue, &msg, portMAX_DELAY))
			{
			//process_message(msg);
			if(msg.cmd_id == CLOSE_CONNECTION)
				{
				ESP_LOGI(TAG, "Closing connection");
				shutdown(sock, 0);
				close(sock);
				}
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

static void tcp_client_task(void *pvParameters)
	{
	struct addrinfo hints;
    struct addrinfo *result;
    //struct sockaddr_storage  peer_addr;
    //char host_ip[] = HOST_IP_ADDR;
    char host_ip[32];
    int addr_family = 0;
    int ip_protocol = 0;
    int err, len;
    
    struct sockaddr_in dest_addr;
    
	commstate = IDLE;
    memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags |= AI_CANONNAME;
	err = getaddrinfo(HOST_IP_ADDR, NULL, &hints, &result);
	if(err == 0)
		{
		dest_addr.sin_addr = ((struct sockaddr_in *) result->ai_addr)->sin_addr;
		inet_ntoa_r(dest_addr.sin_addr.s_addr, host_ip, sizeof(host_ip) - 1);
		ESP_LOGI(TAG, "%s - %s", HOST_IP_ADDR, host_ip);
		}
		
    //inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    socket_message_t msg;
	
    while (1) 
    	{
        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) 
        	{
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        	}
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);
        int no_delay = 1;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int));

		int option = 1;
		int keepalive_intvl = 1;
		int keepalive_count = 3;
		int keepalive_idle = 1;
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof (int) );
		setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(int));
		setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(int));
		setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof(int));
		
        err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) 
        	{
            ESP_LOGI(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            continue;
        	}
        ESP_LOGI(TAG, "Successfully connected");
        // send cominit
        msg.ts = TSINIT;
        msg.cmd_id = COMMINIT;
        len = 0;
        while(len < sizeof(socket_message_t))
			{
        	err = send(sock, (uint8_t *)(&msg + len), sizeof(socket_message_t) - len, 0);
			if (err < 0) 
            	{
                ESP_LOGE(TAG, "Error occurred during sending COMMINIT: errno %d", errno);
                break;
            	}
			else
				len += err;
			}
		if(err < 0)
			{
			close(sock);
			continue;
			}
        ESP_LOGI(TAG, "COMMINIT sent");
        //wait for COMMACK
		len = recv(sock, &msg, sizeof(socket_message_t), MSG_WAITALL);
		if (len < 0)
			{
			ESP_LOGE(TAG, "Error occurred during receiving COMMACK: errno %d", errno);
			shutdown(sock, 0);
			close(sock);
			continue;
			}
		else if (len == 0) //socket closed by client
			{
			ESP_LOGI(TAG, "socket closed by peer");
			shutdown(sock, 0);
			close(sock);
			continue;
			}
		if(msg.cmd_id != COMMACK)
			{
			ESP_LOGI(TAG, "Wrong comminit protocol");
			shutdown(sock, 0);
			close(sock);
			continue;
			}
		//start tcp send task
		tParams_t tParams;
		tParams.socket = sock;
		if(xTaskCreate(process_message_task, "process_message", 8192, &tParams, 4, &process_message_task_handle) == pdPASS)
			{
			if(xTaskCreate(send_task, "send_message_task", 8192, &tParams, 4, &send_task_handle) == pdPASS)
				{
				ESP_LOGI(TAG, "Communication OK, entering receiving loop");	
				commstate = CONNECTED;
				}
			else
				{
				ESP_LOGI(TAG, "Cannot create send_task");
				esp_restart();
				}
			}
		else 
			{
			ESP_LOGI(TAG, "Cannot process_message_task");
			esp_restart();
			}
		while(1)
			{
			//packet_count = 0;
			len = recv(sock, &msg, sizeof(socket_message_t), MSG_WAITALL);
			if (len < 0)
				{
				ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
				ESP_LOGI(TAG, "breaking from receive loop");
				break;
				}
			else if (len == 0) //socket closed by client
				{
				ESP_LOGI(TAG, "socket closed by peer");
				ESP_LOGI(TAG, "breaking from receive loop");
				break;
				}
			//posting the message, triggers process_message task
			xQueueSend(tcp_receive_queue, &msg, 0);
			}
		shutdown(sock, 0);
		close(sock);
		commstate = IDLE;
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
    vTaskDelete(NULL);
	}

static void process_message(socket_message_t msg)
	{
	switch(msg.cmd_id)
		{
		case NMEA_MESSAGE:
			printf("%s", msg.p.payload);
//			sprintf(buf, "%02x %02x %02x %02x %02x\n%02x %02x %02x %02x", 
//			msg.p.payload[0], msg.p.payload[1], msg.p.payload[2], msg.p.payload[3], msg.p.payload[4],
//			msg.p.payload[strlen(msg.p.payload) -4], msg.p.payload[strlen(msg.p.payload) - 3], 
//			msg.p.payload[strlen(msg.p.payload) -2], msg.p.payload[strlen(msg.p.payload) - 1]);
			//ESP_LOGI(TAG, "print count %d", count);
			break;
		default:
			break;
		}
	}
static struct {
    struct arg_str *op;
    struct arg_str *arg1;
    struct arg_str *arg2;
    struct arg_end *end;
} tcp_args;

static int do_tcp(int argc, char ** argv)
	{
	int port, host;
	socket_message_t msg;
	int nerrors = arg_parse(argc, argv, (void **)&tcp_args);
	
    if (nerrors != 0)
    	{
        arg_print_errors(stderr, tcp_args.end, argv[0]);
        return 1;
    	}
    if(!strcmp(tcp_args.op->sval[0], "disconnect"))
    	{
		if(tcp_receive_queue && commstate == CONNECTED)
    		{
			msg.ts = esp_timer_get_time();
			msg.cmd_id = CLOSE_CONNECTION;
			xQueueReset(tcp_receive_queue);
			xQueueSend(tcp_receive_queue, &msg, 0);
			}
		else
			ESP_LOGI(TAG, "Not connected");
		}
	if(!strcmp(tcp_args.op->sval[0], "connect"))
    	{
		if(tcp_args.arg1->count && tcp_args.arg2->count)
			{
			
			}
		else
			ESP_LOGI(TAG, "not provided all fields");

		}
    return 0;
	}
void register_tcp_client()
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
	ESP_LOGE(TAG, "Starting tcp client task");
	if(xTaskCreate(tcp_client_task, "tcp_client task", 8192, NULL, 4, NULL) != pdPASS)
		{
		ESP_LOGE(TAG, "Unable to start tcp client task");
		esp_restart();
		}
	tcp_args.op = arg_str1(NULL, NULL, "<op>", "connect | disconnect");
	tcp_args.arg1 = arg_str0(NULL, NULL, "host", "host to connect to");
	tcp_args.arg2 = arg_str0(NULL, NULL, "port", "port");
    tcp_args.end = arg_end(1);
    const esp_console_cmd_t tcp_cmd =
    	{
        .command = "tcp",
        .help = "tcp client commands",
        .hint = NULL,
        .func = &do_tcp,
        .argtable = &tcp_args
    	};
    ESP_ERROR_CHECK(esp_console_cmd_register(&tcp_cmd));
	}