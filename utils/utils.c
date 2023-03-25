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
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "mqtt_client.h"
#include "common_defines.h"
#include "mqtt_ctrl.h"
#include "external_defs.h"
#include "tcp_log.h"
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
#include "adc_op.h"
#include "pumpop.h"
#endif

const char *TAG = "SPIFFS_RW";

int my_log_vprintf(const char *fmt, va_list arguments)
	{
	if(console_state == CONSOLE_ON)
		return vprintf(fmt, arguments);
	else if(console_state == CONSOLE_TCP)
		{
		char buf[1024];
		vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
		return tcp_log_message(buf);
		}
	return 0;
	}

void my_printf(char *format, ...)
	{
	char buf[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( buf, sizeof(buf) - 1, format, args );
	va_end( args );
	if(console_state == CONSOLE_ON)
		{
		puts(buf);
		}
	else if(console_state == CONSOLE_TCP)
		{
		//puts(buf);
		tcp_log_message(buf);
		}
	}
int rw_params(int rw, int param_type, void * param_val)
	{
	char buf[64];
	esp_err_t ret;

    size_t total = 0, used = 0;
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
			else
				ESP_LOGI(TAG, "SPIFFS_check() successful");
			}
    	}
    struct stat st;
    ret = ESP_FAIL;
    if(rw == PARAM_READ)
    	{
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
    	if(param_type == PARAM_V_OFFSET)
    		{
			if (stat(BASE_PATH"/"OFFSET_FILE, &st) != 0)
				{
				// file does no exists
				((psensor_offset_t *)param_val)->v_offset = 0;
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"OFFSET_FILE, "r");
				if (f != NULL)
					{
					((psensor_offset_t *)param_val)->v_offset = 0xffffff;
					if(fgets(buf, 64, f))
						{
						((psensor_offset_t *)param_val)->v_offset = atoi(buf);
						}
					else
						((psensor_offset_t *)param_val)->v_offset = 0;
					fclose(f);
					ret = ESP_OK;
					}
				else
					{
					ESP_LOGE(TAG, "Failed to open offset file for reading");
					//esp_vfs_spiffs_unregister(conf.partition_label);
					return ret;
					}
				}
			}
		if(param_type == PARAM_LIMITS)
			{
			if (stat(BASE_PATH"/"LIMITS_FILE, &st) != 0)
				{
				// file does no exists
				((pump_limits_t *)param_val)->min_val = ((pump_limits_t *)param_val)->max_val = ((pump_limits_t *)param_val)->faultc = ((pump_limits_t *)param_val)->stdev = 0;
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"LIMITS_FILE, "r");
				if (f != NULL)
					{
					((pump_limits_t *)param_val)->min_val = ((pump_limits_t *)param_val)->max_val = ((pump_limits_t *)param_val)->faultc = ((pump_limits_t *)param_val)->stdev = 0xffffff;
					if(fgets(buf, 64, f))
						{
						((pump_limits_t *)param_val)->min_val = atoi(buf);
						if(fgets(buf, 64, f))
							{
							((pump_limits_t *)param_val)->max_val = atoi(buf);
							if(fgets(buf, 64, f))
								{
								((pump_limits_t *)param_val)->faultc = atoi(buf);
								if(fgets(buf, 64, f))
									{
									((pump_limits_t *)param_val)->stdev = atoi(buf);
									ret = ESP_OK;
									}
								}
							}
						else
							{
							ESP_LOGE(TAG, "max limit not found");
							}
						}
					else
						ESP_LOGI(TAG, "Limits file exists but its empty");
					fclose(f);
					}
				else
					{
					ESP_LOGE(TAG, "Failed to open limits file for reading");
					//esp_vfs_spiffs_unregister(conf.partition_label);
					return ret;
					}
				}
			}
		if(param_type == PARAM_OPERATIONAL)
			{
			if (stat(BASE_PATH"/"OPERATIONAL_FILE, &st) != 0)
				{
				// file does no exists
				*(int *)param_val = PUMP_OFFLINE;
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"OPERATIONAL_FILE, "r");
				if (f != NULL)
					{
					*(int *)param_val = -1;
					if(fgets(buf, 64, f))
						{
						*(int *)param_val = atoi(buf);
						ret = ESP_OK;
						}
					else
						ESP_LOGI(TAG, "Operational file exists but its empty");
					fclose(f);
					}
				else
					{
					ESP_LOGE(TAG, "Failed to open operational file for reading");
					//esp_vfs_spiffs_unregister(conf.partition_label);
					return ret;
					}
				}
			}
#endif
		if(param_type == PARAM_CONSOLE)
    		{
			if (stat(BASE_PATH"/"CONSOLE_FILE, &st) != 0)
				{
				// file does no exists
				console_state = CONSOLE_ON;
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"CONSOLE_FILE, "r");
				if (f != NULL)
					{
					if(fgets(buf, 64, f))
						{
						console_state = atoi(buf);
						}
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

		}
    else if(rw == PARAM_WRITE)
    	{
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
    	if(param_type == PARAM_V_OFFSET)
    		{
    		FILE *f = fopen(BASE_PATH"/"OFFSET_FILE, "w");
			if (f == NULL)
				{
				ESP_LOGE(TAG, "Failed to create offset param file");
				}
			else
				{
				sprintf(buf, "%d\n", ((psensor_offset_t *)param_val)->v_offset);
				if(fputs(buf, f) >= 0)
					ret = ESP_OK;
				fclose(f);
				}
			}
		if(param_type == PARAM_LIMITS)
			{
			FILE *f = fopen(BASE_PATH"/"LIMITS_FILE, "w");
			if (f == NULL)
				{
				ESP_LOGE(TAG, "Failed to create limits param file");
				}
			else
				{
				sprintf(buf, "%d\n%d\n%d\n%d\n", ((pump_limits_t *)param_val)->min_val, ((pump_limits_t *)param_val)->max_val, ((pump_limits_t *)param_val)->faultc, ((pump_limits_t *)param_val)->stdev);
				if(fputs(buf, f) >= 0)
					ret = ESP_OK;
				fclose(f);
				}

			}
		if(param_type == PARAM_OPERATIONAL)
			{
			FILE *f = fopen(BASE_PATH"/"OPERATIONAL_FILE, "w");
			if (f == NULL)
				{
				ESP_LOGE(TAG, "Failed to create operational file");
				}
			else
				{
				sprintf(buf, "%d\n", *(int *)param_val);
				if(fputs(buf, f) >= 0)
					ret = ESP_OK;
				fclose(f);
				}
			}
#endif
		if(param_type == PARAM_CONSOLE)
    		{
    		FILE *f = fopen(BASE_PATH"/"CONSOLE_FILE, "w");
			if (f == NULL)
				{
				ESP_LOGE(TAG, "Failed to create console param file");
				}
			else
				{
				sprintf(buf, "%d\n", *(int *)param_val);
				if(fputs(buf, f) >= 0)
					ret = ESP_OK;
				fclose(f);
				}
			}

    	}
    //esp_vfs_spiffs_unregister(conf.partition_label);
	return ret;
	}

