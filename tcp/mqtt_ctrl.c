/*
 * mqtt_ctrl.c
 *
 *  Created on: Mar 5, 2023
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "spi_flash_mmap.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "driver/i2c_master.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "project_specific.h"
#include "common_defines.h"
#include "external_defs.h"
#include "utils.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "mqtt_ctrl.h"
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
#elif ACTIVE_CONTROLLER == WATER_CONTROLLER || ACTIVE_CONTROLLER == WP_CONTROLLER
#include "waterop.h"
#include "pumpop.h"
#include "ad7811.h"
#endif

#define CONFIG_BROKER_URL "mqtts://proxy.gnet:1886"
esp_mqtt_client_handle_t client;
int client_connected = 0;
static const char *TAG = "MQTTClient";

char TOPIC_STATE[32], TOPIC_MONITOR[32], TOPIC_CTRL[32], TOPIC_CMD[32], TOPIC_LOG[32];
#if ACTIVE_CONTROLLER == WP_CONTROLLER
	char TOPIC_STATE_A[32], TOPIC_MONITOR_A[32];
#endif
char USER_MQTT[32];

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_end");

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
			ESP_LOGE(TAG, "cannot subscribe");
			//esp_restart();
			}
		ESP_LOGI(TAG, "Client subscribed to %s", topic);
		}
	}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
	{
    //ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    //esp_mqtt_client_handle_t client = event->client;
    char topic[80], msg[150];
    char **argv;
    int i, argc, ac, q;

    switch ((esp_mqtt_event_id_t)event_id)
    	{
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
#ifdef ACTIVE_CONTROLLER
			subscribe(TOPIC_CMD);
			subscribe(TOPIC_CTRL);
			subscribe(DEVICE_TOPIC_Q);
	#if ACTIVE_CONTROLLER == WATER_CONTROLLER
			subscribe(DEVICE_TOPIC_R);
			subscribe(WATER_PUMP_DESC"/monitor");
			subscribe(WATER_PUMP_DESC"/state");
	#endif
#endif

			publish_MQTT_client_status();
			client_connected = 1;
			break;
		case MQTT_EVENT_DISCONNECTED:
			client_connected = 0;
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
			for(i = 0; i < event->topic_len; i++)
				topic[i] = event->topic[i];
			topic[i] = 0;
			for(i = 0; i < event->data_len; i++)
				msg[i] = event->data[i];
			msg[i] = 0;
			//ESP_LOGI(TAG, "MQTT_EVENT_DATA: %s - %s / %d / %d", topic, msg, event->topic_len, event->data_len);
			if(controller_op_registered == 1)
				{
				/*
				 * based on assumption no more than 10 arguments will be provided for a command
				 * if wrong assumption: CRASH
				*/
				argv = calloc(10, sizeof(char *));
				argc = 0;
				ac = 0;
				for(i = 0; i < 10; i++)
					{
					argv[i] = calloc(256, sizeof(uint8_t));
					argv[i][0] = 0;
					}
				q = 0;
				for(i = 0; i < event->data_len; i++)
					{
					if(msg[i] == '"')
						{
						q = 1 -q;
						//argv[argc][ac++] = msg[i];
						continue;
						}

					if(isspace((int)msg[i]))
						{
						if(!q)
							{
							if(ac)
								argv[argc][ac] = 0;
							ac = 0;
							}
						else
							argv[argc][ac++] = msg[i];
						}
					else
						{
						if(ac == 0)
							argc++;
						argv[argc][ac++] = msg[i];
						}
					}
				if(ac)
					argv[argc][ac] = 0;
				ESP_LOGI(TAG, "argc, %d, %s, %s, %s, %s", argc, argv[0], argv[1], argv[2], argv[3]);
				argc++;

				if(strcmp(topic, TOPIC_CMD) == 0)
					{
	#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
					strcpy(argv[0], "pump");
					do_pumpop(argc, argv);
	#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
					strcpy(argv[0], "gate");
					do_gateop(argc, argv);
	#elif ACTIVE_CONTROLLER == WESTA_CONTROLLER
					strcpy(argv[0], "westa");
					do_westaop(argc, argv);
	#elif ACTIVE_CONTROLLER == WATER_CONTROLLER
					strcpy(argv[0], "dv");
					do_dvop(argc, argv);
	#elif 	ACTIVE_CONTROLLER == WP_CONTROLLER
					for(i = 0; i < argc - 1; i++)
						strcpy(argv[i], argv[i + 1]);
					argc--;
					do_dvop(argc, argv);
					do_pumpop(argc, argv);
	#endif
					}
				else if(strcmp(topic, TOPIC_CTRL) == 0)
					{
					for(i = 0; i < argc - 1; i++)
						strcpy(argv[i], argv[i + 1]);
					argc--;
					do_system_cmd(argc, argv);
					do_wifi(argc, argv);
#if ACTIVE_CONTROLLER != ESP32_TEST && ACTIVE_CONTROLLER == WP_CONROLLER
					do_ad(argc, argv);
#elif ACTIVE_CONTROLLER == NAVIGATOR
					do_nmea(argc, argv);
					do_mpu(argc, argv);
					do_ptst(argc, argv);
					do_hmc(argc, argv);
#endif
					}
				else if(strcmp(topic, DEVICE_TOPIC_Q) == 0)
					{
					if(!strcmp(argv[1], "reqID"))
						publish_MQTT_client_status();
					}
