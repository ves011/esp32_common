/*
 * adc_op.c
 *
 *  Created on: Feb 3, 2023
 *      Author: viorel_serbu
 */

#include <string.h>
#include <stdio.h>
#include "project_specific.h"
#ifdef ADC_ESP32
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include "esp_netif.h"
#include "driver/gptimer.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "math.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "common_defines.h"
#include "utils.h"
#include "gpios.h"
#include "adc_op.h"

//#define DEBUG_ADC

int stdev_c, stdev_p;

static const char *TAG = "ADC OP";

int sample_count, nr_samples;
int **adc_raw;

gptimer_handle_t adc_timer;
QueueHandle_t adc_evt_queue = NULL;
int cbt_user_data;     // user data top let adc timer cb to pass this value in adc_message_t.source

static adc_cali_handle_t adc1_cal_handle[10];
static adc_oneshot_unit_handle_t adc1_handle;
static SemaphoreHandle_t adcval_mutex;
static int *adc_ch_vect, adc_no_chn;
static int chn_to_use[3];
static int adc_init = 0;

static struct
	{
    struct arg_str *op;
    struct arg_int *arg;
    struct arg_int *arg1;
    struct arg_int *arg2;
    struct arg_end *end;
	} ad_args;
	
static bool IRAM_ATTR adc_timer_callback(gptimer_handle_t ad_timer, const gptimer_alarm_event_data_t *edata, void *args)
	{
    BaseType_t high_task_awoken = pdFALSE;
    for(int i = 0; i < adc_no_chn; i++)
    	{
    	adc_oneshot_read(adc1_handle, adc_ch_vect[i], (int *)(adc_raw[i] + sample_count));
    	//adc_oneshot_read(adc1_handle, adc_ch_vect[i], &adc_raw[i][sample_count]);
    	}
    sample_count++;
    if(sample_count >= nr_samples)
    	{
    	gptimer_stop(adc_timer);
    	adc_msg_t msg;
   	    msg.source = *(int *)args;
   	    msg.val = sample_count;
   		xQueueSendFromISR(adc_evt_queue, &msg, NULL);
    	}
    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
	}
void config_adc_timer()
	{
	adc_timer = NULL;
	gptimer_config_t gptconf = 	{
								.clk_src = GPTIMER_CLK_SRC_DEFAULT,
								.direction = GPTIMER_COUNT_UP,
								.resolution_hz = 1000000,					//1 usec resolution

								};
	gptimer_alarm_config_t al_config = 	{
										.reload_count = 0,
										.alarm_count = SAMPLE_PERIOD,
										.flags.auto_reload_on_alarm = true,
										};
	gptimer_event_callbacks_t cbs = {.on_alarm = &adc_timer_callback,}; // register user callback
	ESP_ERROR_CHECK(gptimer_new_timer(&gptconf, &adc_timer));
	ESP_ERROR_CHECK(gptimer_set_alarm_action(adc_timer, &al_config));
	ESP_ERROR_CHECK(gptimer_register_event_callbacks(adc_timer, &cbs, &cbt_user_data));
	ESP_ERROR_CHECK(gptimer_enable(adc_timer));
	}
//!!!
// ensure the caller always provides s_vect matrix with exact size of n_chn X nr_samp
//!!!
int adc_get_data(int *chn, int n_chn, int **s_vect, int nr_samp)
	{
	int ret = ESP_FAIL;
	adc_msg_t msg;
	int q_wait = (SAMPLE_PERIOD * nr_samp) / 1000 + 50;
	
	/*
     * take adcval mutex in 1.2 sec wait
     * worst case for 10 loops it takes 1.05 sec
     */
    
    if(!adc_init)
    	{
    	return ret;
    	}
	if(xSemaphoreTake(adcval_mutex, ( TickType_t ) 120 ) == pdTRUE)
    	{
		adc_raw = s_vect;
		adc_ch_vect = chn;
		adc_no_chn = n_chn;
		sample_count = 0;
		nr_samples = nr_samp;

		gptimer_set_raw_count(adc_timer, 0);
		gptimer_start(adc_timer);
		if(xQueueReceive(adc_evt_queue, &msg, q_wait / portTICK_PERIOD_MS) == pdPASS)
			{
			int bat_v = 0;
			for(int n = 0; n < n_chn; n++)
				{
				//ESP_LOGI(TAG, "chn calh: %d, %x", n, (unsigned int)adc1_cal_handle[n]);
				for(int j = 0; j < nr_samp; j++)
					{
					adc_cali_raw_to_voltage(/*adc1_cal_handle[chn[n]]*/adc1_cal_handle[n], *(adc_raw[n] + j), (adc_raw[n] + j));
					if(chn[n] == BAT_ADC_CHANNEL)
						{
						bat_v += *(adc_raw[n] + j);
						}
					}
				}
			if(bat_v)
				{
				bat_v /= nr_samp;
				}

			ret = ESP_OK;
			}
		else
			ESP_LOGI(TAG, "ADC not completed in %d (ms)", q_wait);
		xSemaphoreGive(adcval_mutex);
    	}
	else
		ESP_LOGE(TAG, "cannot take adcval_mutex");
	return ret;
	}

