/* cmd_wifi.c
*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <esp_wifi_types_generic.h>
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "lwip/netif.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_timer.h"
#include <nvs.h>
#include "common_defines.h"
#include "project_specific.h"
#ifdef MDNS
	#include "mdns.h"
#endif
#include "utils.h"
#include "cmd_wifi.h"

#if WIFI_STA_ON == 1 || WIFI_AP_ON == 1
static bool initialized = false;
EventGroupHandle_t comm_event_group;


static esp_timer_handle_t wifi_retry_timer;
static int retry_delay_ms = 1000;
#define MAX_RETRY_DELAY_MS 30000


#if WIFI_AP_ON == 1
	#include "esp_chip_info.h"
	#include "esp_mac.h"
	static bool ap_init = false;
#endif
#if WIFI_STA_ON
	static bool sta_init = false;
#endif

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


static void wifi_retry_cb(void *arg)
	{
    ESP_LOGI(WIFITAG, "Retrying WiFi connection...");
    esp_wifi_connect();
	}


static void wifi_retry_timer_init(void)
	{
    esp_timer_create_args_t args = {
        .callback = &wifi_retry_cb,
        .name = "wifi_retry"
        // dispatch_method defaults to ESP_TIMER_TASK
    	};

    ESP_ERROR_CHECK(esp_timer_create(&args, &wifi_retry_timer));
	}

static void print_auth_mode(int authmode, char *logbuf)
	{
    switch (authmode) 
	    {
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
#include "mdns.h"
static void initialise_mdns(void)
	{
	char ctrldevID[20];
	sprintf(ctrldevID, "%s%03d", HOSTNAME, dev_conf.dev_id);
    mdns_init();
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set(HOSTNAME);


    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"id", ctrldevID}
    	};
    ESP_ERROR_CHECK(mdns_service_add(HOSTNAME, "_esp32", "_tcp", TCPCOMMPORT, serviceTxtData, 2));
    
	}
#endif
static int wifi_disconnect()
	{
	esp_err_t err;
	wifi_mode_t mode;
	ESP_LOGI(WIFITAG, "WIFI STA disconnecting");
	err  = esp_wifi_get_mode(&mode);
	if(err != ESP_OK)
		{
		if(err == ESP_ERR_WIFI_NOT_INIT)
			{
			ESP_LOGI(WIFITAG, "WiFi not initialized");
			err = ESP_OK;
			}
		else
			ESP_LOGE(WIFITAG, "%s", esp_err_to_name(err));
		}
	else
		{
		//restart_in_progress = 1;
		TickType_t deadline = xTaskGetTickCount() + DISCONNECT_TIMEOUT / portTICK_PERIOD_MS;
		esp_wifi_disconnect();
		while((xEventGroupGetBits(comm_event_group) & WIFI_CONNECTED_BIT) != 0)
			{
			if (xTaskGetTickCount() >= deadline)
				{
				err = ESP_FAIL;
				ESP_LOGE(WIFITAG, "timeout on disconnect request");
            	break;
            	}
			vTaskDelay(pdMS_TO_TICKS(10));
			}
    	//restart_in_progress = 0;
		}
	return err;
	}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
	{
	if (event_base == WIFI_EVENT)
    	{
        switch (event_id)
        	{
#if WIFI_STA_ON				
			case WIFI_EVENT_STA_START:
            	ESP_LOGI(WIFITAG, "WIFI_EVENT_STA_START");
            	//esp_wifi_connect();
            	break;
	        case WIFI_EVENT_STA_CONNECTED:
	        	xEventGroupSetBits(comm_event_group, WIFI_CONNECTED_BIT);
	            ESP_LOGI(WIFITAG, "WIFI_EVENT_STA_CONNECTED");
	            break;
	        case WIFI_EVENT_STA_DISCONNECTED:
	        	{
	            wifi_event_sta_disconnected_t *evt =
	                (wifi_event_sta_disconnected_t *)event_data;
	            int reason = evt ? evt->reason : -1;
	            ESP_LOGW(WIFITAG, "WIFI_EVENT_STA_DISCONNECTED, reason=%d", reason);
	            xEventGroupClearBits(comm_event_group, WIFI_CONNECTED_BIT | IP_CONNECTED_BIT);
	            esp_timer_stop(wifi_retry_timer);
				ESP_ERROR_CHECK(esp_timer_start_once(wifi_retry_timer, retry_delay_ms * 1000LL));
				retry_delay_ms += 1000;
        		if (retry_delay_ms > MAX_RETRY_DELAY_MS)
            		retry_delay_ms = MAX_RETRY_DELAY_MS;
	            //ESP_ERROR_CHECK(esp_wifi_connect());
	            break;
	        	}
#endif /* WIFI_STA_ON */

