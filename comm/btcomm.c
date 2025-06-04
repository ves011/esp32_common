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
#include "esp_console.h"
#include "argtable3/argtable3.h"
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
#include "ble_server.h"
#include "btcomm.h"

//#define DEBUG_BTCOMM

#if COMM_PROTO == BLE_PROTO

TaskHandle_t process_message_task_handle = NULL, bt_notify_task_handle = NULL;
QueueHandle_t tcp_receive_queue = NULL, tcp_send_queue = NULL;

//static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static int do_bt(int argc, char **argv);

static char* TAG = "BTComm";
//QueueHandle_t msg2remote_queue = NULL;

int btConnected = 0, btEnabled = 0, btRole = 0;
//char mdata[20];
/*
#define PROFILE_NUM					1
#define PROFILE_A_APP_ID 			0
#define GATTS_SERVICE_UUID_TEST_A   0x8f00
#define GATTS_CHAR_UUID_TEST_A      0x8f01
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_TEST_A     4

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
#define PREPARE_BUF_MAX_SIZE 1024
*/

//#define DEV_NAME			"pump01"
/*
static uint8_t char1_str[] = {0x11,0x22,0x33};
static esp_gatt_char_prop_t a_property = 0;
static esp_attr_value_t gatts_demo_char1_val =
	{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
	};
*/
uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

char btDevName[32];

/*
 * all UUIDs below are in reversed order
 * custom UUIDs
		e26784a2-8208-11ee-b962-0242ac120002
		e2678736-8208-11ee-b962-0242ac120002
 * 128bit BT SIG UUID base for 16bit UUID
		fb349b5f-8000-8000-1000-0000xxxx0000
*/
/*
static uint8_t adv_service_uuid128[16] =
	{
    // LSB <--------------------------------------------------------------------------------> MSB 
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
*/
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
/*	
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static gatts_profile_inst_t gl_profile_tab =
	{
    .gatts_cb = gatts_profile_a_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,       // Not get the gatt_if, so initial is ESP_GATT_IF_NONE 
	};

static prepare_type_env_t prepare_write_env;
*/
/*
void process_message(socket_message_t msg)
	{
	
	}
*/
static void process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	while(1)
		{
		if(xQueueReceive(tcp_receive_queue, &msg, portMAX_DELAY))
			{
			process_message(msg);
#ifdef DEBUG_BTCOMM			
			ESP_LOGI(TAG, "Message received cmd_id %lu", (unsigned long)msg.cmd_id);
#endif
			}
		}
	}


static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
	{
	uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    char buf[64], dname[64];
	 switch (event)
		 {
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
	    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
	        //advertising start complete event to indicate advertising start successfully or failed
	        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
	            ESP_LOGE(TAG, "Advertising start failed %d", param->adv_start_cmpl.status);
	        else
	        	ESP_LOGI(TAG, "Advertising start");
	        break;
	    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
	        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
	            ESP_LOGE(TAG, "Advertising stop failed");
	        else
	            ESP_LOGI(TAG, "Stop adv successfully");
	        break;
	        
		case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
	    	{
	        //the unit of the duration is second
	        uint32_t duration = 10;
	        esp_ble_gap_start_scanning(duration);
	        break;
	        }
	    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
	        //scan start complete event to indicate scan start successfully or failed
	        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
	        	{
	            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
	            break;
	        	}
	        ESP_LOGI(TAG, "scan start success");
		        break;
	    case ESP_GAP_BLE_SCAN_RESULT_EVT: 
	    	{
	        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
	        switch (scan_result->scan_rst.search_evt) 
	        	{
		        case ESP_GAP_SEARCH_INQ_RES_EVT:
		        	//ESP_LOGI(GATTC_TAG, " ");
		            //esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
		            //ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
		            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
		                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
					if(adv_name)
						strncpy(dname, (char *)adv_name, adv_name_len);
					else
	 					strcpy(dname, "NULL");
		            //ESP_LOGI(GATTC_TAG, "searched Device Name Len %d", adv_name_len);
		            //esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
		
		#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
		            if (scan_result->scan_rst.adv_data_len > 0) {
		                ESP_LOGI(GATTC_TAG, "adv data:");
		                esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
		            }
		            if (scan_result->scan_rst.scan_rsp_len > 0) {
		                ESP_LOGI(GATTC_TAG, "scan resp:");
		                esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
		            }
		#endif
					sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1]
																	, scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3]
																	, scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);
					ESP_LOGI(TAG, "Device BDA: %s  name: %s", buf, dname);
/*					
		            if (!connect && strcmp(dname, remote_device_name) == 0)
		            	{
	                    connect = true;
	                    ESP_LOGI(GATTC_TAG, "connect to the remote device.");
	                    esp_ble_gap_stop_scanning();
	                    esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
		                }
*/		            
		            break;
		        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
		        	ESP_LOGI(TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
		            break;
		        default:
		            break;
		        }
	        break;
	    	}
	
	    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
	        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
	        	{
	            ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
	            break;
	        	}
	        ESP_LOGI(TAG, "stop scan successfully");
	        break;
	
	    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
	        ESP_LOGI(TAG, "packet length updated: rx = %d, tx = %d, status = %d",
	                  param->pkt_data_length_cmpl.params.rx_len,
	                  param->pkt_data_length_cmpl.params.tx_len,
	                  param->pkt_data_length_cmpl.status);
	        break;	        
	        
	        
	    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
	         ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
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
void start_bt(int role)
	{
	btRole = role;
	btcomm_init();
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
					ESP_LOGE(TAG, "%s enable bluetooth failed", __func__);
				}
			else
				ESP_LOGE(TAG, "%s init bluetooth failed", __func__);
			}
		else
			ESP_LOGE(TAG, "%s enable controller failed", __func__);
		}
	else
	    ESP_LOGE(TAG, "%s initialize controller failed", __func__);
	    
	ret = esp_ble_gap_register_callback(gap_event_handler);
	if (ret)
		{
		ESP_LOGE(TAG, "gap callback register error, error code = %x", ret);
		return ESP_FAIL;
		}
	    
	if(!ret)
		{
		if(btRole == BLE_SERVER)
			{
			ret = create_service_info();
			ret = esp_ble_gatts_register_callback(gatts_event_handler);
			if (ret)
				ESP_LOGE(TAG, "gatts callback register error, error code = %x", ret);
			else
				{
				ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
				if (!ret)
					{
					ret = esp_ble_gatts_app_register(PROFILE_B_APP_ID);
					if (ret)
        				ESP_LOGE(TAG, "gatts app register error - profile b, error code = %x", ret);
        			}
				else
					ESP_LOGE(TAG, "gatts app register error - profile a, error code = %x", ret);
				
    			if(!ret)
    				{
					esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
					if (local_mtu_ret)
		    			ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
		    		}
				}
			}
		}
	
		
	if(bt_notify_task_handle)
		{
		vTaskDelete(bt_notify_task_handle);
		bt_notify_task_handle = NULL;
		}
	xTaskCreate(bt_notify_task, "BT_not_task", 8192, NULL, 5, &bt_notify_task_handle);
	if(!bt_notify_task_handle)
		{
		ESP_LOGE(TAG, "Unable to start bt notification task");
		esp_restart();
		}
	if(process_message_task_handle)
		{
		vTaskDelete(process_message_task_handle);
		process_message_task_handle = NULL;
		}
	if(xTaskCreate(process_message_task, 
						"process_message", 
						8192, 
						NULL, 
						4, 
						&process_message_task_handle) != pdPASS)
		{
		ESP_LOGE(TAG, "Unable to start process message task task");
		esp_restart();
		}
	//esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
	//if (scan_ret)
	//	ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
	return ret;
	}