static bool adc_calibration_init5(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
	{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    	{

        adc_cali_curve_fitting_config_t cali_config =
        	{
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = SOC_ADC_DIGI_MAX_BITWIDTH,
        	};
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        	{
            calibrated = true;
            *out_handle = handle;
        	}
        ESP_LOGI(TAG, "calibration scheme version is Curve Fitting, %d", calibrated);
        }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    	{
        adc_cali_line_fitting_config_t cali_config =
        	{
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        	};
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        	{
            calibrated = true;
            adc1_cal_handle = handle;
        	}
        ESP_LOGI(TAG, "calibration scheme version is Line Fitting, %d", calibrated);
    	}
#endif
	return calibrated;
    }

void adc_init5(int nr_chn, int *chn_no)
	{
	
	adc1_handle = NULL;
	adc_evt_queue = xQueueCreate(5, sizeof(adc_msg_t));
	adcval_mutex = xSemaphoreCreateMutex();
	if(!adcval_mutex)
		{
		ESP_LOGE(TAG, "cannot create adcval_mutex");
		esp_restart();
		}
	adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1,};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
	adc_oneshot_chan_cfg_t config =
		{
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,
	    };
	for(int i = 0; i < nr_chn; i++)
		{
		adc1_cal_handle[i] = NULL;
		ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, chn_no[i], &config));
		adc_calibration_init5(ADC_UNIT_1, chn_no[i], ADC_ATTEN_DB_12, &adc1_cal_handle[i]);//adc1_cal_handle[chn_no[i]]);
		}
	config_adc_timer();
	adc_init = 1;
	}
int do_ad(int argc, char **argv)
	{
	int i, n, j;
	int nerrors = arg_parse(argc, argv, (void **)&ad_args);
	int chnn, chns, nrs;
	int ch_vect[10];
	//int **s_data;
	if (nerrors != 0)
		{
		arg_print_errors(stderr, ad_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(strcmp(ad_args.op->sval[0], "r") == 0)
		{
		int *s_data[1];
		int s1_data[500];
		if(ad_args.arg->count)
			ch_vect[0] = ad_args.arg->ival[0];
		else
			ch_vect[0] = 0;
		
		if(ad_args.arg1->count)
			nrs = ad_args.arg1->ival[0];
		else
			nrs = 1;
			
		for(i = 0; i < nrs; i++)
			{
			s_data[0] = s1_data;
			adc_get_data(ch_vect, 1, (int **)&s_data, 1);
			ESP_LOGI(TAG, "%d chn: %d = %d",  i, ch_vect[0], s1_data[0]);
			vTaskDelay(pdMS_TO_TICKS(10));
			}
		}
	else if(strcmp(ad_args.op->sval[0], "rm") == 0)
		{
		int *s_data[10];
		int *s1_data[5];
		//s_data[0] = s1_data;
		//s_data[1] = s2_data;
		//s_data[2] = s3_data;
		//s_data[3] = s4_data;
		//s_data[4] = s5_data; 
		//int s_data[3][5];
		// ad r <start channel> <no of channels> <nr samples>
		if(ad_args.arg->count)
			chns = ad_args.arg->ival[0];
		else
			chns = 0;
		if(ad_args.arg1->count)
			chnn = ad_args.arg1->ival[0];
		else
			chnn = 1;
		if(ad_args.arg2->count)
			nrs = ad_args.arg2->ival[0];
		else
			nrs = 5;
		for(i = 0; i < chnn; i++)
			{
			ch_vect[i] = chn_to_use[i + chns];
			s1_data[i] = calloc(nrs, sizeof(int));
			if(s1_data[i])
				s_data[i] = s1_data[i];
			else
				{
				for(int k = 0; k < i; k++)
					free(s1_data[k]);
				ESP_LOGE(TAG, "Not enough memory for sample data");
				break;
				}
			}
		if(i == chnn)
			{
			char c[30], m[256];
			strcpy(m, "  #  ");
			for(n = 0; n < chnn; n++)
				{
				sprintf(c, "\t%6d", ch_vect[n]);
				strcat(m, c);
				}
			my_printf("%s", m);
			adc_get_data(ch_vect, chnn, s_data, nrs);
			for(j = 0; j < nrs; j++)
				{
				sprintf(m, "%5d", j);
				for(n = 0; n < chnn; n++)
					{
					sprintf(c, "\t%6d",  *(s_data[n] + j));
					strcat(m, c);
					}
				my_printf("%s",  m);
				}
			for(i = 0; i < chnn; i++)
				free(s1_data[i]);
			}
		}
	return ESP_OK;
	}
void register_ad()
	{
#if ACTIVE_CONTROLLER == NAVIGATOR
	chn_to_use[0]  = ADC_CHN_0;
	chn_to_use[1]  = ADC_CHN_1;
	chn_to_use[2]  = ADC_CHN_2;
	adc_init5(3, chn_to_use);
#elif ACTIVE_CONTROLLER == FLOOR_HC
	chn_to_use[0] = 0;
	chn_to_use[1] = 4;
	adc_init5(2, chn_to_use);
#endif
	
	ad_args.op = arg_str1(NULL, NULL, "<op>", "op: r | mr");
	ad_args.arg = arg_int0(NULL, NULL, "<chn#>", "channel to read");
	ad_args.arg1 = arg_int0(NULL, NULL, "<nrs>", "no of samples");
	ad_args.arg2 = arg_int0(NULL, NULL, "<nrs>", "no of samples");
	ad_args.end = arg_end(1);
	const esp_console_cmd_t ad_cmd =
		{
		.command = "ad",
		.help = "read (r) | rm read AD channel no",
		.hint = NULL,
		.func = &do_ad,
		.argtable = &ad_args
		};
	ESP_ERROR_CHECK(esp_console_cmd_register(&ad_cmd));
	}
#endif