/*
 * ble_client.h
 *
 *  Created on: Jun 26, 2025
 *      Author: viorel_serbu
 */
 #include "esp_gattc_api.h"
#include "inttypes.h"

#ifndef ESP32_COMMON_COMM_BLE_CLIENT_H_
#define ESP32_COMMON_COMM_BLE_CLIENT_H_


#define CLIENT_PROFILE_NUM					1
#define CLIENT_PROFILE_A_APP_ID 			0			// used by main service
#define CLIENT_PROFILE_B_APP_ID 			1			//used by device information service

typedef struct 
	{
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
	} gattc_profile_inst_t;

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
//int start_scan(int duration);
void connect2ble_device(const char *remote_bda, int profile_no);
void bt_client_notify_task(void *pvParams);
void client_process_message_task(void *pvParameters);


#endif /* ESP32_COMMON_COMM_BLE_CLIENT_H_ */
