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
#include "freertos/projdefs.h"
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
#include "driver/i2c_master.h"

#include "lwip/apps/netbiosns.h"
#include "common_defines.h"
#include "external_defs.h"
#include "project_specific.h"
#ifdef MDNS
	#include "mdns.h"
#endif
#include "utils.h"
#include "cmd_wifi.h"
#include "wifi_credentials.h"

static bool initialized = false;
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
//static int scan_done;
esp_netif_ip_info_t dev_ipinfo;

//#if WIFI_AP_ON
static bool ap_init = false;
esp_netif_t *esp_netif_ap;
static char ap_pass[32];
static char ap_ssid[32];
static char sta_pass[32];
static char sta_ssid[32];
static uint8_t ap_a, ap_b, ap_c, ap_d;
//#endif
//#if WIFI_STA_ON
esp_netif_t *esp_netif_sta;
static bool sta_init = false;
//#endif

#define WIFITAG "wifi op"
static char *command = "wifi";
static struct
	{
    struct arg_str *op;
    struct arg_str *ssid;
    struct arg_str *pwd;
    struct arg_int *timeout;
    struct arg_end *end;
	} wifi_args;

static void get_wifi_conf();

#if WIFI_AP_ON	
	static int get_ap_conf();
#endif

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
#ifdef MDNS
static void initialise_mdns(void)
	{
	char ctrldevID[20];
	sprintf(ctrldevID, "%03d", CTRL_DEV_ID);
    mdns_init();
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set(MDNSINSTANCE);


    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"id", ctrldevID}
    	};

    //ESP_ERROR_CHECK(mdns_service_add(MDNSINSTANCENAME, "_tcp", "_tcp", TCPCOMMPORT, serviceTxtData,
    //                                 sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
    ESP_ERROR_CHECK(mdns_service_add(MDNSINSTANCENAME, "_esp32", "_tcp", TCPCOMMPORT, serviceTxtData, 2));
    
	}
#endif
static int wifi_disconnect()
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
	msg_t msg;
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    	{
		ESP_LOGE(WIFITAG, "WiFi connection lost");
		if(dev_mon_queue)
			{
			msg.source = MSG_WIFI;
			msg.val = 0;
			xQueueSend(dev_mon_queue, &msg, pdMS_TO_TICKS(10));
			}
        if(!restart_in_progress)
    		{
			//ESP_ERROR_CHECK(esp_wifi_disconnect());
			//ESP_ERROR_CHECK(esp_wifi_connect());
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
        memcpy(&dev_ipinfo, &ipevt->ip_info, sizeof(dev_ipinfo));
		if(dev_mon_queue)
			{
			msg.source = MSG_WIFI;
			msg.val = 1;
			xQueueSend(dev_mon_queue, &msg, pdMS_TO_TICKS(10));
			}
    	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    	{
    	ESP_LOGE(WIFITAG, "IP lost / WIFI STA disconnected");
    	}
	}

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
    wifi_config.sta.failure_retry_cnt = 3;
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

    wifi_config_t wifi_config = {
		.ap =
			{
	    	//.ssid = ap_ssid,
	    	.ssid_len = strlen(ap_ssid),
	    	.channel = 6,
	    	//.password = ap_pass,
	    	.max_connection = 4,
	    	.authmode = WIFI_AUTH_WPA2_PSK,
	    	.pmf_cfg = {.required = false,},
	    	}
	    };
	strcpy((char *)(wifi_config.ap.ssid), ap_ssid);
	strcpy((char *)(wifi_config.ap.password), ap_pass);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	esp_netif_ip_info_t ip_info;
	IP4_ADDR(&ip_info.ip, ap_a, ap_b, ap_c, ap_d);
	IP4_ADDR(&ip_info.gw, ap_a, ap_b, ap_c, ap_d); 
	IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(esp_netif_dhcps_stop(esp_netif_ap));
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
    ap_init = true;
	}
#endif
	
