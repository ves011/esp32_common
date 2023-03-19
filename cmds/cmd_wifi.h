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

// Register WiFi functions
void register_wifi(void);
bool isConnected(void);
int wifi_connect(int argc, char **argv);
bool wifi_join(const char *ssid, const char *pass, int timeout_ms);

#endif
