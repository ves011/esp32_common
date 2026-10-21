/**
 * @file mqtt_ctrl.c
 * @brief MQTT client lifecycle, messaging, and controller dispatch
 *
 * This module implements the MQTT communication layer for the device.
 * It provides:
 *
 *  - Secure MQTT client initialization (TLS, certificates from NVS)
 *  - Dynamic topic creation based on device identity
 *  - Subscription and publish helpers
 *  - Connection state tracking via event groups
 *  - Dispatch of received MQTT commands to controller-specific handlers
 *
 * Architecture notes:
 *  - MQTT connection state is tracked using comm_event_group bits
 *  - Incoming messages are processed either directly in the event handler
 *    or via a dedicated RX task, depending on target/platform
 *  - This module is controller-agnostic and delegates execution via
 *    controller-specific command handlers
 *
 * Threading model:
 *  - MQTT event callbacks run in the ESP-IDF event loop context
 *  - Optional mqtt_rx_task runs as a FreeRTOS task
 *  - No LVGL/UI code is executed here
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "time.h"
#include "esp_netif_types.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "project_specific.h"
#include "common_defines.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "utils.h"
#include "mqtt_ctrl.h"

#if CONFIG_IDF_TARGET_ESP32C3
    #define PROCESS_MESSAGE_BY_HANDLER 1
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
    #define PROCESS_MESSAGE_BY_HANDLER 0
    #include "freertos/idf_additions.h"
    static QueueHandle_t mqtt_rx_queue;
    static TaskHandle_t mqtt_rx_task_handle;
    static void mqtt_rx_task(void *arg);
#else
    #define PROCESS_MESSAGE_BY_HANDLER 0
#endif

static int parse_argv(char *payload, char ***argv_out);
esp_mqtt_client_handle_t mqtt_client_handle;
static const char *TAG = "MQTTClient";
static esp_netif_ip_info_t dev_ipinfo;


static char USER_MQTT[USER_MQTT_SIZE];
char TOPIC_STATE[MQTT_TOPIC_SIZE], TOPIC_ERROR[MQTT_TOPIC_SIZE], TOPIC_MONITOR[MQTT_TOPIC_SIZE]; 
char TOPIC_CTRL[MQTT_TOPIC_SIZE], TOPIC_LOG[MQTT_TOPIC_SIZE], TOPIC_KA[MQTT_TOPIC_SIZE]; 
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	char *TOPIC_STATE_A, *TOPIC_MONITOR_A;
#endif

app_cmd_handler_t app_cmd_handler;
static int connect_fail_count = 0;

static void create_topics(void);

static void log_error_if_nonzero(const char *message, int error_code)
	{
    if (error_code != 0)
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
	}

void subscribe(char *topic)
	{
	if(!mqtt_client_handle)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		if(esp_mqtt_client_subscribe(mqtt_client_handle, topic, 0) == -1)
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
			subscribe(TOPIC_CTRL);
			subscribe(DEVICE_TOPIC_Q);
			get_sta_conf(NULL, &dev_ipinfo);
			publish_MQTT_client_status();
			xEventGroupSetBits(comm_event_group, MQTT_CONNECTED_BIT);
			connect_fail_count = 0;
			break;
		case MQTT_EVENT_DISCONNECTED:
			xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			int con_state = get_nw_connection_state();
			if(con_state & IP_CONNECTED_BIT )
				connect_fail_count++;
#if ENABLE_MQTT_FAIL_REBOOT == 1
			if(connect_fail_count >= MQTT_FAILATTEMPTS)
				{
				ESP_LOGE(TAG, "reboot due to MQTT connect failure: heap - free=%u largest=%u min=%u", 
					heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), esp_get_minimum_free_heap_size());
				/** Not a good idea to reboot in a thermostat context, the thermostat function still works 
					However if MQTT_FAILATTEMPS is large enough a reboot triggered will help 
					Especially usefull with C3 version of esp32 with low RAM **/
				RESTART("MQTT_FAILATTEMPTS");
				}
