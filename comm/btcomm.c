/*
 * btcomm.c
 *
 *  Created on: Nov 13, 2023
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_netif.h"
#include "common_defines.h"
#include "process_message.h"
#include "project_specific.h"
//#include "comm.h"
#include "external_defs.h"
#include "tcp_server.h"
#include "btcomm.h"

//#define DEBUG_BTCOMM

#if COMM_PROTO == BLE_PROTO

TaskHandle_t process_message_task_handle = NULL, send_task_handle = NULL;
QueueHandle_t tcp_receive_queue = NULL, tcp_send_queue = NULL;

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void bt_notify_task(void *pvParams);

static char* GATTS_TAG = "BTComm";
//QueueHandle_t msg2remote_queue = NULL;
static TaskHandle_t bt_notify_task_handle;
int btConnected;
char mdata[20];

#define PROFILE_NUM					1
#define PROFILE_A_APP_ID 			0
#define GATTS_SERVICE_UUID_TEST_A   0x8f00
#define GATTS_CHAR_UUID_TEST_A      0x8f01
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_TEST_A     4

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
#define PREPARE_BUF_MAX_SIZE 1024

//#define DEV_NAME			"pump01"

static uint8_t char1_str[] = {0x11,0x22,0x33};
static esp_gatt_char_prop_t a_property = 0;
static esp_attr_value_t gatts_demo_char1_val =
	{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
	};

static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static char btDevName[32];

//#define CONFIG_SET_RAW_ADV_DATA

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};
static uint8_t raw_scan_rsp_data[] = {
        0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f
};
#else
//e26784a2-8208-11ee-b962-0242ac120002
//e2678736-8208-11ee-b962-0242ac120002
//fb349b5f-8000-8000-1000

static uint8_t adv_service_uuid128[16] =
	{
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
	//0xe2, 0x67, 0x84, 0xa2, 0x82, 0x08, 0x11, 0xee, 0xb9, 0x62, 0x02, 0x42, 0x00, 0xee, 0xee, 0x00,
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x8f, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
	//0xe2, 0x67, 0x87, 0x36, 0x82, 0x08, 0x11, 0xee, 0xb9, 0x62, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02,
	//0xe2, 0x67, 0x84, 0xa2, 0x82, 0x08, 0x11, 0xee, 0xb9, 0x62, 0x02, 0x42, 0xFF, 0x00, 0x00, 0x00,
	};
static esp_ble_adv_data_t adv_data =
	{
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x023, //0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
	};
// scan response data
static esp_ble_adv_data_t scan_rsp_data =
	{
    .set_scan_rsp = true,
    .include_name = false,
    .include_txpower = true,
    //.min_interval = 0x0006,
    //.max_interval = 0x0010,
    //.appearance = 0x023,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0, //sizeof(adv_service_uuid128),
    .p_service_uuid = NULL, //adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
	};
#endif
static esp_ble_adv_params_t adv_params =
	{
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};
static gatts_profile_inst_t gl_profile_tab =
	{
    .gatts_cb = gatts_profile_a_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	};

static prepare_type_env_t prepare_write_env;

static void process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	while(1)
		{
		if(xQueueReceive(tcp_receive_queue, &msg, portMAX_DELAY))
			{
			process_message(msg);
#ifdef DEBUG_BTCOMM			
			ESP_LOGI(GATTS_TAG, "Message received cmd_id %lu", (unsigned long)msg.cmd_id);
#endif
			}
		}
	}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
	{
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT)
		{
		if (param->reg.status == ESP_GATT_OK)
			gl_profile_tab.gatts_if = gatts_if;
		else
			{
			ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d\n",
					param->reg.app_id,
					param->reg.status);
			return;
			}
		}

	/* If the gatts_if equal to profile A, call profile A cb handler,
	 * so here call each profile's callback */
	do
		{
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++)
			{
			if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
					gatts_if == gl_profile_tab.gatts_if)
				{
				if (gl_profile_tab.gatts_cb)
					{
					gl_profile_tab.gatts_cb(event, gatts_if, param);
					}
				}
			}
		} while (0);
	}
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
	{
	 switch (event)
		 {
#ifdef CONFIG_SET_RAW_ADV_DATA
		 case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
			adv_config_done &= (~adv_config_flag);
			if (adv_config_done==0)
				esp_ble_gap_start_advertising(&adv_params);
			break;
		case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
			adv_config_done &= (~scan_rsp_config_flag);
			if (adv_config_done==0)
				esp_ble_gap_start_advertising(&adv_params);
			break;
#else
		 case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
			 adv_config_done &= (~adv_config_flag);
			 if (adv_config_done == 0)
				 esp_ble_gap_start_advertising(&adv_params);
			 break;
		 case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
			 adv_config_done &= (~scan_rsp_config_flag);
			 if (adv_config_done == 0)
				 esp_ble_gap_start_advertising(&adv_params);
			 break;
#endif
	    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
	        //advertising start complete event to indicate advertising start successfully or failed
	        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
	            ESP_LOGE(GATTS_TAG, "Advertising start failed\n");
	        else
	        	ESP_LOGI(GATTS_TAG, "Advertising start\n");
	        break;
	    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
	        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
	            ESP_LOGE(GATTS_TAG, "Advertising stop failed\n");
	        else
	            ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
	        break;
	    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
	         ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
	                  param->update_conn_params.status,
	                  param->update_conn_params.min_int,
	                  param->update_conn_params.max_int,
	                  param->update_conn_params.conn_int,
	                  param->update_conn_params.latency,
	                  param->update_conn_params.timeout);
	        break;
	    default:
	        break;
	    }

	}
