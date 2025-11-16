/*
 * ble_client1p.c
 *
 *  Created on: Jun 26, 2025
 *      Author: viorel_serbu
 */
/*
 * 	1p stands for one profile
 *	this implementation supports 1 profile with one characteristic
*/
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "common_defines.h"
#include "esp_bt_defs.h"
#include "esp_log_buffer.h"
//#include "nvs.h"
//#include "nvs_flash.h"

//#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
//#include "esp_bt_main.h"
//#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_timer.h"
//#include "freertos/FreeRTOS.h"
#include "ble_client.h"
#include "btcomm.h"
#include "portmacro.h"

#define GATTC_TAG "GATTC_op"
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

#define REMOTE_SERVICE_UUID        0x8e00
#define REMOTE_NOTIFY_CHAR_UUID    0x8e01
#define INVALID_HANDLE   0

//static const char remote_device_name[] = "ESP32bt";
static bool connected    = false;
//static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_SERVICE_UUID,},
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};


static gattc_profile_inst_t gl_profile_tab[CLIENT_PROFILE_NUM] =
	{
	[CLIENT_PROFILE_A_APP_ID] = 
		{
		.gattc_cb = gattc_profile_event_handler, 
		.gattc_if = ESP_GATT_IF_NONE,				
		},       
	};
	/*
	[CLIENT_PROFILE_B_APP_ID] = 
		{
		.gattc_cb = gatts_profile_b_event_handler, 
		.gattc_if = ESP_GATT_IF_NONE,
		},
	};
	*/
	
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0xa0,
    .scan_window            = 0x60,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