void initialise_wifi(void)
	{
	if(initialized)
		return;
	initialized = false;
	ESP_LOGI(WIFITAG, "initialize_wifi");
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
    get_wifi_conf();
  
//#if WIFI_AP_ON
    /* Initialize AP */
    //get_ap_conf();
    //ESP_LOGI(WIFITAG, "ESP_WIFI_MODE_AP");
	//#if WIFI_STA_ON
	//    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	//#else
	//	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	//#endif
	//wifi_init_ap();
//#endif  

#if WIFI_STA_ON && WIFI_AP_ON
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	wifi_init_ap();
	wifi_init_sta();
	ESP_ERROR_CHECK(esp_wifi_start());
	//wifi_join(sta_ssid, sta_pass, JOIN_TIMEOUT_MS);
#else
	#if WIFI_STA_ON
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		wifi_init_sta();
		ESP_ERROR_CHECK(esp_wifi_start());
		//wifi_join(sta_ssid, sta_pass, JOIN_TIMEOUT_MS);
	#elif WIFI_AP_ON
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		wifi_init_ap();
		ESP_ERROR_CHECK(esp_wifi_start());
	#endif
#endif
/*
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));
#if WIFI_STA_ON
	//ESP_LOGI(WIFITAG, "ESP_WIFI_MODE_STA");
	//#if WIFI_AP_ON
	//    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	//#else
	//	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	//#endif
	wifi_init_sta();
	initialized = true;
	wifi_join(sta_ssid, sta_pass, JOIN_TIMEOUT_MS);
#endif
	ESP_ERROR_CHECK(esp_wifi_start());
*/
#ifdef MDNS
	initialise_mdns();
    char localhname[40];
    mdns_hostname_get(localhname);
    ESP_LOGI(WIFITAG, "mDNS hostname: %s.local", localhname);
#endif

#if WIFI_AP_ON
	//esp_netif_ip_info_t ip_info;
	//IP4_ADDR(&ip_info.ip, ap_a, ap_b, ap_c, ap_d);
	//IP4_ADDR(&ip_info.gw, ap_a, ap_b, ap_c, ap_d); 
	//IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
	//ESP_ERROR_CHECK(esp_netif_dhcps_stop(esp_netif_ap));
	//ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ip_info));
	//ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
#endif	
	initialized = true;
	//wifi_join(sta_ssid, sta_pass, JOIN_TIMEOUT_MS);
	}

bool wifi_reconnect()
	{
	return wifi_join(sta_ssid, sta_pass, JOIN_TIMEOUT_MS);
	}
bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
	{
	if(!isConnected(ssid))
	    wifi_disconnect();
	strcpy(sta_ssid, ssid);
	strcpy(sta_pass, pass);
    initialise_wifi();
#if WIFI_STA_ON
	int bits = 0, ret;
    wifi_config_t wifi_config = { 0 };
    ESP_ERROR_CHECK( esp_wifi_get_config(WIFI_IF_STA, &wifi_config) );
    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass)
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    //while((bits & CONNECTED_BIT) != CONNECTED_BIT)
    	{
    	ret = esp_wifi_connect();
    	bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                   pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
    	ESP_LOGI(WIFITAG, "esp_wifi_connect() return %x %x", ret, bits);
    	}
    return (bits & CONNECTED_BIT) != 0;
#else
	return false;
#endif
	}

bool isConnected(char *ssid)
	{
	int i;
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
	if(strlen((char *)ap_info.ssid) == 0)
		return false;
	for(i = 0; i < strlen(ssid); i++)
		{
		if(ap_info.ssid[i] != ssid[i])
			break;
		}
	if(i < strlen(ssid))
		return false;
	ESP_LOGI(WIFITAG, "isConnected() err %d", err);
	return true;
	}
static int wifi_connect(const char *ssid, const char *pwd, int timeout)
	{
	esp_err_t err;
	wifi_mode_t mode;
	wifi_ap_record_t ap_info;
	esp_netif_t *netif;
	esp_netif_ip_info_t ip_info;
	if(!ssid) //no parameters just display connection information
		{
		err = esp_wifi_get_mode(&mode);
		if(err == ESP_ERR_WIFI_NOT_INIT)
			{
			ESP_LOGI(WIFITAG, "No connection. WiFi not initialized");
			return 0;
			}
		//if(mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
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
				//return 0;
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
		/*
		else if (mode == WIFI_MODE_AP)
			{

			}
		else if(mode == WIFI_MODE_APSTA)
			{

			}
		else
			return 1;
		*/
		return 0;
		}
    ESP_LOGI(__func__, "Connecting to '%s'", ssid);

    bool connected = wifi_join(wifi_args.ssid->sval[0],
                               wifi_args.pwd->sval[0],
                               wifi_args.timeout->ival[0]);
    if (!connected)
    	{
        ESP_LOGW(__func__, "Connection timed out");
        return 1;
    	}
    ESP_LOGI(__func__, "Connected");
    return 0;
	}

static int wifi_scan()
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
	wifi_args.op = arg_str1(NULL, NULL, "<op>", "WIFI cmd: connect | disconnect | scan");
    wifi_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP");
    wifi_args.pwd = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    wifi_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    wifi_args.end = arg_end(2);
	const esp_console_cmd_t wifi_cmd =
    	{
        .command = command,
        .help = "wifi cmds",
        .hint = NULL,
        .func = &do_wifi,
        .argtable = &wifi_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_cmd));
	}

