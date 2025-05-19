/*
 * tcp_server.h
 *
 *  Created on: Dec 5, 2024
 *      Author: viorel_serbu
 */

#ifndef MAIN_TCP_SERVER_H_
#define MAIN_TCP_SERVER_H_

//#define DEFAULT_TCP_SERVER_PORT		10011

// communication states
#define IDLE					1	// tcp server waiting for connections
#define CONNECTED				2	// socket connection established

#define TSINIT					0xfa500ff5af00ULL

//command IDs
#define COMMINIT				1
#define COMMACK					2
#define COMMNACK				3
#define NMEA_MESSAGE			4
#define GPS_FIX					5

#define SEND_TO_SERIAL			4

//pump controller messages
#define GET_STATE				0x1001
#define PUMP_LIMITS				0x1002
#define P0_OFFSET				0x1003
#define PUMP_MON				0x1004
#define PUMP_CONF				0x1005
#define PUMP_P0OFF				0x1006
#define PUMP_SETONLINE			0x1007
#define PUMP_ERR				0x2001
#define GET_WIFICONF			0x3001
#define SET_WIFICONF			0x3002
#define SYS_CMD					0x4001
#define LOG_MESSAGE				0x5001



extern QueueHandle_t tcp_receive_queue, tcp_send_queue;

typedef struct
	{
	int socket;
	} tParams_t;
	
void register_tcp_server();

#endif /* MAIN_TCP_SERVER_H_ */