#if WIFI_AP_ON
			case WIFI_EVENT_AP_START:
		        {
	            int8_t txpwr;
	            esp_chip_info_t info;
	            esp_chip_info(&info);
		
				/*
	            * ESP32-C3 TX power guard
	            */
		        if (info.model == CHIP_ESP32C3)
		            {
		            if (esp_wifi_set_max_tx_power(40) != ESP_OK)
		                    ESP_LOGW(WIFITAG, "Failed to set TX power for ESP32-C3");
		            }
	            esp_wifi_get_max_tx_power(&txpwr);
	            ESP_LOGI(WIFITAG, "WIFI_EVENT_AP_START, tx_power=%d", txpwr);
	            break;
		        }
	        case WIFI_EVENT_AP_STOP:
	            ESP_LOGI(WIFITAG, "WIFI_EVENT_AP_STOP");
	            break;
	        case WIFI_EVENT_AP_STACONNECTED:
		        {
	            wifi_event_ap_staconnected_t *evt =  (wifi_event_ap_staconnected_t *)event_data;
	            ESP_LOGI(WIFITAG, "AP client " MACSTR " joined, AID=%d", MAC2STR(evt->mac), evt->aid);
		        break;
		        }
	        case WIFI_EVENT_AP_STADISCONNECTED:
		        {
	            wifi_event_ap_stadisconnected_t *evt = (wifi_event_ap_stadisconnected_t *)event_data;
	            ESP_LOGI(WIFITAG, "AP client " MACSTR " left, AID=%d, reason=%d", MAC2STR(evt->mac), evt->aid, evt->reason);
		        break;
		        }
#endif /* WIFI_AP_ON */
     		default:
            	break;
        	}
    	}

/* -------------------- IP EVENTS -------------------- */
	else if (event_base == IP_EVENT)
		{
        switch (event_id)
        	{
#if WIFI_STA_ON
	        case IP_EVENT_STA_GOT_IP:
	        	{
	            ip_event_got_ip_t *evt =  (ip_event_got_ip_t *)event_data;
	            ESP_LOGI(WIFITAG, "IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&evt->ip_info.ip));
	            xEventGroupSetBits(comm_event_group, IP_CONNECTED_BIT);
	            break;
		        }
	        case IP_EVENT_STA_LOST_IP:
	            ESP_LOGW(WIFITAG, "IP_EVENT_STA_LOST_IP");
	            xEventGroupClearBits(comm_event_group, IP_CONNECTED_BIT);
				retry_delay_ms = 1000;
		        //esp_timer_stop(wifi_retry_timer);
	            break;
#endif /* WIFI_STA_ON */
			default:
        		break;
    		}
        }
	}

#if WIFI_STA_ON
static void wifi_init_sta(void)
	{
    if(sta_init)
    	{
        return;
        }
	assert(esp_netif_create_default_wifi_sta());
    wifi_config_t wifi_config = { 0 };
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    sta_init = true;
	}
#endif
#if WIFI_AP_ON
static void wifi_init_ap(void)
	{
	int ret;
	esp_netif_t *esp_netif_ap;
    if (ap_init) 
        return;
    esp_netif_ap = esp_netif_create_default_wifi_ap();
    assert(esp_netif_ap);

    wifi_config_t wifi_config = {
		.ap =
			{
	    	.ssid_len = strlen(dev_conf.ap_ssid),
	    	.channel = 1,
	    	.max_connection = 4,
	    	.authmode = WIFI_AUTH_WPA2_PSK,
	    	.pmf_cfg = {.required = false,},
	    	}
	    };
	strcpy((char *)(wifi_config.ap.ssid), dev_conf.ap_ssid);
	strcpy((char *)(wifi_config.ap.password), dev_conf.ap_pass);
	ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
	if(ret == ESP_OK)
		{
		esp_netif_ip_info_t ip_info;
		//IP4_ADDR(&ip_info.ip, dev_conf.ap_a, dev_conf.ap_b, dev_conf.ap_c, dev_conf.ap_d);
		//IP4_ADDR(&ip_info.gw, dev_conf.ap_a, dev_conf.ap_b, dev_conf.ap_c, dev_conf.ap_d);
		ip_info.ip.addr = dev_conf.ap_ip;
		ip_info.gw.addr = dev_conf.ap_ip; 
		IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
		ESP_ERROR_CHECK(esp_netif_dhcps_stop(esp_netif_ap));
		ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ip_info));
		ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
	    ap_init = true;
	    }
	else 
		{
		ESP_LOGE(WIFITAG, "error init AP %d", ret);
		}
	}
#endif
	
void initialise_wifi(void)
	{
#if !WIFI_STA_ON && !WIFI_AP_ON
    return;
#endif		
	if(initialized)
		return;
	initialized = false;

	ESP_LOGI(WIFITAG, "initialize_wifi");

	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(!comm_event_group)
    	comm_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
#if WIFI_STA_ON
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &event_handler, NULL) );
#endif
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	wifi_retry_timer_init();
	
#if WIFI_STA_ON && WIFI_AP_ON
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	wifi_init_sta();
	wifi_init_ap();
	ESP_ERROR_CHECK(esp_wifi_start());
