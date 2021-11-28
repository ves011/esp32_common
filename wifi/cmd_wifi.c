/* Console example — WiFi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_netif.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "cmd_wifi.h"

static bool initialized = false;
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
#define WIFITAG "wifi op"

static void print_auth_mode(int authmode, char *logbuf)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_OPEN");
    	strcpy(logbuf, "WIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WEP");
    	strcpy(logbuf, "WIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA_PSK");
    	strcpy(logbuf, "WIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        strcpy(logbuf, "WIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        strcpy(logbuf, "WIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        strcpy(logbuf, "WIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        strcpy(logbuf, "WIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        strcpy(logbuf, "WIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        //ESP_LOGI(WIFITAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        strcpy(logbuf, "WIFI_AUTH_UNKNOWN");
        break;
    }
}

static int wifi_disconnect(int argc, char **argv)
	{
	esp_err_t err;
	wifi_mode_t mode;
	err  = esp_wifi_get_mode(&mode);
	if(err != ESP_OK)
		{
		if(err == ESP_ERR_WIFI_NOT_INIT)
			{
			ESP_LOGI(WIFITAG, "WiFi not initialized");
			return 0;
			}
		ESP_LOGE(WIFITAG, "%s", esp_err_to_name(err));
		}
	else
		{
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		wifi_config_t wifi_config = { 0 };
		memset(&cfg, 0, sizeof(wifi_init_config_t));
		ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
		ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
		esp_wifi_disconnect();
		// stop tcp_server if running

		}
	return 0;
	}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
	{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    	{
       	esp_wifi_connect();
       	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    	}
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    	{
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    	}
	}

static void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    if (initialized) {
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    //assert(ap_netif);
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    initialized = true;
}

bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
	{
	if(isConnected())
	    wifi_disconnect(1, NULL);
    initialise_wifi();
    wifi_config_t wifi_config = { 0 };
    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass)
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                   pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
    return (bits & CONNECTED_BIT) != 0;
	}

/** Arguments used by 'join' function */
static struct
	{
    struct arg_int *timeout;
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
	} join_args;

bool isConnected()
	{
	esp_err_t err;
	mode_t mode;
	wifi_ap_record_t ap_info;
	err = esp_wifi_get_mode(&mode);
	if(err == ESP_ERR_WIFI_NOT_INIT)
		return false;
	if(mode == WIFI_MODE_AP)
		return false;
	err = esp_wifi_sta_get_ap_info(&ap_info);
	if(err == ESP_ERR_WIFI_NOT_CONNECT)
		return false;
	return true;
	}
int wifi_connect(int argc, char **argv)
	{
	esp_err_t err;
	mode_t mode;
	wifi_ap_record_t ap_info;
	esp_netif_t *netif;
	esp_netif_ip_info_t ip_info;
	if(argc == 1) //no parameters just display connection information
		{
		err = esp_wifi_get_mode(&mode);
		if(err == ESP_ERR_WIFI_NOT_INIT)
			{
			ESP_LOGI(WIFITAG, "No connection. WiFi not initialized");
			return 0;
			}
		if(mode == WIFI_MODE_STA)
			{
			err = esp_wifi_sta_get_ap_info(&ap_info);
			if(err == ESP_OK)
				{
				ESP_LOGI(WIFITAG, "Connected to \"%s\"  MAC:%02x:%02x:%02x:%02x:%02x:%02x  Channel: %-d",
					ap_info.ssid, ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2], ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5], ap_info.primary);
				}
			else
				{
				ESP_LOGI(WIFITAG, "%s", esp_err_to_name(err));
				return 0;
				}

			netif = NULL;
			do
				{
				netif = esp_netif_next(netif);
				if(!netif)
					break;
				err = esp_netif_get_ip_info(netif, &ip_info);
				if(err == ESP_OK)
					{
					ESP_LOGI(WIFITAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(netif),
							IP2STR(&ip_info.ip),
							IP2STR(&ip_info.netmask),
							IP2STR(&ip_info.gw));

					}

				} while (netif);

			//esp_err_t esp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info);
			}
		else if (mode == WIFI_MODE_AP)
			{

			}
		else if(mode == WIFI_MODE_APSTA)
			{

			}
		else
			return 1;
		return 0;
		}
    int nerrors = arg_parse(argc, argv, (void **) &join_args);
    if (nerrors != 0)
    	{
        arg_print_errors(stderr, join_args.end, argv[0]);
        return 1;
    	}
    ESP_LOGI(__func__, "Connecting to '%s'", join_args.ssid->sval[0]);

    /* set default value*/
    if (join_args.timeout->count == 0)
    	{
        join_args.timeout->ival[0] = JOIN_TIMEOUT_MS;
    	}

    bool connected = wifi_join(join_args.ssid->sval[0],
                               join_args.password->sval[0],
                               join_args.timeout->ival[0]);
    if (!connected)
    	{
        ESP_LOGW(__func__, "Connection timed out");
        return 1;
    	}
    ESP_LOGI(__func__, "Connected");
    return 0;
	}

