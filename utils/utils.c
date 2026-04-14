/*
 * utils.c
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "lwip/inet.h"
#include "project_specific.h"
#include "common_defines.h"
#include "utils.h"

dev_config_t dev_conf;
char *nvs_cl_crt, *nvs_cl_key, *nvs_ca_crt;
size_t nvs_cl_crt_sz, nvs_ca_crt_sz, nvs_cl_key_sz;

#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
	#include "tcp_log.h"
#endif

static const char *TAG = "SPIFFS_RW";

void my_esp_restart()
	{
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
	}

esp_vfs_spiffs_conf_t conf_spiffs =
	{
	.base_path = BASE_PATH,
	.partition_label = PARTITION_LABEL,
	.max_files = 5,
	.format_if_mount_failed = true
	};

int my_log_vprintf(const char *fmt, va_list arguments)
	{	
	switch (dev_conf.cs)
    	{
        case CONSOLE_OFF:
            // logs disabled
            return 0;
        case CONSOLE_ON:
            // serial console
            return vprintf(fmt, arguments);
        case CONSOLE_TCP:
        case CONSOLE_MQTT:
        	{
			char buf[512];
			vsnprintf(buf, sizeof(buf), fmt, arguments);
    		buf[sizeof(buf) - 1] = '\0';
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
            // best‑effort forward; no buffering
            tcp_log_message(buf);
            return strlen(buf);
#endif
			break;
			}
        default:
            break;
    	}
    return 0;
    }

int my_printf(char *format, ...)
	{
	char buf[512];
	va_list args;
	va_start( args, format );
	vsnprintf( buf, sizeof(buf) - 1, format, args );
	va_end( args );
	buf[sizeof(buf) - 1] = '\0';
	
	if(dev_conf.cs == CONSOLE_ON)
		return puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		{
		tcp_log_message(buf);
		return strlen(buf);
		}
#endif	
	return 0;
	}

void my_fputs(char *buf, FILE *f)
	{
	if(dev_conf.cs == CONSOLE_ON)
		puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		tcp_log_message(buf);
#endif	
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
        my_esp_restart();
		}
    ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
    if (ret != ESP_OK)
    	{
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        if(esp_spiffs_format(conf_spiffs.partition_label) != ESP_OK)
           	return ret;
        
        ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
		if(ret != ESP_OK)
        	return ret;
			
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
	if(nvs_open(NVS_CERT_NS, NVS_READONLY, &handle) == ESP_OK)
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
	nvs_ca_crt_sz = get_nvs_cert(NVSCACRT, &nvs_ca_crt);
	if(nvs_ca_crt_sz)
		{
		nvs_cl_crt_sz = get_nvs_cert(NVSCLCRT, &nvs_cl_crt);
		if(nvs_cl_crt_sz)
			{
			nvs_cl_key_sz = get_nvs_cert(NVSCLKEY, &nvs_cl_key);
			if(nvs_cl_key_sz)
				ret = ESP_OK;
			}
		}
	return ret;
	}
void get_nvs_conf()
	{
	size_t sz = 0;
	nvs_handle handle;
	char b[120];
	int ret;
	dev_conf.cs = DEFAULT_CONSOLE_STATE;
	dev_conf.dev_id = DEFAULT_DEVICE_ID;
	strcpy(dev_conf.dev_name, DEFAULT_DEVICE_NAME);
	strcpy(dev_conf.sta_ssid, DEFAULT_STA_SSID);
	strcpy(dev_conf.sta_pass, DEFAULT_STA_PASS);
	strcpy(dev_conf.ap_ssid, DEFAULT_AP_SSID);
	strcpy(dev_conf.ap_pass, DEFAULT_AP_PASS);
	strcpy(dev_conf.ap_hostname, DEFAULT_AP_HOSTNAME);
	dev_conf.ap_ip = DEFAULT_AP_IP;
	if(nvs_open(NVS_CFG_NS, NVS_READONLY, &handle) == ESP_OK)
		{
		ret = nvs_get_str(handle, NVSSTASSID, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSSTASSID, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.sta_ssid, b);
			}
		ret = nvs_get_str(handle, NVSSTAPASS, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSSTAPASS, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.sta_pass, b);
			}
		ret = nvs_get_str(handle, NVSAPSSID, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSAPSSID, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_ssid, b);
			}
		ret = nvs_get_str(handle, NVSAPPASS, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSAPPASS, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_pass, b);
			}
		ret = nvs_get_str(handle, NVSAPHOSTNAME, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSAPHOSTNAME, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_hostname, b);
			}
		ret = nvs_get_str(handle, NVSAPIP, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSAPIP, b, &sz);
			if(ret == ESP_OK)
				dev_conf.ap_ip = inet_addr(b);
			}
		ret = nvs_get_str(handle, NVSDEVNAME, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSDEVNAME, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.dev_name, b);
			}
		ret = nvs_get_str(handle, NVSDEVID, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSDEVID, b, &sz);
			if(ret == ESP_OK)
				dev_conf.dev_id = atoi(b);
			}
		ret = nvs_get_str(handle, NVSCONSTATE, NULL, &sz);
		if(ret == ESP_OK)
			{
			ret = nvs_get_str(handle, NVSCONSTATE, b, &sz);
			if(ret == ESP_OK)
				dev_conf.cs = atoi(b);
			}
		nvs_close(handle);
		}
	}
	
