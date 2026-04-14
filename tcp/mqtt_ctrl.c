/*
 * mqtt_ctrl.c
 *
 *  Created on: Mar 5, 2023
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "time.h"
#include "esp_netif_types.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "project_specific.h"
#include "common_defines.h"
#include "external_defs.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "utils.h"
#include "mqtt_ctrl.h"

#if ACTIVE_CONTROLLER == WMON_CONTROLLER
	//#include "adc_op.h"
	#include "wmon.h"
#endif
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	#include "adc_op.h"
	#include "pumpop.h"
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
	#include "gateop.h"
#elif ACTIVE_CONTROLLER == WESTA_CONTROLLER
	#include "westaop.h"
#elif ACTIVE_CONTROLLER == NAVIGATOR
	#include "nmea_parser.h"
	#include "mpu6050.h"
	#include "ptst.h"
	#include "hmc5883.h"
#endif
#if ACTIVE_CONTROLLER == WATER_CONTROLLER || ACTIVE_CONTROLLER == WP_CONTROLLER
	#include "waterop.h"
	#include "pumpop.h"
	#include "ad7811.h"
#endif
#if ACTIVE_CONTROLLER == FLOOR_HC
	#include "temps.h"
	#include "actuator.h"
#endif
#if ACTIVE_CONTROLLER == THERMOSTAT
	#include "temps.h"
#endif
#if ACTIVE_CONTROLLER == DS18B20_ALIGNEMNT
	#include "temps.h"
#endif

#define CONFIG_BROKER_URL "mqtts://proxy.gnet:1886"


#if CONFIG_IDF_TARGET_ESP32C3
    #define PROCESS_MESSAGE_BY_HANDLER 1
#elif CONFIG_IDF_TARGET_ESP32S3
    #define PROCESS_MESSAGE_BY_HANDLER 0
    #include "freertos/idf_additions.h"
    static QueueHandle_t mqtt_rx_queue;
    static TaskHandle_t mqtt_rx_task_handle;
    static void mqtt_rx_task(void *arg);
	static int parse_argv(char *payload, char ***argv_out);
#else
    #define PROCESS_MESSAGE_BY_HANDLER 0
#endif

esp_mqtt_client_handle_t client;
static const char *TAG = "MQTTClient";
static esp_netif_ip_info_t dev_ipinfo;


char *TOPIC_STATE, *TOPIC_ERROR, *TOPIC_MONITOR, *TOPIC_CTRL, *TOPIC_LOG, *TOPIC_KA; //TOPIC_CMD[32] 
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	char *TOPIC_STATE_A, *TOPIC_MONITOR_A;
#endif
char *USER_MQTT;

//extern const uint8_t client_cert_pem_start[] asm("_binary_cl_crt_start");
//extern const uint8_t client_cert_pem_end[] asm("_binary_cl_crt_end");
//extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
//extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
//extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_end");

static void create_topics(void);

static void log_error_if_nonzero(const char *message, int error_code)
	{
    if (error_code != 0)
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
	}

void subscribe(char *topic)
	{
	if(!client)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		if(esp_mqtt_client_subscribe(client, topic, 0) == -1)
			{
			ESP_LOGE(TAG, "cannot subscribe to %s", topic);
			//esp_restart();
			}
		else
			ESP_LOGI(TAG, "Client subscribed to %s", topic);
		}
	}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
	{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    	{
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
#ifdef ACTIVE_CONTROLLER
			//subscribe(TOPIC_CMD);
			subscribe(TOPIC_CTRL);
			subscribe(DEVICE_TOPIC_Q);
	#if ACTIVE_CONTROLLER == WATER_CONTROLLER
			subscribe(DEVICE_TOPIC_R);
			subscribe(WATER_PUMP_DESC"/monitor");
			subscribe(WATER_PUMP_DESC"/state");
	#endif
#endif

			get_sta_conf(NULL, &dev_ipinfo);
			publish_MQTT_client_status();
			xEventGroupSetBits(comm_event_group, MQTT_CONNECTED_BIT);
			break;
		case MQTT_EVENT_DISCONNECTED:
			xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			//ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			{
#if PROCESS_MESSAGE_BY_HANDLER == 0			
			mqtt_rx_msg_t msg = {0};
			msg.topic   = strndup(event->topic, event->topic_len);
			if(msg.topic)
				{
				msg.payload = malloc(event->data_len + 2);
				
				if(msg.payload)
					{
					memcpy(msg.payload, event->data, event->data_len);
					msg.payload[event->data_len] = ' ';
					msg.payload[event->data_len + 1] = 0;
					if (xQueueSend(mqtt_rx_queue, &msg, pdMS_TO_TICKS(10)) != pdTRUE) 
						{
        				free(msg.topic);
        				free(msg.payload);
    					}
					}
				else
					free(msg.topic);
				}
			}
#else		
			char *topic, *msg, **argv;
    		int i, argc;	
			topic = msg = NULL;
			topic = malloc(event->topic_len + 1);
			if(!topic)
				{
				ESP_LOGE(TAG, "Cannot allocate memory for local copy of mqtt topic");
				break;
				}
			for(i = 0; i < event->topic_len; i++)
				topic[i] = event->topic[i];
			topic[i] = 0;
			msg = malloc(event->data_len + 2);
			if(!msg)
				{
				ESP_LOGE(TAG, "Cannot allocate memory for local copy of mqtt message");
				free(topic);
				break;
				}
			memcpy(msg, event->data, event->data_len);
			msg[event->data_len] = ' ';
			msg[event->data_len + 1] = 0;

			if(controller_op_registered == 1)
				{
				argc = 0;
				argv = NULL;
				char *pchr = strtok(msg, " \n");
				while(pchr)
					{
					void *tmp = realloc(argv, (argc + 1) * sizeof(char *));
					if(tmp)
						{
						argv = tmp;
						argv[argc] = malloc(strlen(pchr) + 1);
						if(argv[argc])
							{
							strcpy(argv[argc], pchr);
							argc++;
							}
						else
							break;
						}
					else
						break;
					pchr = strtok(NULL, " ");
					}
				if(argc == 0)
					break;
				if(strcmp(topic, TOPIC_CTRL) == 0)
					{
#if ACTIVE_CONTROLLER == WP_CONROLLER
					do_ad(argc, argv);
					do_dvop(argc, argv);
					do_pumpop(argc, argv);
#elif ACTIVE_CONTROLLER == NAVIGATOR
					do_nmea(argc, argv);
					do_mpu(argc, argv);
					do_ptst(argc, argv);
					do_hmc(argc, argv);
#elif ACTIVE_CONTROLLER == PUMP_CONTROLLER
					do_pumpop(argc, argv);
#elif ACTIVE_CONTROLLER == FLOOR_HC
					do_temp(argc, argv);
					do_act(argc, argv);
#elif ACTIVE_CONTROLLER == THERMOSTAT
					do_temp(argc, argv);					
#elif ACTIVE_CONTROLLER == DS18B20_ALIGNEMNT
					do_temp(argc, argv);
#elif ACTIVE_CONTROLLER == WMON_CONTROLLER
					do_wmon(argc, argv);
#endif
					do_system_cmd(argc, argv);
					do_wifi(argc, argv);
					}
				else if(strcmp(topic, DEVICE_TOPIC_Q) == 0)
					{
					if(argc && !strcmp(argv[0], "reqID"))
						publish_MQTT_client_status();
					}
				for(i = 0; i < argc; i++)
					free(argv[i]);
				free(argv);
				}
			if(topic) 
				free(topic);
			if(msg) 
				free(msg);
			}
#endif			
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
				{
				log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
				log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
				log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
				ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
				//esp_restart();
				}
			//TBD
			//xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
    	}
	}

int mqtt_start(void)
	{
	esp_err_t ret = ESP_FAIL;
	xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
	
	int needed = snprintf(NULL, 0, "%s%02d", dev_conf.dev_name, dev_conf.dev_id);
	if (needed < 0) 
		return ESP_FAIL;
	USER_MQTT = malloc((size_t)needed + 1);
	if (!USER_MQTT) 
		return ESP_FAIL;
	snprintf(USER_MQTT, (size_t)needed + 1, "%s%02d", dev_conf.dev_name, dev_conf.dev_id);
	const esp_mqtt_client_config_t mqtt_cfg = {
		    .broker.address.uri = CONFIG_BROKER_URL,
		    //.broker.verification.certificate = (const char *)server_cert_pem_start,
			.broker.verification.certificate = (const char *)nvs_ca_crt,
			.broker.verification.certificate_len = nvs_ca_crt_sz,
		    .credentials = {
		      .client_id = USER_MQTT,
		      .authentication = {
//		        .certificate = (const char *)client_cert_pem_start,
		        .certificate = (const char *)nvs_cl_crt,
		        .certificate_len = nvs_cl_crt_sz,
		        //.key = (const char *)client_key_pem_start,
				.key = (const char *)nvs_cl_key,
				.key_len = nvs_cl_key_sz,
		      	  },
		    	},
			.network.disable_auto_reconnect = false,
			.network.reconnect_timeout_ms = 2000,
			.session.message_retransmit_timeout = 500,
			.session.keepalive = 30,
			.task.stack_size = 8192,
			};
	create_topics();
	if(TOPIC_ERROR && TOPIC_CTRL && TOPIC_MONITOR && TOPIC_STATE && TOPIC_LOG && TOPIC_KA)
		{
#if ACTIVE_CONTROLLER == WP_CONTROLLER
		if(TOPIC_MONITOR_A && TOPIC_STATE_A )
#endif
			{
	    	client = esp_mqtt_client_init(&mqtt_cfg);
	    	if(client)
		    	{
#if PROCESS_MESSAGE_BY_HANDLER == 1					
				if(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK)
					ret = esp_mqtt_client_start(client);
#else
				mqtt_rx_queue = xQueueCreate(8, sizeof(mqtt_rx_msg_t));
				if(mqtt_rx_queue)
					{
					xTaskCreate(mqtt_rx_task, "mqtt_rx", 4096, NULL, USER_TASK_PRIORITY, &mqtt_rx_task_handle);
					if(mqtt_rx_task_handle)
						{
		    			if(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK)
		    				ret = esp_mqtt_client_start(client);
						}
					else
						vQueueDelete(mqtt_rx_queue);
					}
#endif					
		    	}
	    	}
	    }
	if(ret != ESP_OK)
		{
		free(USER_MQTT); 
		free(TOPIC_ERROR);	free(TOPIC_CTRL); free(TOPIC_MONITOR);
		free(TOPIC_STATE); free(TOPIC_KA); free(TOPIC_LOG);
#if ACTIVE_CONTROLLER == WP_CONTROLLER
		free(TOPIC_MONITOR_A); free(TOPIC_STATE_A);	
#endif	    	
		}
    return ret;
    }

void publish_reqID()
	{
	if(client)
		esp_mqtt_client_publish(client, DEVICE_TOPIC_Q, "reqID", 5, 0,0);
	}
void publish_topic(char * topic, char * msg, int qos, int retain)
	{
	char *pmsg;
#if MQTT_PUBLISH == 0
	return;
#endif
	if(!client)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		
		uint64_t tmsec = esp_timer_get_time();
		int needed = snprintf(NULL, 0, "%llu\1%s\1%s", tmsec, USER_MQTT, msg);
		pmsg = malloc(needed + 1);
		if(pmsg)
			{
			snprintf(pmsg, (size_t)needed + 1, "%llu\1%s\1%s", tmsec, USER_MQTT, msg);
			esp_mqtt_client_publish(client, topic, pmsg, strlen(pmsg), qos, retain);
			free(pmsg);
			}
		}
	}

void publish_MQTT_client_status()
	{
	time_t now;
	struct tm timeinfo;
    char msg[200];
    char strtime[50];
    uint64_t tmsec = esp_timer_get_time();
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S\1", &timeinfo);
	snprintf(msg, 199, "%s\1%s\1" IPSTR "\1%d\1%s\1%llu",
			USER_MQTT, DEV_NAME, IP2STR(&dev_ipinfo.ip), CTRL_DEV_ID, strtime, tmsec);
	esp_mqtt_client_publish(client, DEVICE_TOPIC_R, msg, strlen(msg), 0, 0);
	}
void publish_MQTT_client_log(char *message)
	{
	size_t sz = strlen(message);
	if (sz == 0) 
		return;
	if(message[sz - 1] == '\n')
		message[sz - 1] = 0;
	esp_mqtt_client_publish(client, TOPIC_LOG, message, strlen(message), 0, 0);
	}

void create_topics()
	{
	ESP_LOGI(TAG, "USER_MQTT: %s", USER_MQTT);
	TOPIC_STATE = malloc(strlen(USER_MQTT) + strlen("/state") + 1);
	if(TOPIC_STATE)
		{
		strcpy(TOPIC_STATE, USER_MQTT);
		strcat(TOPIC_STATE, "/state");
		}
	TOPIC_ERROR = malloc(strlen(USER_MQTT) + strlen("/error") + 1);
	if(TOPIC_ERROR)
		{
		strcpy(TOPIC_ERROR, USER_MQTT);
		strcat(TOPIC_ERROR, "/error");
		}
	TOPIC_MONITOR = malloc(strlen(USER_MQTT) + strlen("/monitor") + 1);
	if(TOPIC_MONITOR)
		{
		strcpy(TOPIC_MONITOR, USER_MQTT);
		strcat(TOPIC_MONITOR, "/monitor");
		}
	TOPIC_CTRL = malloc(strlen(USER_MQTT) + strlen("/ctrl") + 1);
	if(TOPIC_CTRL)
		{
		strcpy(TOPIC_CTRL, USER_MQTT);
		strcat(TOPIC_CTRL, "/ctrl");
		}
	TOPIC_LOG = malloc(strlen(USER_MQTT) + strlen("/log") + 1);
	if(TOPIC_LOG)
		{
		strcpy(TOPIC_LOG, USER_MQTT);
		strcat(TOPIC_LOG, "/log");
		}
	TOPIC_KA = malloc(strlen(USER_MQTT) + strlen("/ka") + 1);
	if(TOPIC_KA)
		{
		strcpy(TOPIC_KA, USER_MQTT);
		strcat(TOPIC_KA, "/ka");
		}
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	if(TOPIC_STATE)
		{
		TOPIC_STATE_A = malloc(strlen(TOPIC_STATE) + strlen("/w") + 1);
		if(TOPIC_STATE_A)
			{
			strcpy(TOPIC_STATE_A, TOPIC_STATE);
			strcat(TOPIC_STATE_A, "/w");
			}
		}
	if(TOPIC_MONITOR)
		{
		TOPIC_MONITOR_A = malloc(strlen(TOPIC_MONITOR) + strlen("/w") + 1);
		if(TOPIC_MONITOR_A)
			{
			strcpy(TOPIC_MONITOR_A, TOPIC_MONITOR);
			strcat(TOPIC_MONITOR_A, "/w");
			}
		}
#endif
	}
int get_MQTT_connection_state(char *id)
	{
	if(id)
		strcpy(id, USER_MQTT);
	
	if ((xEventGroupGetBits(comm_event_group) & MQTT_CONNECTED_BIT) == 0)
    	return 0;
	return 1;
	}
	
#if !PROCESS_MESSAGE_BY_HANDLER
static int parse_argv(char *payload, char ***argv_out)
	{
    int argc = 0;
    char **argv = NULL;

    char *p = strtok(payload, " \n");
    while (p) 
    	{
        char **tmp = realloc(argv, (argc + 1) * sizeof(char *));
        if (!tmp) 
        	break;
        argv = tmp;
        argv[argc] = strdup(p);
        if (!argv[argc]) 
        	break;
        argc++;
        p = strtok(NULL, " ");
    	}

    *argv_out = argv;
    return argc;
	}


static void mqtt_rx_task(void *arg)
	{
    mqtt_rx_msg_t msg;
	static char **argv;
	int argc;
    for (;;) 
    	{
        if (xQueueReceive(mqtt_rx_queue, &msg, portMAX_DELAY) == pdTRUE) 
        	{
			ESP_LOGI(TAG, "STACK: mqtt_rx free stack 1: %u\n", uxTaskGetStackHighWaterMark(NULL));
			if(controller_op_registered)
				{
				argc = parse_argv(msg.payload, &argv);
			    if(argc)
			    	{
    				if (strcmp(msg.topic, TOPIC_CTRL) == 0) 
    					{
				        do_system_cmd(argc, argv);
				        do_wifi(argc, argv);
				        
#if ACTIVE_CONTROLLER == WP_CONROLLER
				        do_ad(argc, argv);
				        do_dvop(argc, argv);
				        do_pumpop(argc, argv);
#elif ACTIVE_CONTROLLER == NAVIGATOR
				        do_nmea(argc, argv);
				        do_mpu(argc, argv);
				        do_ptst(argc, argv);
				        do_hmc(argc, argv);
#elif ACTIVE_CONTROLLER == PUMP_CONTROLLER
				        do_pumpop(argc, argv);
#elif ACTIVE_CONTROLLER == FLOOR_HC
				        do_temp(argc, argv);
				        do_act(argc, argv);
#elif ACTIVE_CONTROLLER == THERMOSTAT
				        do_temp(argc, argv);
#elif ACTIVE_CONTROLLER == DS18B20_ALIGNEMNT
				        do_temp(argc, argv);
#elif ACTIVE_CONTROLLER == WMON_CONTROLLER
				        do_wmon(argc, argv);
#endif

				    	}
    				else if (strcmp(msg.topic, DEVICE_TOPIC_Q) == 0) 
    					{
        				if (argc && !strcmp(argv[0], "reqID"))
            			publish_MQTT_client_status();
    					}
					for (int i = 0; i < argc; i++)
        				free(argv[i]);
    				free(argv);
    				}
    			}
	        free(msg.topic);
    	    free(msg.payload);
    	    ESP_LOGI(TAG, "STACK: mqtt_rx free stack 2: %u\n", uxTaskGetStackHighWaterMark(NULL));
    	    }
    	}
	}
#endif