static struct
	{
    struct arg_str *op;
    struct arg_str *dname;
    struct arg_end *end;
	} bt_args;
		
static int do_bt(int argc, char **argv)
	{
	if(strcmp(argv[0], "bt"))
		return 1;
	int nerrors = arg_parse(argc, argv, (void **)&bt_args);
	if (nerrors != 0)
		{
		arg_print_errors(stderr, bt_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(strcmp(bt_args.op->sval[0], "start") == 0)
		{
		if(bt_args.dname->count == 0 ||
			(strcmp(bt_args.dname->sval[0], "server") && strcmp(bt_args.dname->sval[0], "client")))
			ESP_LOGI(TAG, "Bluetooth role not specified or invalid role");
		else
			{
			if(strcmp(bt_args.dname->sval[0], "server") == 0)
				btRole = BLE_SERVER;
			else
				btRole = BLE_CLIENT;
			if(btcomm_init() == ESP_OK)
				{
				btEnabled = 1;
				}
			}
		}
	else if(strcmp(bt_args.op->sval[0], "stop") == 0)
		{
		if(btEnabled)
			{
			esp_bluedroid_disable();
			esp_bluedroid_deinit();
			esp_bt_controller_disable();
			esp_bt_controller_deinit();
			if(bt_notify_task_handle)
				{
				vTaskDelete(bt_notify_task_handle);
				bt_notify_task_handle = NULL;
				}
	
			if(process_message_task_handle)
				{
				vTaskDelete(process_message_task_handle);
				process_message_task_handle = NULL;
				}
			btEnabled = 0;
			}
		}
	else if(strcmp(bt_args.op->sval[0], "scan") == 0)
		{
		
		}
	else if(strcmp(bt_args.op->sval[0], "connect") == 0)
		{
		
		}
	else if(strcmp(bt_args.op->sval[0], "disconnect") == 0)
		{
		
		}
	else if(strcmp(bt_args.op->sval[0], "send") == 0)
		{
		
		}
	return ESP_OK;
	}

void register_bt()
	{
	btConnected = btEnabled = 0, btRole = 0;
	bt_args.op = arg_str1(NULL, NULL, "<op>", "start | stop | connect | disconnect | scan | send");
    bt_args.dname = arg_str0(NULL, NULL, "<device name> | <packet to send> | <server | client>", "device name to connect or packet data to send");
    bt_args.end = arg_end(2);
    const esp_console_cmd_t bt_cmd =
    	{
        .command = "bt",
        .help = "bt cmds",
        .hint = NULL,
        .func = &do_bt,
        .argtable = &bt_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&bt_cmd));
    tcp_receive_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_receive_queue)
    	{
		ESP_LOGE(TAG, "Unable to create tcp_receive_queue");
		esp_restart();
		}
	tcp_send_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    if(!tcp_send_queue)
    	{
		ESP_LOGE(TAG, "Unable to create tcp_send_queue");
		esp_restart();
		}
	}
#endif
