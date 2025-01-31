#ifndef EXTERNAL_DEFS_H_
#define EXTERNAL_DEFS_H_

//#include "project_specific.h"

extern char TOPIC_STATE[32], TOPIC_MONITOR[32], TOPIC_CTRL[32], TOPIC_CMD[32], TOPIC_LOG[32];
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	extern char TOPIC_STATE_A[32], TOPIC_MONITOR_A[32];
	extern QueueHandle_t ui_cmd_q;
	extern QueueHandle_t water_cmd_q;
#endif

extern int tsync;//, client_connect_sync;
extern int controller_op_registered;
extern esp_netif_ip_info_t dev_ipinfo;
extern console_state_t console_state;
extern int restart_in_progress;
extern char USER_MQTT[32];
extern QueueHandle_t tcp_log_evt_queue, dev_mon_queue;
extern i2c_master_bus_handle_t i2c_bus_handle_0, i2c_bus_handle_1;

#ifdef TCP_CLIENT_SERVER
	extern QueueHandle_t tcp_send_queue, tcp_receive_queue;
	extern int commstate;
#endif

#endif //EXTERNAL_DEFS_H_