#if ACTIVE_CONTROLLER == WATER_CONTROLLER
				else if(!strcmp(topic, DEVICE_TOPIC_R) ||
						!strcmp(topic, WATER_PUMP_DESC"/monitor")  ||
						!strcmp(topic, WATER_PUMP_DESC"/state"))
					{
					strcpy(argv[0], topic);
					parse_devstr(argc, argv);
					}
#endif
				for(i = 0; i < 10; i++)
					free(argv[i]);
				free(argv);
				}
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
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
    	}
	}

int mqtt_start(void)
	{
	esp_err_t ret = ESP_FAIL;
	const esp_mqtt_client_config_t mqtt_cfg = {
		    .broker.address.uri = CONFIG_BROKER_URL,
		    .broker.verification.certificate = (const char *)server_cert_pem_start,
		    .credentials = {
		      .client_id = USER_MQTT,
		      .authentication = {
		        .certificate = (const char *)client_cert_pem_start,
		        .key = (const char *)client_key_pem_start,
		      	  },
		    	},
			.network.disable_auto_reconnect = false,
			.network.reconnect_timeout_ms = 2000,
			.session.message_retransmit_timeout = 500,
			.session.keepalive = 30,
			};
    create_topics();
    client = esp_mqtt_client_init(&mqtt_cfg);
    if(client)
    	{
    	if(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK)
    		ret = esp_mqtt_client_start(client);

    	}
    return ret;
    }

void publish_reqID()
	{
	char msg[20];
	strcpy(msg, "reqID");
	esp_mqtt_client_publish(client, DEVICE_TOPIC_Q, msg, strlen(msg), 0,0);
	}
void publish_topic(char * topic, char * msg, int qos, int retain)
	{
	char strtime[1024];
#if MQTT_PUBLISH == 0
	return;
#endif
	if(!client)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		uint64_t tmsec = esp_timer_get_time();
		sprintf(strtime, "%llu\1", tmsec);
		strcat(strtime, USER_MQTT);
		strcat(strtime, "\1");
		int len = strlen(strtime);
		strncat(strtime, msg, sizeof(strtime) - len -1);
		esp_mqtt_client_publish(client, topic, strtime, strlen(strtime), qos, retain);
		}
	}

void publish_MQTT_client_status()
	{
	time_t now;
	struct tm timeinfo;
    char msg[150];
    char strtime[50];
    uint64_t tmsec = esp_timer_get_time();
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S\1", &timeinfo);
	sprintf(msg, "%s\1%s\1" IPSTR "\1%d\1%s\1%llu",
			USER_MQTT, DEV_NAME, IP2STR(&dev_ipinfo.ip), CTRL_DEV_ID, strtime, tmsec);
	esp_mqtt_client_publish(client, DEVICE_TOPIC_R, msg, strlen(msg), 0, 0);
	}
void publish_MQTT_client_log(char *message)
	{
	esp_mqtt_client_publish(client, TOPIC_LOG, message, strlen(message), 0, 0);
	}

void create_topics()
	{
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	sprintf(USER_MQTT, "pump%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
	sprintf(USER_MQTT, "gate%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == OTA_CONTROLLER
	sprintf(USER_MQTT, "ota%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == WESTA_CONTROLLER
	sprintf(USER_MQTT, "westa%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == WATER_CONTROLLER
	sprintf(USER_MQTT, "water%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == WP_CONTROLLER
	sprintf(USER_MQTT, "wp%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == ESP32_TEST
	sprintf(USER_MQTT, "esp32%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == NAVIGATOR
	sprintf(USER_MQTT, "navi%02d", CTRL_DEV_ID);
#endif

	strcpy(TOPIC_STATE, USER_MQTT);
	strcat(TOPIC_STATE, "/state");
	strcpy(TOPIC_MONITOR, USER_MQTT);
	strcat(TOPIC_MONITOR, "/monitor");
	strcpy(TOPIC_CMD, USER_MQTT);
	strcat(TOPIC_CMD, "/cmd");
	strcpy(TOPIC_CTRL, USER_MQTT);
	strcat(TOPIC_CTRL, "/ctrl");
	strcpy(TOPIC_LOG, USER_MQTT);
	strcat(TOPIC_LOG, "/log");
	}
