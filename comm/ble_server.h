/*
 * ble_server.h
 *
 *  Created on: May 25, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_COMM_BLE_SERVER_H_
#define ESP32_COMMON_COMM_BLE_SERVER_H_

#define PROFILE_NUM					1
#define PROFILE_A_APP_ID 			0			// used by main service
#define PROFILE_B_APP_ID 			1			//used by device information service

#define GATTS_SERVICE_UUID_A   		0x8f00
#define GATTS_NUM_CHAR_A			1
#define GATTS_CHAR_UUID_A      		0x8f01		// business communication channel
#define LOG_CHAR_UUID_A      		0x8f02		// log notification channel
#define GATTS_NUM_HANDLE_A     		(3 * GATTS_NUM_CHAR_A + 1)

#define DEVICE_INFORMATION_SERVICE	0x180A
#define GATTS_NUM_CHAR_B			1
#define DIS_SYSID 					0x2A23 		//SystemID 
#define DIS_MODELNO					0x2A24 		//ModelNumberString 
#define DIS_SERIALNO				0x2A25 		//SerialNumberString 
#define DIS_FWREV					0x2A26 		//FirmwareRevisionString  
#define DIS_HWREV					0x2A27 		//HardwareRevisionString  
#define DIS_SWREV					0x2A28 		//SoftwareRevisionString  
#define DIS_MANUFACT				0x2A29 		//ManufacturerNameString
#define GATTS_NUM_HANDLE_B		    (3 * GATTS_NUM_CHAR_B + 1)

#define GATTS_CHAR_VAL_LEN_MAX 0xff
#define PREPARE_BUF_MAX_SIZE 1024

typedef struct
	{
	uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
	} char_info_t;
	

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void bt_notify_task(void *pvParams);
int create_service_info();

#endif /* ESP32_COMMON_COMM_BLE_SERVER_H_ */
