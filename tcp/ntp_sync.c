/*
 * ntp_sync.c
 *
 *  Created on: Sep 19, 2022
 *      Author: viorel_serbu
 */

#include <time.h>
#include <sys/time.h>
#include "common_defines.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "mqtt_ctrl.h"
#include "ntp_sync.h"
#if ACTIVE_CONTROLLER == PUMP_CONTRILLER
	#include "adc_op.h"
	#include "pumpop.h"
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
	#include "gateop.h"
#endif

int tsync;
TaskHandle_t ntp_sync_task_handle;

void time_sync_notification_cb(struct timeval *tv)
	{
    ESP_LOGI("NTP sync", "Notification of a time synchronization event");
    tsync = 1;
	}


static void initialize_sntp(void)
	{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "0.pool.ntp.org");
    esp_sntp_setservername(1, "1.pool.ntp.org");
    esp_sntp_setservername(2, "2.pool.ntp.org");
    esp_sntp_setservername(3, "3.pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
	}

void ntp_sync(void)
	{
	TickType_t refresh = 3600000 / portTICK_PERIOD_MS;
	const int retry_count = 100;
	char strtime[100];
	initialize_sntp();
	while(1)
		{
		time_t now = 0;
    	struct tm timeinfo = { 0 };
    	int retry = 0;
   		tsync = 0;
    	while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) && ! tsync)
    		{
        	ESP_LOGI("NTP sync", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        	vTaskDelay(1000 / portTICK_PERIOD_MS);
    		}
    	if(tsync)
    		{
			time(&now);
			localtime_r(&now, &timeinfo);
			strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S", &timeinfo);
			ESP_LOGI("NTP sync", "local time updated %s", strtime);
			publish_MQTT_client_status();
			}
		else
			ESP_LOGI("NTP sync", "timeout connecting to NTP server");

		vTaskDelay(refresh);
		}

	}