int start_scan(int duration)
	{
	scan_d = duration;
	esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
	if (scan_ret)
		ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
	return scan_ret;
	}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
	{
	struct message
		{
		uint64_t ts;
		uint32_t cmd;
		uint8_t params[8];
		};
	//struct message msg;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) 
    	{
	    case ESP_GATTC_REG_EVT:
	        ESP_LOGI(GATTC_TAG, "REG_EVT");
	        //esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
	        //if (scan_ret)
	        //    ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
	        break;
	    case ESP_GATTC_CONNECT_EVT:
	        ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
	        gl_profile_tab[CLIENT_PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
	        memcpy(gl_profile_tab[CLIENT_PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
	        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
	        ESP_LOG_BUFFER_HEX(GATTC_TAG, gl_profile_tab[CLIENT_PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
	        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
	        if (mtu_ret)
	            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
	        break;
	    case ESP_GATTC_OPEN_EVT:
			if (param->open.status != ESP_GATT_OK)
				{
	            ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
	            
	            }
			else
				{
	        	ESP_LOGI(GATTC_TAG, "open success");
				//esp_ble_gattc_search_service(gattc_if, param->open.conn_id, &remote_filter_service_uuid);
				}
	        break;
	    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
			if (param->dis_srvc_cmpl.status != ESP_GATT_OK)
	            ESP_LOGE(GATTC_TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
			else
				{
	        	ESP_LOGI(GATTC_TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
	        	esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid);
	        	}
	        break;
	    case ESP_GATTC_CFG_MTU_EVT:
	    	ESP_LOGI(GATTC_TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
	        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
	        if (param->cfg_mtu.status != ESP_GATT_OK)
	            ESP_LOGE(GATTC_TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
	        break;
	    case ESP_GATTC_SEARCH_RES_EVT:
	        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
	        ESP_LOGI(GATTC_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
	        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) 
	        	{
	            ESP_LOGI(GATTC_TAG, "service found");
	            connected = true;
	            gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
	            gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
	            ESP_LOGI(GATTC_TAG, "UUID16: %x / %d %d", p_data->search_res.srvc_id.uuid.uuid.uuid16, p_data->search_res.start_handle, p_data->search_res.end_handle);
	        	}
	        break;
	    case ESP_GATTC_SEARCH_CMPL_EVT:
	    	ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
	        if (p_data->search_cmpl.status != ESP_GATT_OK)
	        	{
	            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
	            break;
	        	}
	        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) 
	            ESP_LOGI(GATTC_TAG, "Get service information from remote device");
	        else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) 
	            ESP_LOGI(GATTC_TAG, "Get service information from flash");
	        else 
	            ESP_LOGI(GATTC_TAG, "unknown service source");
	
	        if (connected)
	        	{
	            uint16_t count = 0;
	            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
	                                                                     p_data->search_cmpl.conn_id,
	                                                                     ESP_GATT_DB_CHARACTERISTIC,
	                                                                     gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_start_handle,
	                                                                     gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_end_handle,
	                                                                     INVALID_HANDLE,
	                                                                     &count);
	            if (status != ESP_GATT_OK)
	            	{
	                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
	                break;
	            	}
	
	            if (count > 0)
	            	{
	                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
	                if (char_elem_result)
	                	{
	                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
	                                                             p_data->search_cmpl.conn_id,
	                                                             gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_start_handle,
	                                                             gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_end_handle,
	                                                             remote_filter_char_uuid,
	                                                             char_elem_result,
	                                                             &count);
	                    if (status != ESP_GATT_OK)
	                    	{
	                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
	                        free(char_elem_result);
	                        char_elem_result = NULL;
	                        break;
	                    	}
	
	                    // only one char support
	                    // if more are needed to update gattc_profile_inst_t structure
	                    ESP_LOGI(GATTC_TAG, "%d Characteristics found (only 1 handled)", count);
	                    for(int i = 0; i < count && i < 1; i++)
	                    	{
							ESP_LOGI(GATTC_TAG, "Characteristic: UUID: %x, props: %x", char_elem_result[i].uuid.uuid.uuid16, char_elem_result[i].properties);
							if(char_elem_result[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
	                    		{
	                        	gl_profile_tab[CLIENT_PROFILE_A_APP_ID].char_handle = char_elem_result[i].char_handle;
	                        	esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[CLIENT_PROFILE_A_APP_ID].remote_bda, char_elem_result[i].char_handle);
		                  		}
		                  	}
		                /* free char_elem_result */
		                free(char_elem_result);
	                	}
	                else 
	                	{
						ESP_LOGE(GATTC_TAG, "gattc no mem");
						}
	            	}
	            else
	                ESP_LOGE(GATTC_TAG, "no char found");
	        	}
	         break;
	    case ESP_GATTC_REG_FOR_NOTIFY_EVT: 
	    	{
	        ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
	        if (p_data->reg_for_notify.status != ESP_GATT_OK)
	        	{
	            ESP_LOGE(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
	        	}
	        else
	        	{
	            uint16_t count = 0;
	            uint16_t notify_en = 1;
	            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
	                                                                         gl_profile_tab[CLIENT_PROFILE_A_APP_ID].conn_id,
	                                                                         ESP_GATT_DB_DESCRIPTOR,
	                                                                         gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_start_handle,
	                                                                         gl_profile_tab[CLIENT_PROFILE_A_APP_ID].service_end_handle,
	                                                                         gl_profile_tab[CLIENT_PROFILE_A_APP_ID].char_handle,
	                                                                         &count);
	            if (ret_status != ESP_GATT_OK)
	            	{
	                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
	                break;
	            	}
	            if (count > 0)
	            	{
	                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
	                if (!descr_elem_result)
	                	{
	                    ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
	                    break;
	                	}
	                else
	                	{
	                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
	                                                                         gl_profile_tab[CLIENT_PROFILE_A_APP_ID].conn_id,
	                                                                         p_data->reg_for_notify.handle,
	                                                                         notify_descr_uuid,
	                                                                         descr_elem_result,
	                                                                         &count);
	                    if (ret_status != ESP_GATT_OK)
	                    	{
	                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
	                        free(descr_elem_result);
	                        descr_elem_result = NULL;
	                        break;
	                    	}
	                    // only one char support
	                    // if more are needed to update gattc_profile_inst_t structure
	                    for(int i = 0; i < count && i < 1; i++)
	                    	{
							if(descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
		                    	{
		                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
		                                                                     gl_profile_tab[CLIENT_PROFILE_A_APP_ID].conn_id,
		                                                                     descr_elem_result[i].handle,
		                                                                     sizeof(notify_en),
		                                                                     (uint8_t *)&notify_en,
		                                                                     ESP_GATT_WRITE_TYPE_RSP,
		                                                                     ESP_GATT_AUTH_REQ_NONE);
		                    	}
		
		                    if (ret_status != ESP_GATT_OK)
		                    	{
		                        ESP_LOGE(GATTC_TAG, "chara %d - esp_ble_gattc_write_char_descr error", i);
		                    	}
		                    }
	
	                    /* free descr_elem_result */
	                    free(descr_elem_result);
	                	}
	            	}
	            else
	            	{
	                ESP_LOGE(GATTC_TAG, "decsr not found");
	            	}
		        }
	        break;
	    	}
	    case ESP_GATTC_NOTIFY_EVT:
	    	{
			socket_message_t *msg;
	        if (p_data->notify.is_notify)
	            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
	        else
	            ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
	        //esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
	        msg = (socket_message_t *)p_data->notify.value;
	        xQueueSend(bt_client_receive_queue, msg, portMAX_DELAY);
	        break;
	        }
	    case ESP_GATTC_WRITE_DESCR_EVT:
	        if (p_data->write.status != ESP_GATT_OK)
	        	{
	            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
	            break;
	        	}
	        ESP_LOGI(GATTC_TAG, "write descr success ");
	        break;
	    case ESP_GATTC_SRVC_CHG_EVT: 
	    	{
	        esp_bd_addr_t bda;
	        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
	        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
	        ESP_LOG_BUFFER_HEX(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
	        break;
	    	}
	    case ESP_GATTC_WRITE_CHAR_EVT:
	        if (p_data->write.status != ESP_GATT_OK)
	        	{
	            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
	            break;
	        	}
	        ESP_LOGI(GATTC_TAG, "write char success ");
	        break;
	    case ESP_GATTC_DISCONNECT_EVT:
	        connected = false;
	        ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
	        break;
	    default:
	        break;
	    }
	}

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
	{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) 
    	{
        if (param->reg.status == ESP_GATT_OK)
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        else 
        	{
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        	}
   		}

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do 
    	{
        int idx;
        for (idx = 0; idx < CLIENT_PROFILE_NUM; idx++) 
        	{
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) 
            	{
                if (gl_profile_tab[idx].gattc_cb)
                    	gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
        	}
    	} while (0);
	}

void connect2ble_device(const char *remote_bda, int profile)
	{
	int i;
	esp_bd_addr_t bda;
	sscanf(remote_bda, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", &bda[0], &bda[1], &bda[2], &bda[3], &bda[4], &bda[5]);
	for(i = 0; i < n_scan_results; i++)
		{
		if(memcmp(ble_dev[i].dev_bda, bda, 6) == 0)
			break;
		}
	if(i < n_scan_results)
		{
    	ESP_LOGI(GATTC_TAG, "connect to the remote device.");
    	esp_ble_gap_stop_scanning();
    	esp_ble_gattc_open(gl_profile_tab[CLIENT_PROFILE_A_APP_ID].gattc_if, ble_dev[i].dev_bda, ble_dev[i].bda_type, true);
		}               
	else
		ESP_LOGI(GATTC_TAG, "MAC address not found in scan list");
	}

void bt_client_notify_task(void *pvParams)
	{
	bt_raw_data_t msg;
	int len;
	while(1)
		{
		if(xQueueReceive(bt_client_send_queue, &msg, portMAX_DELAY))
			{
			len = msg.len + 16;
			int ret = esp_ble_gattc_write_char( gl_profile_tab[CLIENT_PROFILE_A_APP_ID].gattc_if,
	                                  gl_profile_tab[CLIENT_PROFILE_A_APP_ID].conn_id,
	                                  gl_profile_tab[CLIENT_PROFILE_A_APP_ID].char_handle,
	                                  len,
	                                  (uint8_t *)&msg,
	                                  ESP_GATT_WRITE_TYPE_RSP,
	                                  ESP_GATT_AUTH_REQ_NONE);
			ESP_LOGI(GATTC_TAG, "esp_ble_gattc_write_char: %d (%d, %d)", ret, len, msg.flags);
			}
		}
	}
void client_process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	while(1)
		{
		if(xQueueReceive(bt_client_receive_queue, &msg, portMAX_DELAY))
			{
			ESP_LOGI(GATTC_TAG, "Notification received %u", (unsigned int)msg.cmd_id);
			}
		}
	}