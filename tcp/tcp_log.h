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

int tcp_log_init();
int tcp_log_message(char *message);
int bt_log_message(char *message);

#endif /* COMMON_TCP_TCP_LOG_H_ */
