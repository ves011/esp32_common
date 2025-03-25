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

typedef struct
	{
	int socket;
	} tParams_t;
	
void register_tcp_server();

#endif /* MAIN_TCP_SERVER_H_ */