#endif			
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
			mqtt_rx_msg_t msg = {0};
			msg.topic   = strndup(event->topic, event->topic_len);
			if(msg.topic)
				{
				msg.payload = malloc(event->data_len + 2);
				if(msg.payload)
					{
					memcpy(msg.payload, event->data, event->data_len);
					int ml = event->data_len;
					while(ml - 1 >= 0 && (msg.payload[ml - 1] == '\n' || msg.payload[ml - 1] == '\r'))
						{
						msg.payload[ml - 1] = 0;
						ml--;
						}
					msg.payload[ml] = ' ';
					msg.payload[ml + 1] = 0;
					}
				else
					free(msg.topic);
				}
			if(msg.topic && msg.payload)
				{
#if PROCESS_MESSAGE_BY_HANDLER == 0			
				xQueueSend(mqtt_rx_queue, &msg, pdMS_TO_TICKS(10)); 
#else	
				char **argv;
				int argc = 0;
				if(controller_op_registered)
					{
					argc = parse_argv(msg.payload, &argv);
					if(argc)
						{
						if(strcmp(msg.topic, DEVICE_TOPIC_Q) == 0)
							{
							if(argc && !strcmp(argv[0], "reqID"))
								publish_MQTT_client_status();
							}
						else
							app_cmd_handler(argc, argv);

						for (int i = 0; i < argc; i++)
							{
							if(argv[i])
								free(argv[i]);
							}
						if(argv)
							free(argv);	
						}
					}
				free(msg.topic);
				free(msg.payload);
#endif					
				}
			break;
			}
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

