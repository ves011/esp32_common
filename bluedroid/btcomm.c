/*
 * btcomm.c
 *
 *  Created on: Nov 13, 2023
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
//#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
//#include "esp_netif.h"
#include "common_defines.h"
#include "project_specific.h"
#include "process_message.h"
//#include "comm.h"
//#include "external_defs.h"
//#include "tcp_server.h"
#include "ble_server.h"
#include "ble_client.h"
#include "btcomm.h"

//#define DEBUG_BTCOMM

#if (COMM_PROTO & BLE_PROTO) == BLE_PROTO

TaskHandle_t process_message_task_handle = NULL, bt_notify_task_handle = NULL;
QueueHandle_t bt_receive_queue = NULL, bt_send_queue = NULL;

TaskHandle_t client_process_message_task_handle = NULL, bt_client_notify_task_handle = NULL;
QueueHandle_t bt_client_receive_queue = NULL, bt_client_send_queue = NULL;

//static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static int do_bt(int argc, char **argv);
static int add_ble_dev(uint8_t *bda, esp_ble_addr_type_t bda_type, char *dname);
//static void connect2ble_device(const char *remote_bda);

static char* TAG = "BTComm";
//QueueHandle_t msg2remote_queue = NULL;

int btConnected = 0, btEnabled = 0, btRole = 0;
uint32_t scan_d = 10;

ble_dev_list_t *ble_dev = NULL;
int n_scan_results = 0;
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
	
static void send_data(int len, uint16_t flags);

static void process_message_task(void *pvParameters)
	{
	socket_message_t msg;
	while(1)
		{
		if(xQueueReceive(bt_receive_queue, &msg, portMAX_DELAY))
			{
			process_message(msg);
#ifdef DEBUG_BTCOMM			
			ESP_LOGI(TAG, "Message received cmd_id %lu", (unsigned long)msg.cmd_id);
#endif
			}
		}
	}
#if (BT_ROLE & BLE_CLIENT) == BLE_CLIENT
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0xa0,
    .scan_window            = 0x60,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};
static int start_scan(int duration)
	{
	scan_d = duration;
	esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
	if (scan_ret)
		ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
	return scan_ret;
	}
#endif
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
	{
	uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    char dname[64];
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
	        //the unit of the duration is second
			if(ble_dev)
				{
	        	free(ble_dev);
	        	ble_dev = NULL;
	        	}
			n_scan_results = 0;
	        esp_ble_gap_start_scanning(scan_d);
	        break;

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
		            //ESP_LOGI(TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
		            //ESP_LOG_BUFFER_HEX(TAG, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len +  scan_result->scan_rst.scan_rsp_len);
		            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
		                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
					if(adv_name)
						{
						strncpy(dname, (char *)adv_name, adv_name_len);
						dname[adv_name_len] = 0;
						}
					else
						{
						adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
		                                                ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len);
						if(adv_name)
							{
							strncpy(dname, (char *)adv_name, adv_name_len);
							dname[adv_name_len] = 0;
							}
						else                              
	 						strcpy(dname, "NULL");
						}
		            //ESP_LOGI(GATTC_TAG, "searched Device Name Len %d", adv_name_len);
		            //esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
					add_ble_dev(scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, dname);
					//sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x:", scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1]
					//												, scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3]
					//												, scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);
					//ESP_LOGI(TAG, "Device BDA: %s  name: %s", buf, dname);
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
	if(btcomm_init() == ESP_OK)
		btEnabled = 1;
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
#if ((BT_ROLE & BLE_SERVER) == BLE_SERVER) //(btRole == BLE_SERVER)
			{
			bt_receive_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
    		if(!bt_receive_queue)
    			{
				ESP_LOGE(TAG, "Unable to create bt_receive_queue");
				esp_restart();
				}
			bt_send_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
		    if(!bt_send_queue)
		    	{
				ESP_LOGE(TAG, "Unable to create bt_send_queue");
				esp_restart();
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
								"process_msg", 
								8192, 
								NULL, 
								4, 
								&process_message_task_handle) != pdPASS)
				{
				ESP_LOGE(TAG, "Unable to start process message task");
				esp_restart();
				}
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
					ret = esp_ble_gatt_set_local_mtu(500);
					if (ret)
		    			ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
					else
						ESP_LOGI(TAG, "BLE server initialization OK");
		    		}
				}
			}
#endif
#if ((BT_ROLE & BLE_CLIENT) == BLE_CLIENT) 
			{
			bt_client_receive_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
		    if(!bt_client_receive_queue)
		    	{
				ESP_LOGE(TAG, "Unable to create bt_client_receive_queue");
				esp_restart();
				}
			bt_client_send_queue = xQueueCreate(TCP_QUEUE_SIZE, sizeof(socket_message_t));
		    if(!bt_client_send_queue)
		    	{
				ESP_LOGE(TAG, "Unable to create bt_client_send_queue");
				esp_restart();
				}
			if(bt_client_notify_task_handle)
				{
				vTaskDelete(bt_client_notify_task_handle);
				bt_client_notify_task_handle = NULL;
				}
			xTaskCreate(bt_client_notify_task, "BTC_not_task", 8192, NULL, 5, &bt_client_notify_task_handle);
			if(!bt_client_notify_task_handle)
				{
				ESP_LOGE(TAG, "Unable to start bt client notification task");
				esp_restart();
				}
			if(client_process_message_task_handle)
				{
				vTaskDelete(client_process_message_task_handle);
				client_process_message_task_handle = NULL;
				}
			if(xTaskCreate(client_process_message_task, 
								"process_msg_cli", 
								8192, 
								NULL, 
								4, 
								&client_process_message_task_handle) != pdPASS)
				{
				ESP_LOGE(TAG, "Unable to start client process message task");
				esp_restart();
				}
			ret = esp_ble_gattc_register_callback(esp_gattc_cb);
			if (!ret)
				{
				ret = esp_ble_gattc_app_register(CLIENT_PROFILE_A_APP_ID);
				if(!ret)
					{
					ret = esp_ble_gatt_set_local_mtu(500);
					if (ret)
				        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", ret);
					else
						ESP_LOGI(TAG, "BLE client initialization OK");
					}
				else
					ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
				}
			else
				ESP_LOGE(TAG, "gattc callback register error, error code = %x", ret);
		    }
#endif
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
    struct arg_int *profile;
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
		if(btcomm_init() == ESP_OK)
			btEnabled = 1;
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
			if(bt_client_notify_task_handle)
				{
				vTaskDelete(bt_client_notify_task_handle);
				bt_client_notify_task_handle = NULL;
				}
	
			if(client_process_message_task_handle)
				{
				vTaskDelete(client_process_message_task_handle);
				client_process_message_task_handle = NULL;
				}
			btEnabled = 0;
			}
		}

#if ((BT_ROLE & BLE_CLIENT) == BLE_CLIENT)
	else if(strcmp(bt_args.op->sval[0], "scan") == 0)
		{
		int sd = 10;
		if(bt_args.dname->count)
			sd = atoi(bt_args.dname->sval[0]);
		start_scan(sd);	
		}
	else if(strcmp(bt_args.op->sval[0], "list") == 0)
		{
		char buf[32];
		for(int i = 0; i < n_scan_results; i++)
			{
			sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x:", ble_dev[i].dev_bda[0], ble_dev[i].dev_bda[1], ble_dev[i].dev_bda[2], ble_dev[i].dev_bda[3], 
															ble_dev[i].dev_bda[4], ble_dev[i].dev_bda[5] );
			ESP_LOGI(TAG, "%4d  Device BDA: %s  name: %s", i, buf, ble_dev[i].dnamae);				
			}
		}
	else if(strcmp(bt_args.op->sval[0], "connect") == 0)
		{
		if(bt_args.dname->count)
			{
			if(bt_args.profile->count)
				connect2ble_device(bt_args.dname->sval[0], bt_args.profile->ival[0]);
			else
				ESP_LOGI(TAG, "Profile to be used not provided");
			}
		else
			ESP_LOGI(TAG, "Remote BDA not provided");
		}
	else if(strcmp(bt_args.op->sval[0], "disconnect") == 0)
		{
		
		}
	else if(strcmp(bt_args.op->sval[0], "send") == 0)
		{
		if(bt_args.dname->count)
			{
			if(bt_args.profile->count)
				{
				send_data(atoi(bt_args.dname->sval[0]), bt_args.profile->ival[0]);
				}
			}
		}
#endif
#if ((BT_ROLE & BLE_SERVER) == BLE_SERVER)
	else if(strcmp(bt_args.op->sval[0], "notify") == 0)
		{
		send_indication();
		}	
#endif		
	return ESP_OK;
	}

void register_bt()
	{
	btConnected = btEnabled = 0, btRole = 0;
	bt_args.op = arg_str1(NULL, NULL, "<op>", "start | stop | connect | disconnect | scan | send");
    bt_args.dname = arg_str0(NULL, NULL, "<device name> | <packet to send> | <server | client>", "device name to connect or packet data to send");
    bt_args.profile = arg_int0(NULL, NULL, "<0...#>", "profile to be used by connection");
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
    
	}
int add_ble_dev(uint8_t *bda, esp_ble_addr_type_t bda_type, char *dname)
	{
	int i, ret = ESP_FAIL;
	for(i = 0; i < n_scan_results; i++)
		{
		if(memcmp(ble_dev[i].dev_bda, bda, 6) == 0)
			break;
		}
	if(i == n_scan_results)
		{
		ble_dev = realloc(ble_dev, (n_scan_results + 1) * sizeof(ble_dev_list_t));
		if(ble_dev)
			{
			memcpy(ble_dev[n_scan_results].dev_bda, bda, 6);
			strncpy(ble_dev[n_scan_results].dnamae, dname, 31);
			ble_dev[n_scan_results].bda_type = bda_type;
			ble_dev[n_scan_results].in_use = 0;
			n_scan_results++;
			ret = ESP_OK;
			}
		}
	else
		ret = ESP_OK;
	return ret;
	}
	
static void send_data(int len, uint16_t flags)
	{
	bt_raw_data_t btrd;
	btrd.ts = esp_timer_get_time();
	btrd.id = BT_RAW_PDU;
	btrd.len = len;
	btrd.flags = flags;
	for(int i = 0; i < len; i++)	
		btrd.pdu[i] = i;
	xQueueSend(bt_client_send_queue, &btrd, portMAX_DELAY);
	}
void set_advp(int maxint, int minint)
	{
	adv_params.adv_int_min = (minint * 1000) / 625;
	adv_params.adv_int_max = (maxint * 1000) / 625;
	}
void get_advp()
	{
	ESP_LOGI(TAG, "Adv params - min int: %d, max int: %d", (adv_params.adv_int_min * 625) / 1000, (adv_params.adv_int_max * 625) / 1000);
	}
#endif
