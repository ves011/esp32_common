/* 
 cmd_wifi.h
*/
#ifndef CMD_WIFI_H__
#define CMD_WIFI_H__

#include <stdbool.h>
#include "esp_netif_types.h"
#include "freertos/idf_additions.h"
#define SCAN_LIST_SIZE 30
#define JOIN_TIMEOUT_MS 	15000
#define DISCONNECT_TIMEOUT 	1000

extern EventGroupHandle_t comm_event_group;
extern int restart_in_progress;

void register_wifi(void);
bool isConnected(void);
bool wifi_reconnect();
bool wifi_join(const char *ssid, const char *pass, int timeout_ms);
//void do_wifi_cmd(int argc, char **argv);
int do_wifi(int argc, char **argv);
void initialise_wifi(void);
bool get_sta_conf(char *ssid, esp_netif_ip_info_t *ipinfo);
int get_nvs_stacred(char *ssid, char *passwd);

#endif
