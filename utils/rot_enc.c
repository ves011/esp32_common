/*
 * rot_enc.c
 *
 *  Created on: Mar 3, 2024
 *      Author: viorel_serbu
 */
#include "esp_timer.h"
//#include "portmacro.h"
#include "project_specific.h"
#ifdef ROT_ENCODER
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "common_defines.h"
#include "driver/gptimer.h"
#include "utils.h"
#include "rot_enc.h"

static const char *TAG = "ROT_ENC";
static QueueHandle_t cmd_q;
static gptimer_handle_t key_timer;
static int key_state, press_time;
static int key_count;
static uint64_t last_key_event_time;

static QueueHandle_t ui_cmd_queue;

//#define DEBUG_ON

void IRAM_ATTR rot_enc_isr_handler(void* arg)
	{
	msg_t msg = {0};
	BaseType_t high_task_awoken = pdFALSE;
    uint32_t gpio_num = (uint32_t) arg;
	if(gpio_num == ROT_ENC_S1)
		{
		msg.ifvals.ival[0] = gpio_get_level(ROT_ENC_S1);
		msg.ifvals.ival[1] = gpio_get_level(ROT_ENC_S2);
		msg.source = SOURCE_ROT;
		}
	else if(gpio_num == ROT_ENC_KEY)
		{
		msg.source = SOURCE_KEY;
		msg.val = gpio_get_level(ROT_ENC_KEY);
		msg.vts.ts = esp_timer_get_time();
		}
	else 
		return;
	xQueueSendFromISR(cmd_q, &msg, &high_task_awoken);
	if(high_task_awoken)
		portYIELD_FROM_ISR();
	}

