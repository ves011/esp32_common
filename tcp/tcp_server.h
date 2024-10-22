/*
 * tcp_server.h
 *
 *  Created on: Oct 20, 2024
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_TCP_TCP_SERVER_H_
#define ESP32_COMMON_TCP_TCP_SERVER_H_


#define DEFAULT_TCP_SERVER_PORT     10001
#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

#define CONACK				0x10A5
#define COMMINIT			0x1001
#define STCALIBRATE			0x1002
#define STCALRESPONSE		0x2002
#define STSTEP				0x1003
#define STSTEPRESPONSE		0x2003
#define PTDC				0x1004
#define PTDCRESPONSE		0x2004

#define PTSTOP				0x1005
#define BATLEVEL			0x1006
#define BATLEVELRESPONSE	0x2006
#define NETLEVEL			0x1007
#define NETLEVELRESPOSNSE	0x2007

#define TERMINATESELF		0x10a6 // message to client comm task to terminate self

extern TaskHandle_t tcp_server_handle;

typedef struct
	{
	uint64_t ts;
	uint32_t cmd_id;
	uint8_t params[20];
	} socket_message_t;

typedef struct
	{
	int addr_family;		/*!< AF_INET or AF_INET6 */
	int port;				/*!< port to accept incoming connections */
	} server_task_args_t;

void tcp_server();

#endif /* ESP32_COMMON_TCP_TCP_SERVER_H_ */
