#ifndef EXTERNAL_DEFS_H_
#define EXTERNAL_DEFS_H_

//#include "project_specific.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_netif.h"

extern char TOPIC_STATE[32], TOPIC_MONITOR[32], TOPIC_CTRL[32], TOPIC_CMD[32], TOPIC_LOG[32], TOPIC_KA[32];
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
#ifdef USE_I2C
#include "driver/i2c_master.h"
extern i2c_master_bus_handle_t i2c_bus_handle_0, i2c_bus_handle_1;
#endif

#ifdef TCP_CLIENT_SERVER
	extern QueueHandle_t tcp_send_queue, tcp_receive_queue;
	extern int commstate;
#endif

#if (COMM_PROTO & BLE_PROTO) == BLE_PROTO
	extern QueueHandle_t msg2remote_queue;
#endif

#endif //EXTERNAL_DEFS_H_
