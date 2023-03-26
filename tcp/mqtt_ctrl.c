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
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_spiffs.h"
#include "sdkconfig.h"
#include "common_defines.h"
#include "external_defs.h"
#include "utils.h"
#include "cmd_system.h"
#include "mqtt_ctrl.h"
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
#include "adc_op.h"
#include "pumpop.h"
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
#include "gateop.h"
#endif

#define CONFIG_BROKER_URL "mqtts://proxy.gnet:1886"
esp_mqtt_client_handle_t client;
int client_connected = 0;
static const char *TAG = "MQTTClient";

static char TOPIC_STATE[32], TOPIC_MONITOR[32], TOPIC_CTRL[32], TOPIC_CMD[32];
char USER_MQTT[32];

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_end");

static void create_topics(void);

static struct
	{
    struct arg_str *topic;
    struct arg_str *msg;
    struct arg_int *qos;
    struct arg_int *retain;
    struct arg_end *end;
	} mqtt_pargs;

static struct
	{
    struct arg_str *topic;
    struct arg_end *end;
	} mqtt_sargs;

static void log_error_if_nonzero(const char *message, int error_code)
	{
    if (error_code != 0)
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
	}

static void subscribe(char *topic)
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
		//ESP_LOGI(TAG, "Client subscribed to %s", topic);
		}
	}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
	{
    //ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    //esp_mqtt_client_handle_t client = event->client;
    char topic[80], msg[150];
    char **argv;
    int i, argc, ac;

    switch ((esp_mqtt_event_id_t)event_id)
    	{
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
#ifdef ACTIVE_CONTROLLER
			subscribe(TOPIC_CMD);
			subscribe(TOPIC_CTRL);
			subscribe(DEVICE_TOPIC_Q);
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
			ESP_LOGI(TAG, "MQTT_EVENT_DATA: %s - %s / %d / %d", topic, msg, event->topic_len, event->data_len);
			argv = calloc(10, sizeof(char *));
			argc = 0;
			ac = 0;
			for(i = 0; i < 10; i++)
				{
				argv[i] = calloc(256, sizeof(uint8_t));
				argv[i][0] = 0;
				}
			for(i = 0; i < event->data_len; i++)
				{
				if(isspace(msg[i]))
					{
					if(ac)
						argv[argc][ac] = 0;
					ac = 0;
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
#endif
				}
			else if(strcmp(topic, TOPIC_CTRL) == 0)
				{
				for(i = 0; i < argc - 1; i++)
					strcpy(argv[i], argv[i + 1]);
				argc--;
				do_system_cmd(argc, argv);
				}
			else if(strcmp(topic, DEVICE_TOPIC_Q) == 0)
				{
				if(!strcmp(argv[1], "reqID"))
					publish_MQTT_client_status();
				}
			for(i = 0; i < 10; i++)
				free(argv[i]);
			free(argv);
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

static int mqtt_send(int argc, char **argv)
	{
	char topic[100], msg[100];
	int qos = 0, retain = 0;
	int nerrors = arg_parse(argc, argv, (void **) &mqtt_pargs);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, mqtt_pargs.end, argv[0]);
        my_printf("%s arguments error\n", argv[0]);
        return 1;
    	}
	if(mqtt_pargs.topic->count == 0)
		{
		my_printf("No topic provided\n");
		return 1;
		}
	else
		strcpy(topic, mqtt_pargs.topic->sval[0]);
	if(mqtt_pargs.msg->count)
		strcpy(msg, mqtt_pargs.msg->sval[0]);
	else
		msg[0] = 0;
	if(mqtt_pargs.qos->count)
		qos = mqtt_pargs.qos->ival[0];
	if(mqtt_pargs.retain->count)
		retain = mqtt_pargs.retain->ival[0];

	esp_mqtt_client_publish(client, topic, msg, 0, qos, retain);
	return 0;
	}
static int mqtt_subscribe(int argc, char **argv)
	{
	char topic[100];
	int nerrors = arg_parse(argc, argv, (void **) &mqtt_sargs);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, mqtt_sargs.end, argv[0]);
        my_printf("%s arguments error\n", argv[0]);
        return 1;
    	}
    if(mqtt_sargs.topic->count == 0)
		{
		my_printf("No topic provided\n");
		return 1;
		}
	else
		strcpy(topic, mqtt_sargs.topic->sval[0]);
	esp_mqtt_client_subscribe(client, topic, 0);
	return 0;
	}

