/*
 * rot_enc.c
 *
 *  Created on: Mar 3, 2024
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "freertos/freertos.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "common_defines.h"
#include "project_specific.h"
#include "driver/gptimer.h"
#include "gpios.h"
#include "rot_enc.h"

static const char *TAG = "ROT_ENC";
static QueueHandle_t cmd_q;
static gptimer_handle_t key_timer;
static int key_state, key_timer_state;

void IRAM_ATTR rot_enc_isr_handler(void* arg)
	{
	msg_t msg;
    uint32_t gpio_num = (uint32_t) arg;
	if(gpio_num == ROT_ENC_S1)
		msg.source = 1;
	else if(gpio_num == ROT_ENC_KEY)
		msg.source = 2;
	xQueueSendFromISR(cmd_q, &msg, NULL);
	}

static bool IRAM_ATTR key_timer_callback(gptimer_handle_t c_timer, const gptimer_alarm_event_data_t *edata, void *args)
	{
	msg_t msg;
    BaseType_t high_task_awoken = pdFALSE;
    msg.source = 3;
	xQueueSendFromISR(cmd_q, &msg, NULL);
    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
	}

static void config_key_timer()
	{
	key_timer = NULL;
	gptimer_config_t gptconf = 	{
								.clk_src = GPTIMER_CLK_SRC_DEFAULT,
								.direction = GPTIMER_COUNT_UP,
								.resolution_hz = 1000000,					//1 usec resolution

								};
	gptimer_alarm_config_t al_config = 	{
										.reload_count = 0,
										.alarm_count = PUSH_TIME_SHORT,
										.flags.auto_reload_on_alarm = true,
										};

	gptimer_event_callbacks_t cbs = {.on_alarm = &key_timer_callback,}; // register user callback
	ESP_ERROR_CHECK(gptimer_new_timer(&gptconf, &key_timer));
	ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
	ESP_ERROR_CHECK(gptimer_register_event_callbacks(key_timer, &cbs, NULL));
	ESP_ERROR_CHECK(gptimer_enable(key_timer));
	}
static void init_gpios()
	{
	gpio_config_t io_conf;

	io_conf.intr_type = GPIO_INTR_POSEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ROT_ENC_S1);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ROT_ENC_KEY);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_isr_handler_add(ROT_ENC_S1, rot_enc_isr_handler, (void*) ROT_ENC_S1);
    gpio_isr_handler_add(ROT_ENC_KEY, rot_enc_isr_handler, (void*) ROT_ENC_KEY);

    io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ROT_ENC_S2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
	}

static void rot_enc_cmd(void* arg)
	{
	msg_t msg;
    int i;
    uint32_t s1, s2, key;
    while(1)
    	{
        if(xQueueReceive(cmd_q, &msg, portMAX_DELAY))
        	{
        	s1 = gpio_get_level(ROT_ENC_S1);
        	s2 = gpio_get_level(ROT_ENC_S2);
        	key = gpio_get_level(ROT_ENC_KEY);
        	if(msg.source == 1) // knob rotation
        		{
        		if(s1 == 1 && s2 == 0)
        			ESP_LOGI(TAG, "knob rotate left");
        		else if(s1 == 1 && s2 == 1)
        			ESP_LOGI(TAG, "knob rotate right");
        		}
        	else if(msg.source == 2) // key pressed or released
        		{
        		if(key)
        			{
        			if(!key_state)
        				{
        				if(key_timer_state)
        					{
        					gptimer_stop(key_timer);
        					key_timer_state = 0;
        					}
        				ESP_LOGI(TAG, "key released");
        				key_state = key;
        				}
        			}
        		else
        			{
        			if(key_state)
        				{
        				gptimer_set_raw_count(key_timer, 0);
        				gptimer_start(key_timer);
        				key_timer_state = 1;
        				ESP_LOGI(TAG, "key pressed");
        				key_state = key;
        				}
        			}
        		}
        	else if(msg.source == 3) //key timer expired
        		{
        		if(key_timer_state)
        			{
        			gptimer_stop(key_timer);
        			key_timer_state = 0;
        			ESP_LOGI(TAG, "timer expired - key command");
        			}
        		}
        	}
    	}
	}
void init_rotenc()
	{
	TaskHandle_t rot_enc_cmd_handle;
	key_state = 1;
	cmd_q = xQueueCreate(10, sizeof(msg_t));
	if(!cmd_q)
		{
		ESP_LOGE(TAG, "Unable to create rotary encoder cmd queue");
		esp_restart();
		}
	init_gpios();
	config_key_timer();
	key_timer_state = 0;
	xTaskCreate(rot_enc_cmd, "rot_enc_cmd", 4196, NULL, 5, &rot_enc_cmd_handle);
	if(!rot_enc_cmd_handle)
		{
		ESP_LOGE(TAG, "Unable to start rotary encoder cmd task");
		esp_restart();
		}
	}

