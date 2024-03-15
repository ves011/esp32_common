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
#include "lvgl.h"
#include "lcd.h"
#include "rot_enc.h"

extern QueueHandle_t ui_cmd_q;

static const char *TAG = "ROT_ENC";
static QueueHandle_t cmd_q;
static gptimer_handle_t key_timer;
static int key_state, key_timer_state, press_time;

void IRAM_ATTR rot_enc_isr_handler(void* arg)
	{
	msg_t msg;
    uint32_t gpio_num = (uint32_t) arg;
	if(gpio_num == ROT_ENC_S1)
		msg.source = SOURCE_ROT;
	else if(gpio_num == ROT_ENC_KEY)
		msg.source = SOURCE_KEY;
	xQueueSendFromISR(cmd_q, &msg, 0);
	}

static bool IRAM_ATTR key_timer_callback(gptimer_handle_t c_timer, const gptimer_alarm_event_data_t *edata, void *args)
	{
	msg_t msg;
    BaseType_t high_task_awoken = pdFALSE;
    //gptimer_get_raw_count(c_timer, &value);
    msg.source = SOURCE_TIMER;
	xQueueSendFromISR(cmd_q, &msg, 0);
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
    uint32_t s1, s2, key;
    while(1)
    	{
        if(xQueueReceive(cmd_q, &msg, portMAX_DELAY))
        	{
        	s1 = gpio_get_level(ROT_ENC_S1);
        	s2 = gpio_get_level(ROT_ENC_S2);
        	key = gpio_get_level(ROT_ENC_KEY);
        	if(msg.source == SOURCE_ROT) // knob rotation
        		{
        		if(s1 == 1 && s2 == 0)
        			{
        			ESP_LOGI(TAG, "knob rotate left");
        			msg.source = K_ROT;
        			msg.val = K_ROT_LEFT;
        			xQueueSend(ui_cmd_q, &msg, 0);
        			}
        		else if(s1 == 1 && s2 == 1)
        			{
        			ESP_LOGI(TAG, "knob rotate right");
        			msg.source = K_ROT;
        			msg.val = K_ROT_RIGHT;
        			//ui_cmd_task();
        			xQueueSend(ui_cmd_q, &msg, 0);
        			}
        		}
        	else if(msg.source == SOURCE_KEY) // key pressed or released
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
        				msg.source = K_UP;
        				xQueueSend(ui_cmd_q, &msg, 0);
        				}
        			}
        		else
        			{
        			if(key_state)
        				{
        				//always start timer with PUSH_TIME_SHORT;
        				gptimer_alarm_config_t al_config = 	{
										.reload_count = 0,
										.alarm_count = PUSH_TIME_SHORT,
										.flags.auto_reload_on_alarm = true,
										};
        				ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
        				gptimer_set_raw_count(key_timer, 0);
        				gptimer_start(key_timer);
        				press_time = PUSH_TIME_SHORT;
        				key_timer_state = 1;
        				ESP_LOGI(TAG, "key pressed");
        				key_state = key;
        				msg.source = K_DOWN;
        				xQueueSend(ui_cmd_q, &msg, 0);
        				}
        			}
        		}
        	else if(msg.source == SOURCE_TIMER) //key timer expired
        		{
        		if(key_timer_state)
        			{
        			gptimer_stop(key_timer);
        			key_timer_state = 0;
        			if(press_time == PUSH_TIME_SHORT)
        				{
        				//short press
        				ESP_LOGI(TAG, "short timer expired - key command");
        				//reconfigure timer to count for long press
        				gptimer_alarm_config_t al_config = 	{
										.reload_count = 0,
										.alarm_count = PUSH_TIME_LONG - PUSH_TIME_SHORT,
										.flags.auto_reload_on_alarm = true,
										};
        				ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
        				gptimer_set_raw_count(key_timer, 0);
        				gptimer_start(key_timer);
        				key_timer_state = 1;
        				msg.source = K_PRESS;
        				press_time = PUSH_TIME_LONG;
        				msg.val = PUSH_TIME_SHORT;
        				xQueueSend(ui_cmd_q, &msg, 0);
        				}
        			else if(press_time == PUSH_TIME_LONG)
        				{
        				//long press
        				ESP_LOGI(TAG, "long timer expired - key command");
        				//reconfigure timer to count for short press
        				msg.source = K_PRESS;
        				msg.val = PUSH_TIME_LONG;
        				xQueueSend(ui_cmd_q, &msg, 0);
        				}
        			}
        		}
        	}
    	}
	}
void init_rotenc()
	{
	TaskHandle_t rot_enc_cmd_handle;
	key_state = 1;
	press_time = PUSH_TIME_SHORT;
	cmd_q = xQueueCreate(10, sizeof(msg_t));
	if(!cmd_q)
		{
		ESP_LOGE(TAG, "Unable to create rotary encoder cmd queue");
		esp_restart();
		}
	init_gpios();
	config_key_timer();
	key_timer_state = 0;
	xTaskCreatePinnedToCore(rot_enc_cmd, "rot_enc_cmd", 4196, NULL, 5, &rot_enc_cmd_handle, 1);
	if(!rot_enc_cmd_handle)
		{
		ESP_LOGE(TAG, "Unable to start rotary encoder cmd task");
		esp_restart();
		}
	}

