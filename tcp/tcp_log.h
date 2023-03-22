/*
 * tcp_log.h
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */

#ifndef COMMON_TCP_TCP_LOG_H_
#define COMMON_TCP_TCP_LOG_H_

#define LOG_SERVER			"proxy.gnet"
#define LOG_PORT			8081

int tcp_log_init(void);
int tcp_log_message(char *message);

#endif /* COMMON_TCP_TCP_LOG_H_ */
