/*
 * utils.c
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "sys/stat.h"
#include "project_specific.h"
#include "common_defines.h"
//#include "external_defs.h"
#include "utils.h"

dev_config_t dev_conf;
char *nvs_cl_crt, *nvs_cl_key, *nvs_ca_crt;
size_t nvs_cl_crt_sz, nvs_ca_crt_sz, nvs_cl_key_sz;

#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
//	#include "mqtt_client.h"
	#include "tcp_log.h"
#endif
/*
#include "external_defs.h"
#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
	#include "westaop.h"
#endif

#if ACTIVE_CONTROLLER == WATER_CONTROLLER
#include "waterop.h"
#endif
*/
static const char *TAG = "SPIFFS_RW";

void my_esp_restart()
	{
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
	}

static bool validateIPV4(int a, int b, int c, int d)
	{
	if(a > 253 || a < 1)
		return false;
	if(b > 253 || b < 0)
		return false;
	if(c > 253 || c < 0)
		return false;
	if(d > 253 || d < 1)
		return false;
	return true;
	}

esp_vfs_spiffs_conf_t conf_spiffs =
	{
	.base_path = BASE_PATH,
	.partition_label = PARTITION_LABEL,
	.max_files = 5,
	.format_if_mount_failed = true
	};
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
int my_log_vprintf(const char *fmt, va_list arguments)
	{
	if(dev_conf.cs == CONSOLE_ON)
		return vprintf(fmt, arguments);
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		{
		char buf[1024];
		vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
		//strip special characters
		return tcp_log_message(buf);
		}
	//else if(console_state >= CONSOLE_BTLE)
	//	{
	//	char buf[1024];
	//	vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
	//	return bt_log_message(buf);
	//	}
	else
		return 1;
	}
#endif

