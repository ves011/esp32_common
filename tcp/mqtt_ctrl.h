/*
 * mqtt_ctrl.h
 *
 *  Created on: Mar 5, 2023
 *      Author: viorel_serbu
 */

#ifndef TCP_MQTT_CTRL_H_
#define TCP_MQTT_CTRL_H_

int mqtt_start(void);
void register_mqtt(void);
void publish(char *topic, char *msg, int qos, int retain);
void publish_MQTT_client_status();
#endif /* TCP_MQTT_CTRL_H_ */
