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
#include "esp_log.h"
#include "errno.h"
#include "esp_spiffs.h"
#include "mqtt_client.h"
#include "sys/stat.h"
#include "common_defines.h"
#include "mqtt_ctrl.h"
#include "external_defs.h"
#include "tcp_log.h"
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER || ACTIVE_CONTROLLER == WP_CONTROLLER
//#include "adc_op.h"
#include "pumpop.h"
#endif
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
	else if(console_state >= CONSOLE_TCP)
		{
		char buf[1024];
		vsnprintf(buf, sizeof(buf) - 1, fmt, arguments);
		//strip special characters
		return tcp_log_message(buf);
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
		{
		puts(buf);
		}
	else if(console_state >= CONSOLE_TCP)
		{
		//puts(buf);
		tcp_log_message(buf);
		}
	}
void my_fputs(char *buf, FILE *f)
	{
	if(console_state == CONSOLE_ON)
		{
		puts(buf);
		}
	else if(console_state >= CONSOLE_TCP)
		{
		//puts(buf);
		tcp_log_message(buf);
		}
	}
int rw_params(int rw, int param_type, void * param_val)
	{
	char buf[64];
	esp_err_t ret;
    struct stat st;
    ret = ESP_FAIL;
    if(rw == PARAM_READ)
    	{
#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
		if(param_type == PARAM_PNORM)
			{
			if (stat(BASE_PATH"/"PNORM_FILE, &st) != 0)
				{
				// file does no exists
				((pnorm_param_t *)param_val)->hmp = DEFAULT_ELEVATION;
				((pnorm_param_t *)param_val)->psl = DEFAULT_PSL;
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"PNORM_FILE, "r");
				if (f != NULL)
					{
					double p = 0, h = 0;
					if(fgets(buf, 64, f))
						{
						sscanf(buf, "%lf %lf", &p, &h);
						}
					((pnorm_param_t *)param_val)->hmp = h;
					((pnorm_param_t *)param_val)->psl = p;
					fclose(f);
					ret = ESP_OK;
					}
				else
					{
					ESP_LOGE(TAG, "Failed to open pnorm file for reading");
						//esp_vfs_spiffs_unregister(conf.partition_label);
					return ret;
					}
				}
			}
#endif
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER || ACTIVE_CONTROLLER == WP_CONTROLLER
    	if(param_type == PARAM_V_OFFSET)
    		{
			if (stat(BASE_PATH"/"OFFSET_FILE, &st) != 0)
				{
				// file does no exists
				((psensor_offset_t *)param_val)->v_offset = 0;
				printf("\nno file: %s", BASE_PATH"/"OFFSET_FILE);
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
			((pump_limits_t *)param_val)->min_val = DEFAULT_PRES_MIN_LIMIT;
			((pump_limits_t *)param_val)->max_val = DEFAULT_PRES_MAX_LIMIT;
			((pump_limits_t *)param_val)->faultc = DEFAULT_PUMP_CURRENT_LIMIT;
			((pump_limits_t *)param_val)->overp_lim = DEFAULT_OVERP_TIME_LIMIT;
			((pump_limits_t *)param_val)->void_run_count = DEFAULT_VOID_RUN_COUNT;
			if (stat(BASE_PATH"/"LIMITS_FILE, &st) != 0)
				{
				// file does no exists
				printf("\nno file: %s", BASE_PATH"/"LIMITS_FILE);
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"LIMITS_FILE, "r");
				if (f != NULL)
					{
					((pump_limits_t *)param_val)->min_val = ((pump_limits_t *)param_val)->max_val = ((pump_limits_t *)param_val)->faultc = 0xffffff;
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
									((pump_limits_t *)param_val)->overp_lim = atoi(buf);
									if(fgets(buf, 64, f))
										{
										((pump_limits_t *)param_val)->void_run_count = atoi(buf);
										ret = ESP_OK;
										}
									}
								}
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
			*(int *)param_val = PUMP_OFFLINE;
			if (stat(BASE_PATH"/"OPERATIONAL_FILE, &st) != 0)
				{
				// file does no exists
				ret = ESP_OK;
				}
			else
				{
				FILE *f = fopen(BASE_PATH"/"OPERATIONAL_FILE, "r");
				if (f != NULL)
					{
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
			//stat(BASE_PATH"/"LIMITS_FILE, &st)
			//if (stat(BASE_PATH"/"OPERATIONAL_FILE, &st) != 0)
			if (stat(BASE_PATH"/"CONSOLE_FILE, &st) != 0)
				{
				// file does no exists
				console_state = CONSOLE_ON;
				printf("\nno file: --%s--", BASE_PATH"/"CONSOLE_FILE);
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
#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
    	if(param_type == PARAM_PNORM)
    		{
    		FILE *f = fopen(BASE_PATH"/"PNORM_FILE, "w");
			if (f == NULL)
				{
				ESP_LOGE(TAG, "Failed to create pnorm param file");
				}
			else
				{
				sprintf(buf, "%.3lf %.3lf\n", ((pnorm_param_t *)param_val)->psl, ((pnorm_param_t *)param_val)->hmp);
				if(fputs(buf, f) >= 0)
					ret = ESP_OK;
				fclose(f);
				}
			}
#endif
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER|| ACTIVE_CONTROLLER == WP_CONTROLLER
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
				sprintf(buf, "%lu\n%lu\n%lu\n%lu\n%lu\n",
						((pump_limits_t *)param_val)->min_val, ((pump_limits_t *)param_val)->max_val, ((pump_limits_t *)param_val)->faultc,
						((pump_limits_t *)param_val)->overp_lim, ((pump_limits_t *)param_val)->void_run_count);
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
			else
				ESP_LOGI(TAG, "SPIFFS_check() successful");
			}
    	}
    return ESP_OK;
    }