void register_mqtt()
	{
	mqtt_pargs.topic = arg_str1(NULL, "t", "topic", "topic");
    mqtt_pargs.msg = arg_str0(NULL, "m", "message", "message");
    mqtt_pargs.qos = arg_int0(NULL, "q", "<qos>", "qos");
    mqtt_pargs.retain = arg_int0(NULL, "r", "<1|0>", "retain");
    mqtt_pargs.end = arg_end(2);
    const esp_console_cmd_t mqtt_cmd =
    	{
        .command = "send",
        .help = "send mqtt message",
        .hint = NULL,
        .func = &mqtt_send,
        .argtable = &mqtt_pargs
    	};
 	ESP_ERROR_CHECK( esp_console_cmd_register(&mqtt_cmd) );

 	mqtt_sargs.topic = arg_str1(NULL, NULL, "topic", "topic");
    mqtt_sargs.end = arg_end(2);
    const esp_console_cmd_t mqtt_subs =
    	{
        .command = "subscribe",
        .help = "subscribe to topic",
        .hint = NULL,
        .func = &mqtt_subscribe,
        .argtable = &mqtt_sargs
    	};
 	ESP_ERROR_CHECK( esp_console_cmd_register(&mqtt_subs) );
	}

int mqtt_start(void)
	{
	esp_err_t ret = ESP_FAIL;
    esp_mqtt_client_config_t mqtt_cfg =
    	{
        .uri = CONFIG_BROKER_URL,
        .client_cert_pem = (const char *)client_cert_pem_start,
        .client_key_pem = (const char *)client_key_pem_start,
        .cert_pem = (const char *)server_cert_pem_start,
        //.username = USER_MQTT,
        //.password = PASS_MQTT,
        .client_id = USER_MQTT,
        .disable_auto_reconnect = false,
        //.refresh_connection_after_ms = 5000,
        .reconnect_timeout_ms = 2000,
        .message_retransmit_timeout = 500,
        .keepalive = 30,
        .task_stack = 8192,
        //.disable_clean_session = false,
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

void publish_state(char *msg, int qos, int retain)
	{
	time_t now;
    struct tm timeinfo;
    char strtime[1024];
	if(!client)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S\1", &timeinfo);
		strcat(strtime, USER_MQTT);
		strcat(strtime, "\1");
		strcat(strtime, msg);

		esp_mqtt_client_publish(client, TOPIC_STATE, strtime, strlen(strtime), qos, retain);
		}
	}

void publish_monitor(char *msg, int qos, int retain)
	{
	time_t now;
    struct tm timeinfo;
    char strtime[1024];
	if(!client)
		ESP_LOGE(TAG, "Client not connected");
	else
		{
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strtime, sizeof(strtime), "%Y-%m-%d/%H:%M:%S\1", &timeinfo);
		strcat(strtime, USER_MQTT);
		strcat(strtime, "\1");
		strcat(strtime, msg);

		esp_mqtt_client_publish(client, TOPIC_MONITOR, strtime, strlen(strtime), qos, retain);
		}
	}

void publish_MQTT_client_status()
	{
	time_t now;
    char msg[150];
    uint64_t tmsec = esp_timer_get_time() /1000000;
	time(&now);
	sprintf(msg, "%s\1%s\1" IPSTR "\1%d\1%lu\1%llu",
			USER_MQTT, DEV_NAME, IP2STR(&dev_ipinfo.ip), CTRL_DEV_ID, now, tmsec);
	esp_mqtt_client_publish(client, DEVICE_TOPIC_R, msg, strlen(msg), 0, 1);
	}

void create_topics()
	{
#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	sprintf(USER_MQTT, "pump%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == AGATE_CONTROLLER
	sprintf(USER_MQTT, "gate%02d", CTRL_DEV_ID);
#elif ACTIVE_CONTROLLER == OTA_CONTROLLER
	sprintf(USER_MQTT, "ota%02d", CTRL_DEV_ID);
#endif
	strcpy(TOPIC_STATE, USER_MQTT);
	strcat(TOPIC_STATE, "/state");
	strcpy(TOPIC_MONITOR, USER_MQTT);
	strcat(TOPIC_MONITOR, "/monitor");
	strcpy(TOPIC_CMD, USER_MQTT);
	strcat(TOPIC_CMD, "/cmd");
	strcpy(TOPIC_CTRL, USER_MQTT);
	strcat(TOPIC_CTRL, "/ctrl");
	}
