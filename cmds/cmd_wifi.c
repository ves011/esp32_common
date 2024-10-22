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
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/netif.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_netif.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "common_defines.h"
#include "external_defs.h"
#include "utils.h"
#include "cmd_wifi.h"
#include "wifi_credentials.h"

static bool initialized = false;
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
//static int scan_done;
//esp_netif_ip_info_t dev_ipinfo;

#if WIFI_AP_ON
static bool ap_init = false;
esp_netif_t *esp_netif_ap;
#endif
#if WIFI_STA_ON
esp_netif_t *esp_netif_sta;
static bool sta_init = false;
#endif

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
	int bits = 0;
	ESP_LOGI(WIFITAG, "WIFI STA disconnecting");
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
		restart_in_progress = 1;
		esp_wifi_disconnect();
		do
	    	{
	    	bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
	                                   pdFALSE, pdTRUE, 1000 / portTICK_PERIOD_MS);
	    	} while((bits & CONNECTED_BIT) != 0);
	    	
    	restart_in_progress = 0;
		}
	return 0;
	}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
	{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    	{
		ESP_LOGE(WIFITAG, "WiFi connection lost");
        if(!restart_in_progress)
    		{
			ESP_ERROR_CHECK(esp_wifi_disconnect());
			ESP_ERROR_CHECK(esp_wifi_connect());
			}
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    	}
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) 
    	{
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(WIFITAG, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
    	} 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    	{
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(WIFITAG, "Station "MACSTR" left, AID=%d, reason:%d", MAC2STR(event->mac), event->aid, event->reason);
        }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) 
    	{
		ESP_LOGI(WIFITAG, "AP start");
		}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) 
    	{
		ESP_LOGI(WIFITAG, "AP stop");
		}
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    	{
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ip_event_got_ip_t *ipevt = event_data;
        //memcpy(&dev_ipinfo, &ipevt->ip_info, sizeof(dev_ipinfo));

    	}
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    	{
    	ESP_LOGE(WIFITAG, "IP lost / WIFI STA disconnected");
    	}
	}
/*
static void scan_done_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
	{
	//ESP_LOGI(WIFITAG, "SCAN DONE event");
	scan_done = 1;
	}
*/
#if WIFI_STA_ON
static void wifi_init_sta(void)
	{
    if (sta_init) 
        return;

    esp_netif_sta = esp_netif_create_default_wifi_sta();
    assert(esp_netif_sta);

    wifi_config_t wifi_config = { 0 };
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    sta_init = true;
	}
#endif
#if WIFI_AP_ON
static void wifi_init_ap(void)
	{
    if (ap_init) 
        return;

    esp_netif_ap = esp_netif_create_default_wifi_ap();
    assert(esp_netif_ap);

    wifi_config_t wifi_config =
    	{
		.ap =
			{
	    	.ssid = AP_SSID,
	    	.ssid_len = strlen(AP_SSID),
	    	.channel = 6,
	    	.password = AP_PASS,
	    	.max_connection = 4,
	    	.authmode = WIFI_AUTH_WPA2_PSK,
	    	.pmf_cfg = {.required = false,},
	    	}
	    };
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ap_init = true;
	}
#endif
	
void initialise_wifi(void)
	{
	if(initialized)
		return;
#if !WIFI_AP_ON && !WIFI_STA_ON
	return;
#endif
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(!wifi_event_group)
    	wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &event_handler, NULL) );
    	 
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    
#if WIFI_AP_ON
    /* Initialize AP */
    ESP_LOGI(WIFITAG, "ESP_WIFI_MODE_AP");
	#if WIFI_STA_ON
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	#else
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	#endif
	wifi_init_ap();
#endif   

#if WIFI_STA_ON
	ESP_LOGI(WIFITAG, "ESP_WIFI_MODE_STA");
	#if WIFI_AP_ON
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	#else
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	#endif
	wifi_init_sta();
