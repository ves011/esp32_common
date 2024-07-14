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
void publish_state(char *msg, int qos, int retain);
void publish_monitor(char *msg, int qos, int retain);
void publish_state_a(char *msg, int qos, int retain);
void publish_monitor_a(char *msg, int qos, int retain);
void publish_MQTT_client_status();
void subscribe(char *topic);
void publish_reqID();
void publish_topic(char * topic, char * msg, int qos, int retain);
void publish_MQTT_client_log(char *message);
#endif /* TCP_MQTT_CTRL_H_ */