void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
	{
    esp_gatt_status_t status = ESP_GATT_OK;
#ifdef DEBUG_BTCOMM    
    ESP_LOGI(GATTS_TAG, "example_write_event_env() %d %d", param->write.need_rsp, param->write.is_prep);
#endif
    if (param->write.need_rsp)
    	{
        if (param->write.is_prep)
        	{
            if (prepare_write_env->prepare_buf == NULL)
            	{
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE*sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL)
                	{
                    ESP_LOGE(GATTS_TAG, "Gatt_server prep no mem\n");
                    status = ESP_GATT_NO_RESOURCES;
                	}
            	}
            else
            	{
                if(param->write.offset > PREPARE_BUF_MAX_SIZE)
                    status = ESP_GATT_INVALID_OFFSET;
                else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE)
                    status = ESP_GATT_INVALID_ATTR_LEN;
                }
            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK)
               ESP_LOGE(GATTS_TAG, "Send response error\n");
            free(gatt_rsp);
            if (status != ESP_GATT_OK)
                return;
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;

        	}
        else
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }


void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
	{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
        esp_log_buffer_hex(GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    else
        ESP_LOGI(GATTS_TAG,"ESP_GATT_PREP_WRITE_CANCEL");

    if (prepare_write_env->prepare_buf)
    	{
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    	}
    prepare_write_env->prepare_len = 0;
	}

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
	{
	socket_message_t msg;
	switch (event)
		{
	    case ESP_GATTS_REG_EVT:
	         ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
	         gl_profile_tab.service_id.is_primary = true;
	         gl_profile_tab.service_id.id.inst_id = 0x00;
	         gl_profile_tab.service_id.id.uuid.len = ESP_UUID_LEN_16;
	         gl_profile_tab.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;

	         esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(btDevName);
	         if (set_dev_name_ret)
	        	 ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
#ifdef CONFIG_SET_RAW_ADV_DATA
	         esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
			 if (raw_adv_ret)
				ESP_LOGE(GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
			 else
				 {
				 adv_config_done |= adv_config_flag;
				 esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
				 if (raw_scan_ret)
					ESP_LOGE(GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
				 }
			adv_config_done |= scan_rsp_config_flag;
#else
	         //config adv data
			 esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
			 if (ret)
				 ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
			 adv_config_done |= adv_config_flag;
			 //config scan response data
			 if(strlen(mdata))
				 {
				 uint8_t svdata[22];//[sizeof(mdata) + 2];
				 svdata[0] = 0x8f; svdata[1] = 0x00;
				 memcpy(svdata + 2, mdata, strlen(mdata));
				 scan_rsp_data.service_data_len = strlen(mdata) + 2;
				 scan_rsp_data.p_service_data = (uint8_t *)svdata;
				 }
			 ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
			 if (ret)
				 ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);

			 adv_config_done |= scan_rsp_config_flag;
#endif
	        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab.service_id, GATTS_NUM_HANDLE_TEST_A);
	        break;
	    case ESP_GATTS_READ_EVT:
	    	ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
			esp_gatt_rsp_t rsp;
			memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
			rsp.attr_value.handle = param->read.handle;
			rsp.attr_value.len = 4;
			rsp.attr_value.value[0] = 0xde;
			rsp.attr_value.value[1] = 0xed;
			rsp.attr_value.value[2] = 0xbe;
			rsp.attr_value.value[3] = 0xef;
			esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
			break;
	    case ESP_GATTS_WRITE_EVT:
#ifdef DEBUG_BTCOMM
			ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d len %d", param->write.conn_id, param->write.trans_id, param->write.handle, param->write.len);
#endif			
			if (!param->write.is_prep)
				{
				//ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
				if(param->write.handle == gl_profile_tab.char_handle && param->write.len == sizeof(socket_message_t))
					{
					//esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
					msg.cmd_id = ((socket_message_t *)param->write.value)->cmd_id;
					msg.ts = ((socket_message_t *)param->write.value)->ts;
					memcpy(msg.p.u8params, ((socket_message_t *)param->write.value)->p.u8params, param->write.len - 16);
#ifdef DEBUG_BTCOMM					
					ESP_LOGI(GATTS_TAG, "socket_message_t ts:%llu, cmd_id: %lu", msg.ts, (unsigned long)msg.cmd_id);
#endif
					xQueueSend(tcp_receive_queue, &msg, portMAX_DELAY);
					}
				
				if (gl_profile_tab.descr_handle == param->write.handle && param->write.len == 2)
					{
					uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
					if (descr_value == 0x0001)
						{
						if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
							{
							ESP_LOGI(GATTS_TAG, "notify enable");
							uint8_t notify_data[2];
							notify_data[0] = 0;
							notify_data[1] = NOTIFICATION_OK;
							//the size of notify_data[] need less than MTU size
							int si = esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab.char_handle,
													sizeof(notify_data), notify_data, false);
							ESP_LOGI(GATTS_TAG, "notification sent: %d, descr handle: %x", si, gl_profile_tab.char_handle);
							}
						}
					else if (descr_value == 0x0002)
						{
						if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
							{
							ESP_LOGI(GATTS_TAG, "indicate enable");
							uint8_t notify_data[2];
							notify_data[0] = 0;
							notify_data[1] = INDICATION_OK;
							//the size of indicate_data[] need less than MTU size
							esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab.char_handle,
													sizeof(notify_data), notify_data, true);
							}
						}
					else if (descr_value == 0x0000)
						ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
					else
						{
						ESP_LOGE(GATTS_TAG, "unknown descr value");
						esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
						}
					}
					
				}
			example_write_event_env(gatts_if, &prepare_write_env, param);
			break;
		case ESP_GATTS_EXEC_WRITE_EVT:
			ESP_LOGI(GATTS_TAG,"ESP_GATTS_EXEC_WRITE_EVT");
			esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
			example_exec_write_event_env(&prepare_write_env, param);
			break;
		case ESP_GATTS_MTU_EVT:
			ESP_LOGI(GATTS_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
			break;
		case ESP_GATTS_UNREG_EVT:
			break;
		case ESP_GATTS_CREATE_EVT:
			ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
			gl_profile_tab.service_handle = param->create.service_handle;
			gl_profile_tab.char_uuid.len = ESP_UUID_LEN_16;
			gl_profile_tab.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;

			esp_ble_gatts_start_service(gl_profile_tab.service_handle);
			a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
			esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab.service_handle, &gl_profile_tab.char_uuid,
															ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
															a_property,
															&gatts_demo_char1_val, NULL);
			if (add_char_ret)
				ESP_LOGE(GATTS_TAG, "add char failed, error code =%x",add_char_ret);
			break;
		case ESP_GATTS_ADD_INCL_SRVC_EVT:
			break;
		case ESP_GATTS_ADD_CHAR_EVT:
			uint16_t length = 0;
			const uint8_t *prf_char;

			ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
					param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
			gl_profile_tab.char_handle = param->add_char.attr_handle;
			gl_profile_tab.descr_uuid.len = ESP_UUID_LEN_16;
			gl_profile_tab.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
			esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
			if (get_attr_ret == ESP_FAIL)
				ESP_LOGE(GATTS_TAG, "ILLEGAL HANDLE");

			ESP_LOGI(GATTS_TAG, "the gatts demo char length = %x\n", length);
			for(int i = 0; i < length; i++)
				ESP_LOGI(GATTS_TAG, "prf_char[%x] =%x\n",i,prf_char[i]);
			esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab.service_handle, &gl_profile_tab.descr_uuid,
																	ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
			if (add_descr_ret)
				ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);
			break;
		case ESP_GATTS_ADD_CHAR_DESCR_EVT:
			gl_profile_tab.descr_handle = param->add_char_descr.attr_handle;
			ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
					 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
			break;
		case ESP_GATTS_DELETE_EVT:
			break;
		case ESP_GATTS_START_EVT:
			ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
					 param->start.status, param->start.service_handle);
			break;
		case ESP_GATTS_STOP_EVT:
			break;
		case ESP_GATTS_CONNECT_EVT:
			esp_ble_conn_update_params_t conn_params = {0};
			memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
			/* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
			conn_params.latency = 0;
			conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
			conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
			conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
			ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
					 param->connect.conn_id,
					 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
					 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
			gl_profile_tab.conn_id = param->connect.conn_id;
			//start sent the update connection parameters to the peer device.
			esp_ble_gap_update_conn_params(&conn_params);
			btConnected = 1;
			break;
		case ESP_GATTS_DISCONNECT_EVT:
			ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
			esp_ble_gap_start_advertising(&adv_params);
			btConnected = 0;
			break;
		case ESP_GATTS_CONF_EVT:
			//ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
			if (param->conf.status != ESP_GATT_OK)
				esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
			break;
		case ESP_GATTS_OPEN_EVT:
		case ESP_GATTS_CANCEL_OPEN_EVT:
		case ESP_GATTS_CLOSE_EVT:
		case ESP_GATTS_LISTEN_EVT:
		case ESP_GATTS_CONGEST_EVT:
		default:
			break;
		}
	}


int btcomm_init()
	{
	int ret = ESP_OK;
	btConnected = 0;
	sprintf(btDevName, "%s%02d", DEV_NAME, CTRL_DEV_ID);
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init(&bt_cfg);
	if (!ret)
		{
		ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
		if(!ret)
			{
			ret = esp_bluedroid_init();
			if(!ret)
				{
				ret = esp_bluedroid_enable();
				if (ret)
					ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed\n", __func__);
				}
			else
				ESP_LOGE(GATTS_TAG, "%s init bluetooth failed\n", __func__);
			}
		else
			ESP_LOGE(GATTS_TAG, "%s enable controller failed\n", __func__);
		}
	else
	    ESP_LOGE(GATTS_TAG, "%s initialize controller failed\n", __func__);
	if(!ret)
		{
		ret = esp_ble_gatts_register_callback(gatts_event_handler);
		if (!ret)
			{
			ret = esp_ble_gap_register_callback(gap_event_handler);
			if (ret)
				ESP_LOGE(GATTS_TAG, "gap callback register error, error code = %x", ret);
			}
		else
			ESP_LOGE(GATTS_TAG, "gatts callback register error, error code = %x", ret);
		}
	if(!ret)
		{
		ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
		if (ret)
			ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
		esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
		if (local_mtu_ret)
		    ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
		}
	tcp_receive_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_receive_queue)
    	{
		ESP_LOGE(GATTS_TAG, "Unable to create tcp_receive_queue");
		esp_restart();
		}
	tcp_send_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_send_queue)
    	{
		ESP_LOGE(GATTS_TAG, "Unable to create tcp_send_queue");
		esp_restart();
		}

	xTaskCreate(bt_notify_task, "BT_not_task", 8192, NULL, 5, &bt_notify_task_handle);
	if(!bt_notify_task_handle)
		{
		ESP_LOGE(GATTS_TAG, "Unable to start bt notification task");
		esp_restart();
		}
	if(xTaskCreate(process_message_task, 
						"process_message", 
						8192, 
						NULL, 
						4, 
						&process_message_task_handle) != pdPASS)
		{
		ESP_LOGE(GATTS_TAG, "Unable to start process message task task");
		esp_restart();
		}
	return ret;
	}

static void bt_notify_task(void *pvParams)
	{
	socket_message_t qmsg;
	int ret;
	while(1)
		{
		if(xQueueReceive(tcp_send_queue, &qmsg, portMAX_DELAY))
			{
			if(btConnected)
				{
				ret = esp_ble_gatts_send_indicate(gl_profile_tab.gatts_if, gl_profile_tab.conn_id, gl_profile_tab.char_handle,
																	sizeof(socket_message_t), (uint8_t *)&qmsg, true);
				vTaskDelay(pdMS_TO_TICKS(20));
#ifndef DEBUG_BTCOMM	
				if(ret)
#endif
					ESP_LOGI(GATTS_TAG, "notification sent: %u / %x", (unsigned int)qmsg.cmd_id, ret);
				//if(qmsg.cmd_id == PUMP_ERR)
				//	ESP_LOGI(GATTS_TAG, "notification sent: %u / %x", (unsigned int)qmsg.cmd_id, ret);
				}
			}
		}
	}
#endif
