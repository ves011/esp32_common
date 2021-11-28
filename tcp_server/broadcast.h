/*
 * broadcast.h
 *
 *  Created on: Nov 23, 2021
 *      Author: viorel_serbu
 */

#ifndef COMPONENTS_TCP_SERVER_BROADCAST_H_
#define COMPONENTS_TCP_SERVER_BROADCAST_H_

/** @brief default UDP broadcast port */
#define CTRL_DEVICE_BCAST_PORT		9001
#define USER_DEVICE_BCAST_PORT		9000


extern int listen_bc_sock;
extern int bc_port;

void register_bcast(void);
void bcast_listener_task(void *pvParameters);
struct
	{
    struct arg_str *op;
	struct arg_int *port;
	struct arg_str *message;
    struct arg_end *end;
	} bcast_args;

#endif /* COMPONENTS_TCP_SERVER_BROADCAST_H_ */