int do_wifi(int argc, char **argv)
	{
	if(strcmp(argv[0], command))
		return 1;
	int nerrors = arg_parse(argc, argv, (void **)&wifi_args);
	if (nerrors != 0)
		{
		arg_print_errors(stderr, wifi_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(strcmp(wifi_args.op->sval[0], "connect") == 0)
		{
		if(wifi_args.ssid->count != 0 && wifi_args.pwd->count != 0)
			{
			int to;
			if(wifi_args.timeout->count == 0)
				to = JOIN_TIMEOUT_MS;
			else
 				to = wifi_args.timeout->ival[0];
 			wifi_connect(wifi_args.ssid->sval[0], wifi_args.pwd->sval[0], to);
			}
		else
			wifi_connect(NULL, NULL, 0);
		}
	else if(strcmp(wifi_args.op->sval[0], "disconnect") == 0)
		{
		wifi_disconnect();
		}
	else if(strcmp(wifi_args.op->sval[0], "scan") == 0)
		{
		wifi_scan();
		}
	return ESP_OK;
	}
static void get_wifi_conf()
	{
	FILE *f = NULL;
	char buf[64];
	char ap_ss[32], ap_p[32], sta_ss[32], sta_p[32];
	uint8_t a, b, c, d;
	a = b = c = d = 0;
	ap_ss[0] = ap_p[0] = sta_ss[0] = sta_p[0] = 0;
	
	f = fopen(BASE_PATH"/"WIFICONF_FILE, "r");
	if(f)
		{
		while(fgets(buf, 64, f))
			{
			if(buf[strlen(buf) -1] == 0x0a)
				buf[strlen(buf) -1] = 0;
			if(strstr(buf, "APNW: "))
				sscanf(buf + strlen("APNW: "), "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d);
			else if(strstr(buf, "APSSID: "))
				sscanf(buf + strlen("APSSID: "), "%s", ap_ss);
			else if(strstr(buf, "APPASS: "))
				sscanf(buf + strlen("APPASS: "), "%s", ap_p);
			else if(strstr(buf, "STASSID: "))
				sscanf(buf + strlen("STASSID: "), "%s", sta_ss);
			else if(strstr(buf, "STAPASS: "))
				sscanf(buf + strlen("STAPASS: "), "%s", sta_p);
			}
		fclose(f);
		}
	else
		ESP_LOGI(WIFITAG, "cannot open %s", WIFICONF_FILE);
	if(a != 0){	ap_a = a; ap_b = b; ap_c = c; ap_d = d; }
	else {ap_a = AP_A; ap_b = AP_B; ap_c = AP_C; ap_d = AP_D;}
	if(strlen(ap_ss))
		strcpy(ap_ssid, ap_ss);
	else
		strcpy(ap_ssid, AP_SSID);
		
	if(strlen(ap_p))
		strcpy(ap_pass, ap_p);
	else
		strcpy(ap_pass, AP_PASS);
		
	if(strlen(sta_ss)) 
		strcpy(sta_ssid, sta_ss);
	else
		strcpy(sta_ssid, DEFAULT_SSID);
		
	if(strlen(sta_p))
		strcpy(sta_pass, sta_p);
	else
		strcpy(sta_pass, DEFAULT_PASS);
	ESP_LOGI(WIFITAG, "AP NW: %hhu.%hhu.%hhu.%hhu AP SSID: %s AP PASS: %s", ap_a, ap_b, ap_c, ap_d, ap_ssid, ap_pass);
	ESP_LOGI(WIFITAG, "STA SSID: %s STA PASS: %s", sta_ssid, sta_pass);
	}

#if WIFI_AP_ON
static int get_ap_conf()
	{
	int ret = ESP_FAIL;
	FILE *f = NULL;
	char buf[64];
	f = fopen(BASE_PATH"/"APCONF_FILE, "r");
	if(f)
		{
		if(fgets(buf, 64, f))
			{
			sscanf(buf, "%hhu.%hhu.%hhu.%hhu", &ap_a, &ap_b, &ap_c, &ap_d);
			if(fgets(buf, 64, f))
				{
				if(buf[strlen(buf) -1] == 0x0a)
					buf[strlen(buf) -1] = 0;
				strcpy(ap_ssid, buf);
				if(fgets(buf, 64, f))
					{
					if(buf[strlen(buf) -1] == 0x0a)
						buf[strlen(buf) -1] = 0;
					strcpy(ap_pass, buf);
					ret = ESP_OK;
					}
				}
			}
		fclose(f);
		}
	if(ret != ESP_OK)
		{
		ESP_LOGI(WIFITAG, "No AP conf file or invalid parameters. Taking default params");
		ap_a = 192, ap_b = 168, ap_c = 4, ap_d = 1;
		strcpy(ap_pass, AP_PASS);
		strcpy(ap_ssid, AP_SSID);	
		}
	printf("%x %x %x %x, %s %s", ap_a, ap_b, ap_c, ap_d, ap_ssid, ap_pass);
	return ret;
	}
#endif

void get_sta_conf(char *ssid, char *pass)
	{
	strcpy(ssid, sta_ssid);
	strcpy(pass, sta_pass);
	}