static int wifi_scan(int argc, char **argv)
	{
	esp_err_t err;
	wifi_mode_t mode;
	int deinit = 0;
	esp_netif_t *sta_netif;
	char logbuf[256];
	err  = esp_wifi_get_mode(&mode);
	if(err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) //error case return 1
		{
		ESP_LOGE(WIFITAG, "%s", esp_err_to_name(err));
		return 1;
		}
	else
		{
		if(mode == WIFI_MODE_AP)
			{
			ESP_LOGE(WIFITAG, "WiFi mode is AP. Cannot scan");
			return 1;
			}
		}
	if(err == ESP_ERR_WIFI_NOT_INIT) //wifi not initialized - need to init
		{
		ESP_ERROR_CHECK(esp_netif_init());
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		sta_netif = esp_netif_create_default_wifi_sta();
		assert(sta_netif);
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	    ESP_ERROR_CHECK(esp_wifi_start());
	    deinit = 1;
		}
	uint16_t number = SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(WIFITAG, "Total APs scanned = %u", ap_count);
    ESP_LOGI(WIFITAG, "\n");
    ESP_LOGI(WIFITAG, "           SSID             RSSI  Channel                 Auth mode");
    ESP_LOGI(WIFITAG, "-----------------------------------------------------------------------------------");
    for (int i = 0; (i < SCAN_LIST_SIZE) && (i < ap_count); i++)
    	{
    	print_auth_mode(ap_info[i].authmode, logbuf);
    	//sprintf(logbuf, "SSID: %25s\t\RSSI: %d\tChannel: %d", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);
        //ESP_LOGI(WIFITAG, "SSID: %25s \tRSSI: %5d\tChannel: %8d\tAuth mode: %s", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary, logbuf);
    	ESP_LOGI(WIFITAG, "%-25s%5d%8d               %-s", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary, logbuf);
        }
    ESP_LOGI(WIFITAG, "\n");
    if(deinit)
    	{
    	esp_netif_destroy(sta_netif);
    	esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
    	esp_netif_deinit();
    	esp_event_loop_delete_default();
    	}
    return 0;
	}

void register_wifi(void)
	{
    join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    join_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd =
    	{
        .command = "connect",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = &wifi_connect,
        .argtable = &join_args
    	};

    const esp_console_cmd_t wifi_scan_cmd =
        	{
            .command = "wifiscan",
            .help = "scan available wifi nw",
            .hint = NULL,
            .func = &wifi_scan,
            .argtable = NULL
        	};

    const esp_console_cmd_t wifi_disconnect_cmd =
    		{
            .command = "disconnect",
            .help = "Disconect WiFi ",
            .hint = NULL,
            .func = &wifi_disconnect,
            .argtable = NULL
    		};

    ESP_ERROR_CHECK( esp_console_cmd_register(&join_cmd) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_scan_cmd));
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_disconnect_cmd));
	}