#endif
	ESP_ERROR_CHECK(esp_wifi_start());
	initialized = true;
	}

bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
	{
	int bits = 0;
	if(isConnected())
	    wifi_disconnect(1, NULL);
    initialise_wifi();
    wifi_config_t wifi_config = { 0 };
    ESP_ERROR_CHECK( esp_wifi_get_config(WIFI_IF_STA, &wifi_config) );
    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass)
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    while((bits & CONNECTED_BIT) != CONNECTED_BIT)
    	{
    	esp_wifi_connect();
    	bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                   pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
    	//ESP_LOGI(WIFITAG, "WiFi connect timeout. retrying...");
    	}
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
	wifi_mode_t mode;
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
	wifi_mode_t mode;
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
		if(mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
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
				netif = esp_netif_next_unsafe(netif);
				if(!netif)
					break;
				uint8_t mac[NETIF_MAX_HWADDR_LEN];
				esp_netif_get_mac(netif, mac);
				err = esp_netif_get_ip_info(netif, &ip_info);
				if(err == ESP_OK)
					{
					ESP_LOGI(WIFITAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(netif),
							IP2STR(&ip_info.ip),
							IP2STR(&ip_info.netmask),
							IP2STR(&ip_info.gw));
					ESP_LOGI(WIFITAG, "sta MAC = %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);//, netif->mac[1], netif->mac[2], netif->mac[3], netif->mac[4], netif->mac[5]);

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
        //arg_print_errors(stderr, join_args.end, argv[0]);
        my_printf("%s arguments error\n", argv[0]);
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
	uint16_t sta_number = 0;
    uint8_t i;
    wifi_ap_record_t *ap_list_buffer;
    char logbuf[256], mac[20];;
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
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    memset(ap_info, 0, sizeof(ap_info));
    //scan_done = 0;
    ESP_LOGI(WIFITAG, "scan_done start");
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

	ESP_LOGI(WIFITAG, "scan_done complete");
	esp_wifi_scan_get_ap_num(&sta_number);
	if (!sta_number)
		{
		ESP_LOGE(WIFITAG, "No AP found\n");
		return 1;
		}
	ESP_LOGI(WIFITAG, "Total APs scanned = %u", sta_number);

	ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
	if (ap_list_buffer == NULL)
		{
		ESP_LOGE(WIFITAG, "Failed to malloc buffer to print scan results\n");
		return 1;
		}

	if (esp_wifi_scan_get_ap_records(&sta_number,(wifi_ap_record_t *)ap_list_buffer) == ESP_OK)
		{
		ESP_LOGI(WIFITAG, "           SSID                        RSSI  Channel                 Auth mode");
		ESP_LOGI(WIFITAG, "---------------------------------------------------------------------------------------");
		for(i=0; i<sta_number; i++)
			{
			print_auth_mode(ap_list_buffer[i].authmode, logbuf);
			sprintf(mac, "(%02x:%02x:%02x:%02x:%02x:%02x)", ap_list_buffer[i].bssid[0], ap_list_buffer[i].bssid[1], ap_list_buffer[i].bssid[2],
					ap_list_buffer[i].bssid[3], ap_list_buffer[i].bssid[3], ap_list_buffer[i].bssid[5]);

			ESP_LOGI(WIFITAG, "%-18s%18s%5d%8d               %-s", ap_list_buffer[i].ssid, mac, ap_list_buffer[i].rssi, ap_list_buffer[i].primary, logbuf);
			}
		}

	free(ap_list_buffer);
	ESP_LOGI(WIFITAG, "AP scan done\n");

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

void do_wifi_cmd(int argc, char **argv)
	{
	if(!strcmp(argv[0], "wifiscan"))
		wifi_scan(argc, argv);
	else if(!strcmp(argv[0], "disconnect"))
		wifi_disconnect(argc, argv);
	else if(!strcmp(argv[0], "connect"))
		wifi_connect(argc, argv);
	}