static bool IRAM_ATTR key_timer_callback(gptimer_handle_t c_timer, const gptimer_alarm_event_data_t *edata, void *args)
	{
	msg_t msg = {0};
    BaseType_t high_task_awoken = pdFALSE;
    msg.source = SOURCE_TIMER;
	xQueueSendFromISR(cmd_q, &msg, &high_task_awoken);
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
										.flags.auto_reload_on_alarm = false,
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
	msg_t msgr, msgs;
    int s1, s2, key;
    key_count = 0;
    last_key_event_time = 0;
    while(1)
    	{
        if(xQueueReceive(cmd_q, &msgr, portMAX_DELAY))
        	{
        	if(msgr.source == SOURCE_ROT) // knob rotation
        		{
				s1 = msgr.ifvals.ival[0];
        		s2 = msgr.ifvals.ival[1];
        		if(s1 == 1 && s2 == 0)
        			{
					#ifdef DEBUG_ON
        				ESP_LOGI(TAG, "knob rotate left");
        			#endif
        			msgs.source = K_ROT;
        			msgs.val = K_ROT_RIGHT;
        			xQueueSend(ui_cmd_queue, &msgs, 0);
        			}
        		else if(s1 == 1 && s2 == 1)
        			{
					#ifdef DEBUG_ON
        				ESP_LOGI(TAG, "knob rotate right");
        			#endif
        			msgs.source = K_ROT;
        			msgs.val = K_ROT_LEFT;
        			xQueueSend(ui_cmd_queue, &msgs, 0);
        			}
        		}
        	else if(msgr.source == SOURCE_KEY) // key pressed or released
        		{
				key = msgr.val;
        		if(key == KEY_RELEASED)
        			{
        			if(key_state)
        				{
       					gptimer_stop(key_timer);
        				#ifdef DEBUG_ON
        					ESP_LOGI(TAG, "key released");
        				#endif
        				key_state = key;
        				msgs.source = K_UP;
        				xQueueSend(ui_cmd_queue, &msgs, 0);
        				}
        			}
        		else
        			{
        			if(!key_state)
        				{
        				//always start timer with PUSH_TIME_SHORT;
        				gptimer_alarm_config_t al_config = 	{
										.reload_count = 0,
										.alarm_count = PUSH_TIME_SHORT,
										.flags.auto_reload_on_alarm = false,
										};
        				ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
        				gptimer_set_raw_count(key_timer, 0);
        				gptimer_start(key_timer);
        				press_time = PUSH_TIME_SHORT;
        				#ifdef DEBUG_ON
        					ESP_LOGI(TAG, "key pressed %llu / %d", msgr.vts.ts - last_key_event_time, key_count);
        				#endif

						if(msgr.vts.ts - last_key_event_time < PUSH_KEY_REPEAT)
							key_count++;
						else
							key_count = 0;

						last_key_event_time = msgr.vts.ts;
        				key_state = key;
        				msgs.source = K_DOWN;
        				msgs.ifvals.ival[0] = key_count;
        				xQueueSend(ui_cmd_queue, &msgs, 0);
        				}
        			}
        		}
        	else if(msgr.source == SOURCE_TIMER) //key timer expired
        		{
				// Timer events are valid only while the key is logically pressed.
    			// Any KEY_RELEASE invalidates all pending timer events.
				if(key_state == KEY_RELEASED)
					continue;
				if(press_time == PUSH_TIME_SHORT)
    				{
    				//short press
    				#ifdef DEBUG_ON
    					ESP_LOGI(TAG, "short timer expired - key command");
    				#endif
    				msgs.source = K_PRESS;
    				msgs.val = PUSH_TIME_SHORT;
    				xQueueSend(ui_cmd_queue, &msgs, 0);
    				
    				//reconfigure timer to count for long press
    				gptimer_alarm_config_t al_config = 	{
									.reload_count = 0,
									.alarm_count = PUSH_TIME_LONG - PUSH_TIME_SHORT,
									.flags.auto_reload_on_alarm = false,
									};
    				ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
    				gptimer_set_raw_count(key_timer, 0);
    				gptimer_start(key_timer);
    				press_time = PUSH_TIME_LONG;
    				}
				else if(press_time == PUSH_TIME_LONG)
    				{
    				//long press
    				#ifdef DEBUG_ON
    					ESP_LOGI(TAG, "long timer expired - key command");
    				#endif
    				//reconfigure timer to count for short press
    				msgs.source = K_PRESS;
    				msgs.val = PUSH_TIME_LONG;
    				xQueueSend(ui_cmd_queue, &msgs, 0);
    				
    				//reconfigure timer to count for longlong press
    				gptimer_alarm_config_t al_config = 	{
									.reload_count = 0,
									.alarm_count = PUSH_TIME_LONGLONG -PUSH_TIME_LONG,
									.flags.auto_reload_on_alarm = false,
									};
    				ESP_ERROR_CHECK(gptimer_set_alarm_action(key_timer, &al_config));
    				gptimer_set_raw_count(key_timer, 0);
    				gptimer_start(key_timer);
    				press_time = PUSH_TIME_LONGLONG;
    				}
				else if(press_time == PUSH_TIME_LONGLONG)
    				{
    				//long press
    				#ifdef DEBUG_ON
    					ESP_LOGI(TAG, "longlong timer expired - key command");
    				#endif
    				msgs.source = K_PRESS;
    				msgs.val = PUSH_TIME_LONGLONG;
    				xQueueSend(ui_cmd_queue, &msgs, 0);
    				}
        		}
        	}
    	}
	}
void init_rotenc(QueueHandle_t ui_cmd_q)
	{
	TaskHandle_t rot_enc_cmd_handle;
	init_gpios();
	key_state = gpio_get_level(ROT_ENC_KEY);;
	press_time = PUSH_TIME_SHORT;
	cmd_q = xQueueCreate(10, sizeof(msg_t));
	if(!cmd_q)
		{
		ESP_LOGE(TAG, "Unable to create rotary encoder cmd queue");
		my_esp_restart();
		}
	ui_cmd_queue = ui_cmd_q;
	
	config_key_timer();
	//key_timer_state = 0;
	xTaskCreate(rot_enc_cmd, "rot_enc_cmd", 2048, NULL, 5, &rot_enc_cmd_handle);
	if(!rot_enc_cmd_handle)
		{
		ESP_LOGE(TAG, "Unable to start rotary encoder cmd task");
		my_esp_restart();
		}
	}
#endif