void my_printf(char *format, ...)
	{
	char buf[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( buf, sizeof(buf) - 1, format, args );
	va_end( args );
	if(dev_conf.cs == CONSOLE_ON)
		puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		tcp_log_message(buf);
#endif	
	//else if(console_state >= CONSOLE_BTLE)
	//	bt_log_message(buf);
	}
void my_fputs(char *buf, FILE *f)
	{
	if(dev_conf.cs == CONSOLE_ON)
		puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		tcp_log_message(buf);
#endif	
	//else if(console_state >= CONSOLE_BTLE)
	//	bt_log_message(buf);
	}
/*
int rw_console_state(int rw, console_state_t *cs)
	{
	struct stat st;
	int ret = ESP_FAIL;
	char buf[80];
	if(rw == PARAM_READ)
		{
		if (stat(BASE_PATH"/"CONSOLE_FILE, &st) != 0)
			{
			// file does no exists
			ESP_LOGI(TAG, "file does no exist: --%s--", BASE_PATH"/"CONSOLE_FILE);
			ret = ESP_OK;
			}
		else
			{
			FILE *f = fopen(BASE_PATH"/"CONSOLE_FILE, "r");
			if (f != NULL)
				{
				if(fgets(buf, 64, f))
					console_state = atoi(buf);
				else
					console_state = CONSOLE_ON;
				fclose(f);
				ret = ESP_OK;
				}
			else
				{
				ESP_LOGE(TAG, "Failed to open console file for reading");
				//esp_vfs_spiffs_unregister(conf.partition_label);
				return ret;
				}
			}
		}
	else if(rw == PARAM_WRITE)
		{
		FILE *f = fopen(BASE_PATH"/"CONSOLE_FILE, "w");
		if (f == NULL)
			ESP_LOGE(TAG, "Failed to create console param file");
		else
			{
			sprintf(buf, "%d\n", *(int *)cs);
			if(fputs(buf, f) >= 0)
				ret = ESP_OK;
			fclose(f);
			}
		}
	return ret;
	}
*/
int rw_dev_config(int rw)
	{
	struct stat st;
	int ret = ESP_FAIL;
	char buf[80];
	FILE *f;
	bool cs = false, dn = false, id = false, ss = false, pass = false, asid = false, apass = false, aip = false;
	if(rw == PARAM_READ)
		{
		ESP_LOGI(TAG, "default console: %d", DEFAULT_CONSOLE_STATE);
		dev_conf.cs = DEFAULT_CONSOLE_STATE;
		dev_conf.dev_id = DEFAULT_DEVICE_ID;
		strcpy(dev_conf.dev_name, DEFAULT_DEVICE_NAME);
		strcpy(dev_conf.sta_ssid, DEFAULT_STA_SSID);
		strcpy(dev_conf.sta_pass, DEFAULT_STA_PASS);
		strcpy(dev_conf.ap_ssid, DEFAULT_AP_SSID);
		strcpy(dev_conf.ap_pass, DEFAULT_AP_PASS);
		strcpy(dev_conf.ap_hostname, DEFAULT_AP_HOSTNAME);
		dev_conf.ap_a = DEFAULT_AP_A, dev_conf.ap_b = DEFAULT_AP_B, dev_conf.ap_c = DEFAULT_AP_C, dev_conf.ap_d = DEFAULT_AP_D;
		if(stat(BASE_PATH"/"DEVCONF_FILE, &st) != 0)
			{
			ESP_LOGI(TAG, "no %s file found. Taking default values", DEVCONF_FILE);
			}
		else
			{
			f = fopen(BASE_PATH"/"DEVCONF_FILE, "r");
			if(f)
				{
				while(fgets(buf, 64, f))
					{
					if(strstr(buf, CONSOLE_TXT))
						{
						dev_conf.cs = atof(buf + strlen(CONSOLE_TXT));
						cs = true;
						}
					if(strstr(buf, ID_TXT))
						{
						dev_conf.dev_id = atof(buf + strlen(ID_TXT));
						id = true;
						}
					if(strstr(buf, NAME_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcpy(dev_conf.dev_name, buf + strlen(NAME_TXT));
						dn = true;
						}
					if(strstr(buf, STA_SSID_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcpy(dev_conf.sta_ssid, buf + strlen(STA_SSID_TXT));
						ss = true;
						}
					if(strstr(buf, STA_PWD_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcpy(dev_conf.sta_pass, buf + strlen(STA_PWD_TXT));
						pass = true;
						}
					if(strstr(buf, AP_SSID_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcpy(dev_conf.ap_ssid, buf + strlen(AP_SSID_TXT));
						asid = true;
						}
					if(strstr(buf, AP_PWD_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcpy(dev_conf.ap_pass, buf + strlen(AP_PWD_TXT));
						apass = true;
						}
					if(strstr(buf, AP_IP_TXT))
						{
						while(buf[strlen(buf) - 1] == 0x0a || buf[strlen(buf) - 1] == 0x0d)
							buf[strlen(buf) - 1] = 0;
						strcat(buf, ".");
						char *pstr;
						pstr = strtok(buf, ".");
						
						if(pstr)
							dev_conf.ap_a = atoi(pstr);
						else
							dev_conf.ap_a = 0;
						pstr = strtok(NULL, ".");
						if(pstr)
							dev_conf.ap_b = atoi(pstr);
						else
							dev_conf.ap_b = 0;
						pstr = strtok(NULL, ".");
						if(pstr)
							dev_conf.ap_c = atoi(pstr);
						else
							dev_conf.ap_c = 0;
						pstr = strtok(NULL, ".");
						if(pstr)
							dev_conf.ap_d = atoi(pstr);
						else
							dev_conf.ap_d = 0;
						aip = true;
						}
					}
				fclose(f);
				if(!cs)
					ESP_LOGI(TAG, "console state entry not found. taking default value: %d", dev_conf.cs);
				if(!id)
					ESP_LOGI(TAG, "device ID entry not found. taking default value: %d", dev_conf.dev_id);
				if(!dn)
					ESP_LOGI(TAG, "device name entry not found. taking default value: %s", dev_conf.dev_name);
				if(!ss)
					ESP_LOGI(TAG, "sta ssid entry not found. taking default value: %s", dev_conf.sta_ssid);
				if(!pass)
					ESP_LOGI(TAG, "sta pass entry not found. taking default value: %s", dev_conf.sta_pass);
				if(!asid)
					ESP_LOGI(TAG, "ap ssid entry not found. taking default value: %s", dev_conf.ap_ssid);
				if(!apass)
					ESP_LOGI(TAG, "ap pass entry not found. taking default value: %s", dev_conf.ap_pass);
				if(!aip)
					ESP_LOGI(TAG, "sta pass entry not found. taking default value: %s", dev_conf.sta_pass);
				else
					{
					if(!validateIPV4(dev_conf.ap_a, dev_conf.ap_b, dev_conf.ap_c, dev_conf.ap_d))
						{
						dev_conf.ap_a = DEFAULT_AP_A, dev_conf.ap_b = DEFAULT_AP_B, dev_conf.ap_c = DEFAULT_AP_C, dev_conf.ap_d = DEFAULT_AP_D;
						ESP_LOGI(TAG, "ap IP entry found but it's not a valid IPV4 address. taking default value: %d.%d.%d.%d", 
							dev_conf.ap_a, dev_conf.ap_b, dev_conf.ap_c, dev_conf.ap_d);
						}
					}
				ret = ESP_OK;
				}
			}
		}
	else if(rw == PARAM_WRITE)
		{
		FILE *f = fopen(BASE_PATH"/"DEVCONF_FILE, "w");
		if (f == NULL)
			ESP_LOGE(TAG, "Failed to create configuration file");
		else 
			{
			sprintf(buf, "%s%d\n", CONSOLE_TXT, dev_conf.cs);
			if(fputs(buf, f) >= 0)
				{
				sprintf(buf, "%s%d\n", ID_TXT, dev_conf.dev_id);
				if(fputs(buf, f) >= 0)
					{
					sprintf(buf, "%s%s\n", NAME_TXT, dev_conf.dev_name);
					if(fputs(buf, f) >= 0)
						{
						sprintf(buf, "%s%s\n", STA_SSID_TXT, dev_conf.sta_ssid);
						if(fputs(buf, f) >= 0)
							{
							sprintf(buf, "%s%s\n", STA_PWD_TXT, dev_conf.sta_pass);
							if(fputs(buf, f) >= 0)
								{
								sprintf(buf, "%s%s\n", AP_SSID_TXT, dev_conf.ap_ssid);
								if(fputs(buf, f) >= 0)
									{
									sprintf(buf, "%s%s\n", AP_PWD_TXT, dev_conf.ap_pass);
									if(fputs(buf, f) >= 0)
										{
										sprintf(buf, "%s%d.%d.%d.%d\n", AP_IP_TXT, dev_conf.ap_a, dev_conf.ap_b, dev_conf.ap_c, dev_conf.ap_d);
										if(fputs(buf, f) >= 0)
											ret = ESP_OK;
										}
									}
								}
							}
						}
					}
				}
			fclose(f);
			}
		}
	return ret;
	}
int spiffs_storage_check()
	{
	esp_err_t ret;
    size_t total = 0, used = 0;
    ret = esp_vfs_spiffs_register(&conf_spiffs);
    if (ret != ESP_OK)
		{
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        esp_restart();
		}
    ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
    if (ret != ESP_OK)
    	{
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        if(esp_spiffs_format(conf_spiffs.partition_label) != ESP_OK)
        	{
        	//esp_vfs_spiffs_unregister(conf.partition_label);
        	return ret;
        	}
        ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
        if(ret != ESP_OK)
        	{
			//esp_vfs_spiffs_unregister(conf.partition_label);
			return ret;
			}
		// Check consistency of reported partiton size info.
		if (used > total)
			{
			ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
			ret = esp_spiffs_check(conf_spiffs.partition_label);
			// Could be also used to mean broken files, to clean unreferenced pages, etc.
			// More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
			if (ret != ESP_OK)
				{
				ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
				//esp_vfs_spiffs_unregister(conf.partition_label);
				return ret;
				}
			}
    	}
	ESP_LOGI(TAG, "SPIFFS_check() successful");
    return ESP_OK;
    }
int get_nvs_cert(char * entry_name, char **cert)
	{
	char *t = "NVS";
	size_t sz = 0;
	nvs_handle handle;
	if(nvs_open("certificates", NVS_READONLY, &handle) == ESP_OK)
		{
		if(nvs_get_str(handle, entry_name, NULL, &sz) == ESP_OK)
			{
			if(*cert)
				free(*cert);
			*cert = calloc(sz, 1);
			if(cert)
				{
				if(nvs_get_str(handle, entry_name, *cert, &sz) != ESP_OK)
					{
					ESP_LOGE(t, "error reading %s from NVS", entry_name);
					free(*cert);
					*cert = NULL;
					}
				}
			else
				ESP_LOGE(t, "could not allocate memory for %s", entry_name);
			}
		else
			ESP_LOGI(t, "%s entry not found in certificates namespace", entry_name);
		
		nvs_close(handle);
		}
	else
		ESP_LOGI(t, "certificates namespace not found");
	ESP_LOGI(t, "%s - size: %d", entry_name, sz);
	return sz;
	}

int get_all_nvscerts()
	{
	int ret = ESP_FAIL;
	nvs_ca_crt_sz = get_nvs_cert("ca-crt", &nvs_ca_crt);
	if(nvs_ca_crt_sz)
		{
		nvs_cl_crt_sz = get_nvs_cert("client-crt", &nvs_cl_crt);
		if(nvs_cl_crt_sz)
			{
			nvs_cl_key_sz = get_nvs_cert("client-key", &nvs_cl_key);
			if(nvs_cl_key_sz)
				ret = ESP_OK;
			}
		}
	return ret;
	}
	
