#ifndef EXTERNAL_DEFS_H_
#define EXTERNAL_DEFS_H_

extern char TOPIC_STATE[32], TOPIC_MONITOR[32], TOPIC_CTRL[32], TOPIC_CMD[32], TOPIC_LOG[32];
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	extern char TOPIC_STATE_A[32], TOPIC_MONITOR_A[32];
#endif

extern int tsync, client_connect_sync;
extern int controller_op_registered;
extern esp_netif_ip_info_t dev_ipinfo;
extern console_state_t console_state;
extern esp_vfs_spiffs_conf_t conf_spiffs;
extern esp_mqtt_client_handle_t client;
extern int client_connected;
extern int restart_in_progress;
extern char USER_MQTT[32];
extern TaskHandle_t tcp_log_task_handle;
extern QueueHandle_t tcp_log_evt_queue;
extern QueueHandle_t ui_cmd_q;
extern QueueHandle_t water_cmd_q;
#endif //EXTERNAL_DEFS_H_
