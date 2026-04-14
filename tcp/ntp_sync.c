/*
 * ntp_sync.c
 *
 *  Created on: Sep 19, 2022
 *      Author: viorel_serbu
 */

#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "ntp_sync.h"

int tsync;

void time_sync_notification_cb(struct timeval *tv)
	{
    ESP_LOGI("NTP sync", "Notification of a time synchronization event");
    tsync = 1;
	}


void sync_NTP_time(void)
	{
	tsync = 0;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    config.start = false;
    
    esp_netif_sntp_init(&config);
    sntp_set_sync_interval(3600000);
    esp_netif_sntp_start();
    /*
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
    	{
    	ESP_LOGI("NTP sync", "Failed to update system time within 10s timeout. Restart...");
    	//esp_restart();
    	}
    */
	}