int mqtt_start(app_cmd_handler_t app_exec_funcion)
	{
	esp_err_t ret = ESP_FAIL;
	xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
	if(app_exec_funcion)
		app_cmd_handler = app_exec_funcion;
	if(!app_cmd_handler)
		return ESP_FAIL;
	int needed = snprintf(NULL, 0, "%s%02d", dev_conf.dev_name, dev_conf.dev_id);
	if (needed < 0) 
		return ESP_FAIL;
	snprintf(USER_MQTT, sizeof(USER_MQTT) - 1, "%s%02d", dev_conf.dev_name, dev_conf.dev_id);
	USER_MQTT[sizeof(USER_MQTT) - 1] = 0;
	const esp_mqtt_client_config_t mqtt_cfg = {
		    .broker.address.uri = dev_conf.broker_url,
			.broker.verification.certificate = (const char *)nvs_ca_crt,
			.broker.verification.certificate_len = nvs_ca_crt_sz,
			.credentials = {
				.client_id = USER_MQTT,
				.authentication = {
					.certificate = (const char *)nvs_cl_crt,
					.certificate_len = nvs_cl_crt_sz,
					.key = (const char *)nvs_cl_key,
					.key_len = nvs_cl_key_sz,
					},
				},
			.network.disable_auto_reconnect = false,
			.network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
			.session.message_retransmit_timeout = 500,
			.session.keepalive = 30,
			.task.stack_size = 8192,
			};
	create_topics();
   	mqtt_client_handle = esp_mqtt_client_init(&mqtt_cfg);
   	if(mqtt_client_handle)
    	{
#if PROCESS_MESSAGE_BY_HANDLER == 1					
		if(esp_mqtt_client_register_event(mqtt_client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK)
			ret = esp_mqtt_client_start(mqtt_client_handle);
#else
		if(!mqtt_rx_queue)
			mqtt_rx_queue = xQueueCreate(8, sizeof(mqtt_rx_msg_t));
		if(mqtt_rx_queue)
			{
			//xTaskCreate(mqtt_rx_task, "mqtt_rx", 8192, NULL, USER_TASK_PRIORITY, &mqtt_rx_task_handle);
			if(!mqtt_rx_task_handle)
				xTaskCreate(mqtt_rx_task, "mqtt_rx", 8192, NULL, USER_TASK_PRIORITY, &mqtt_rx_task_handle);
			if(mqtt_rx_task_handle)
				{
    			if(esp_mqtt_client_register_event(mqtt_client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK)
    				ret = esp_mqtt_client_start(mqtt_client_handle);
				}
			else
				{
				vQueueDelete(mqtt_rx_queue);
				mqtt_rx_queue = NULL;
				}
			}
#endif					
    	}
    return ret;
    }

void publish_reqID()
	{
	if(mqtt_client_handle)
		esp_mqtt_client_publish(mqtt_client_handle, DEVICE_TOPIC_Q, "reqID", 5, 0,0);
	}
void publish_topic(char * topic, char * msg, int qos, int retain)
	{
	char *pmsg;
#if MQTT_PUBLISH == 0
	return;
#endif
	if(!mqtt_client_handle)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		
		uint64_t tmsec = esp_timer_get_time();
		int needed = snprintf(NULL, 0, "%llu\1%s\1%s", tmsec, USER_MQTT, msg);
		pmsg = malloc(needed + 1);
		if(pmsg)
			{
			snprintf(pmsg, (size_t)needed + 1, "%llu\1%s\1%s", tmsec, USER_MQTT, msg);
			esp_mqtt_client_publish(mqtt_client_handle, topic, pmsg, strlen(pmsg), qos, retain);
			free(pmsg);
			}
		}
	}

void publish_MQTT_client_status()
	{
	time_t now;
	struct tm timeinfo;
    char msg[512];
    char strtime[50];
    uint64_t tmsec = esp_timer_get_time();
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S", &timeinfo);
	snprintf(msg, sizeof(msg) - 1, "%s\1%s\1" IPSTR "\1%d\1%s\1%llu\1%s\1%s<br>%s\1%s\1%s\1",
			USER_MQTT, dev_conf.friendly_name, IP2STR(&dev_ipinfo.ip), dev_conf.dev_id, strtime, tmsec, 
			bpart_desc.project_name, bpart_desc.version, common_app_desc.common_version, bpart_desc.date, bpart_desc.time);
	esp_mqtt_client_publish(mqtt_client_handle, DEVICE_TOPIC_R, msg, strlen(msg), 0, 0);
	}
void publish_MQTT_client_log(char *message)
	{
	size_t sz = strlen(message);
	if (sz == 0) 
		return;
	if(message[sz - 1] == '\n')
		message[sz - 1] = 0;
	esp_mqtt_client_publish(mqtt_client_handle, TOPIC_LOG, message, strlen(message), 0, 0);
	}

void create_topics()
	{
	ESP_LOGI(TAG, "USER_MQTT: %s", USER_MQTT);
	strcpy(TOPIC_STATE, USER_MQTT);
	strcat(TOPIC_STATE, "/state");
	strcpy(TOPIC_ERROR, USER_MQTT);
	strcat(TOPIC_ERROR, "/error");
	strcpy(TOPIC_MONITOR, USER_MQTT);
	strcat(TOPIC_MONITOR, "/monitor");
	strcpy(TOPIC_CTRL, USER_MQTT);
	strcat(TOPIC_CTRL, "/ctrl");
	strcpy(TOPIC_LOG, USER_MQTT);
	strcat(TOPIC_LOG, "/log");
	strcpy(TOPIC_KA, USER_MQTT);
	strcat(TOPIC_KA, "/ka");
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	strcpy(TOPIC_STATE_A, TOPIC_STATE);
	strcat(TOPIC_STATE_A, "/w");
	strcpy(TOPIC_MONITOR_A, TOPIC_MONITOR);
	strcat(TOPIC_MONITOR_A, "/w");
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
        p = strtok(NULL, " \n");
    	}

    *argv_out = argv;
    return argc;
	}

#if !PROCESS_MESSAGE_BY_HANDLER
static void mqtt_rx_task(void *arg)
	{
    mqtt_rx_msg_t msg;
	static char **argv;
	int argc;
	ESP_LOGI(TAG, "mqtt_rx_task started");
    for (;;) 
    	{
        if (xQueueReceive(mqtt_rx_queue, &msg, portMAX_DELAY) == pdTRUE) 
        	{
			if(controller_op_registered)
				{
				argc = parse_argv(msg.payload, &argv);
			    if(argc)
			    	{
    				if (strcmp(msg.topic, TOPIC_CTRL) == 0) 
						{
						if(app_cmd_handler)
							app_cmd_handler(argc, argv);
						}
    				else if (strcmp(msg.topic, DEVICE_TOPIC_Q) == 0) 
    					{
        				if (argc && !strcmp(argv[0], "reqID"))
            			publish_MQTT_client_status();
    					}
					for (int i = 0; i < argc; i++)
						{
						if(argv[i])
							free(argv[i]);
						}
					if(argv)
    					free(argv);
    				}
    			}
			free(msg.payload);
			free(msg.topic);
    	    }
    	}
	}
#endif

