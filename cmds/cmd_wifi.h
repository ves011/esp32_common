/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef CMD_WIFI_H__
#define CMD_WIFI_H__

#define SCAN_LIST_SIZE 30
#define JOIN_TIMEOUT_MS (10000)

#define WIFICONF_FILE "wificonf.txt"

#if WIFI_AP_ON
	#define APCONF_FILE "apconf.txt"
	extern esp_netif_t *esp_netif_ap;
#endif
#if WIFI_STA_ON
	extern esp_netif_t *esp_netif_sta;
#endif

// Register WiFi functions
void register_wifi(void);
bool isConnected(const char *ssid);
//int wifi_connect(int argc, char **argv);
bool wifi_reconnect();
bool wifi_join(const char *ssid, const char *pass, int timeout_ms);
void do_wifi_cmd(int argc, char **argv);
int do_wifi(int argc, char **argv);
void initialise_wifi(void);
void get_sta_conf(char *ssid, char *pass);

#endif
