/*
 * tcp_log.h
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */

#ifndef COMMON_TCP_TCP_LOG_H_
#define COMMON_TCP_TCP_LOG_H_

#define TRANSPORT_TCP	1
#define TRANSPORT_MQTT	2

#define LOG_SERVER			"proxy.gnet"
#define LOG_PORT			8081

#define LOG_SERVER_DEV		"proxy.gnet"
#define LOG_PORT_DEV		8084

int tcp_log_init(int level);
void tcp_log_message(const char *message);
int bt_log_message(char *message);

#endif /* COMMON_TCP_TCP_LOG_H_ */
