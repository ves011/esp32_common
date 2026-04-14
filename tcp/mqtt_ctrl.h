/*
 * mqtt_ctrl.h
 *
 *  Created on: Mar 5, 2023
 *      Author: viorel_serbu
 */

#ifndef TCP_MQTT_CTRL_H_
#define TCP_MQTT_CTRL_H_

extern char *TOPIC_STATE, *TOPIC_ERROR, *TOPIC_MONITOR, *TOPIC_CTRL, *TOPIC_LOG, *TOPIC_KA; //TOPIC_CMD[32]*
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	extern char *TOPIC_STATE_A, *TOPIC_MONITOR_A;
	extern QueueHandle_t water_cmd_q;
#endif


typedef struct 
	{
    char *topic;
    char *payload;
	} mqtt_rx_msg_t;

int mqtt_start(void);
void register_mqtt(void);

void publish_MQTT_client_status();
void subscribe(char *topic);
void publish_reqID();
void publish_topic(char * topic, char * msg, int qos, int retain);
void publish_MQTT_client_log(char *message);
int get_MQTT_connection_state(char *id);
#endif /* TCP_MQTT_CTRL_H_ */
