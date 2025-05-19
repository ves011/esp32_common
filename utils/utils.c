/*
 * utils.c
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "errno.h"
#include "esp_spiffs.h"
#include "mqtt_client.h"
#include "sys/stat.h"
#include "common_defines.h"
//#include "mqtt_ctrl.h"
#include "external_defs.h"
#include "tcp_log.h"
#include "project_specific.h"


#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
#include "westaop.h"
#endif

#if ACTIVE_CONTROLLER == WATER_CONTROLLER
#include "waterop.h"
#endif

static const char *TAG = "SPIFFS_RW";

esp_vfs_spiffs_conf_t conf_spiffs =
	{
	.base_path = BASE_PATH,
	.partition_label = PARTITION_LABEL,
	.max_files = 5,
	.format_if_mount_failed = true
	};

int my_log_vprintf(const char *fmt, va_list arguments)
	{
	if(console_state == CONSOLE_ON)
		return vprintf(fmt, arguments);
	else if(console_state == CONSOLE_TCP || console_state == CONSOLE_MQTT)
		{
		char buf[1024];
		vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
		//strip special characters
		return tcp_log_message(buf);
		}
	else if(console_state >= CONSOLE_BTLE)
		{
		char buf[1024];
		vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
		return bt_log_message(buf);
		}
	else
		return 1;
	}

void my_printf(char *format, ...)
	{
	char buf[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( buf, sizeof(buf) - 1, format, args );
	va_end( args );
	if(console_state == CONSOLE_ON)
		puts(buf);
	else if(console_state == CONSOLE_TCP || console_state == CONSOLE_MQTT)
		tcp_log_message(buf);
	else if(console_state >= CONSOLE_BTLE)
		bt_log_message(buf);
	}
void my_fputs(char *buf, FILE *f)
	{
	if(console_state == CONSOLE_ON)
		puts(buf);
	else if(console_state == CONSOLE_TCP || console_state == CONSOLE_MQTT)
		tcp_log_message(buf);
	else if(console_state >= CONSOLE_BTLE)
		bt_log_message(buf);
	}
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

/*
#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
int write_tpdata(int rw, char *bufdata)
	{
	FILE *f = NULL;
	time_t now = 0;
    struct tm timeinfo = { 0 };
    char strtime[100], file_name[64], fbuf[400];
    int ret = ESP_FAIL;
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strtime, sizeof(strtime), "%Y-%m-%dT%H:%M:%S", &timeinfo);
	sprintf(file_name, "%s/%d.tph", BASE_PATH, timeinfo.tm_year + 1900);
	if(rw == PARAM_WRITE)
		{
		if(xSemaphoreTake(pthfile_mutex, ( TickType_t ) 50 )) // 2 sec wait
			{
			f = fopen(file_name, "a");
			if(f)
				{
				sprintf(fbuf, "%s %s\n", strtime, bufdata);
				fputs(fbuf, f);
				fclose(f);
				ret =  ESP_OK;
				}
			else
				{
				ESP_LOGI(TAG, "Cannot open %s for writing. errno = %d", file_name, errno);
				ret = ESP_FAIL;
				}
			xSemaphoreGive(pthfile_mutex);
			}
		else
			ret = ESP_FAIL;
		}
	return ret;
	}
#endif
*/
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