#else
	#if WIFI_STA_ON
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		wifi_init_sta();
		ESP_ERROR_CHECK(esp_wifi_start());
	#elif WIFI_AP_ON
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		wifi_init_ap();
		ESP_ERROR_CHECK(esp_wifi_start());
	#endif
#endif

#ifdef MDNS
	initialise_mdns();
    char localhname[40];
    mdns_hostname_get(localhname);
    ESP_LOGI(WIFITAG, "mDNS hostname: %s.local", localhname);
#endif
	initialized = true;
	}

bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
	{
#if !WIFI_STA_ON
    return false;
#endif		
	char use_ssid[33] = {0};
    char use_pass[65] = {0};
	int bits = 0, ret;
    wifi_config_t wifi_config = { 0 };
    
	initialise_wifi();
	if(!initialized)
		return false;
	if(ssid == NULL)
		{
		if (dev_conf.sta_ssid[0] == '\0')
            return false;
        strlcpy(use_ssid, dev_conf.sta_ssid, sizeof(use_ssid));
        strlcpy(use_pass, dev_conf.sta_pass, sizeof(use_pass));
		}
	else
		{
		strlcpy(use_ssid, ssid, sizeof(use_ssid));
        if (pass)
            strlcpy(use_pass, pass, sizeof(use_pass));
		}

    ESP_LOGI(WIFITAG, "wifi_join(): STA SSID: \"%s\" STA PASS: \"%s\" timeout: %d", use_ssid, use_pass, timeout_ms);
    ESP_ERROR_CHECK( esp_wifi_get_config(WIFI_IF_STA, &wifi_config) );
    strlcpy((char *) wifi_config.sta.ssid, use_ssid, sizeof(wifi_config.sta.ssid));
	if (use_pass[0])
        strlcpy((char *) wifi_config.sta.password, use_pass, sizeof(wifi_config.sta.password));
	else
		wifi_config.sta.password[0] = '\0';
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    //if(wifi_disconnect() == ESP_OK)
    if((xEventGroupGetBits(comm_event_group) & WIFI_CONNECTED_BIT) != 0)
    	{
    	wifi_disconnect();
    	}

	ret = esp_wifi_connect();
	ESP_LOGI(WIFITAG, "esp_wifi_connect(): %d", ret);
	bits = xEventGroupWaitBits(comm_event_group, IP_CONNECTED_BIT,
                               pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
	ESP_LOGI(WIFITAG, "connect to %s return %x %x", use_ssid, ret, bits);
	return (bits & IP_CONNECTED_BIT) != 0;
	}

bool isConnected()
	{
	if(comm_event_group)
		return (xEventGroupGetBits(comm_event_group) & IP_CONNECTED_BIT) != 0;
	else
		return false;
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
				ESP_LOGI(WIFITAG, "Connected to \"%s\"  MAC:%02x:%02x:%02x:%02x:%02x:%02x Channel: %-d",
					ap_info.ssid, MAC2STR(ap_info.bssid), ap_info.primary);
					//ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2], ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5], ap_info.primary);
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
					ESP_LOGI(WIFITAG, "sta MAC = %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
					}
				} while (netif);
			}
		return 0;
		}
    ESP_LOGI(__func__, "Connecting to '%s'", ssid);

    bool connected = wifi_join(ssid, pwd,timeout);
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
	uint16_t sta_number = 0;
    uint8_t i;
    wifi_ap_record_t *ap_list_buffer;
    char logbuf[256], mac[20];;

    initialise_wifi();

    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    memset(ap_info, 0, sizeof(ap_info));
    ESP_LOGI(WIFITAG, "wifi scan_ start");
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
	if(!argc || !argv || !argv[0] || strcmp(argv[0], command))
		return 1;
	int nerrors = arg_parse(argc, argv, (void **)&wifi_args);
	if (nerrors != 0)
		{
		//arg_print_errors(stderr, wifi_args.end, argv[0]);
		return 1;
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
	return 0;
	}

bool get_sta_conf(char *ssid, esp_netif_ip_info_t *ipinfo)
	{
	wifi_ap_record_t ap_info;
	esp_netif_t *netif = NULL;
	int err;
	bool ret = false;
	if(isConnected())
		{
		err = esp_wifi_sta_get_ap_info(&ap_info);
		if(err == ESP_OK)
			{
			if(ssid)
				strcpy(ssid, (char *)(ap_info.ssid));
			do
				{
				netif = esp_netif_next_unsafe(netif);
				if(!netif)
					break;
				err = esp_netif_get_ip_info(netif, ipinfo);
				if(err == ESP_OK)
					{
					ret = true;
					break;
					}
				} while (netif);
			}
		}
	else
		{
		if(ssid)
			ssid[0] = 0;
		if(ipinfo)
			ipinfo->ip.addr = 0;
		}
	return ret;
	}
#endif
