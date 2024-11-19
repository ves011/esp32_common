/*
 * ad7811.c
 *
 *  Created on: Nov 30, 2023
 *      Author: viorel_serbu
 */
#include "freertos/projdefs.h"
#include "project_specific.h"
#ifdef ADC_AD7811
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_reg.h"
#include "soc/dport_access.h"
#include "driver/spi_master.h"
#include "driver/gptimer.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "common_defines.h"
#include "project_specific.h"
#include "gpios.h"
#include "spicom.h"
#include "ad7811.h"

static const char* TAG = "AD op:";
static int vref;
static gptimer_handle_t adc_timer;
static volatile int sample_count, nr_samples;
uint16_t pc_samples[NR_SAMPLES_PC];
uint16_t ps_samples[NR_SAMPLES_PS];
uint16_t dv_samples[NR_SAMPLES_DV];
volatile int16_t *samples, *samp_bin;
int s_channel;
static QueueHandle_t adc_evt_queue = NULL;
static SemaphoreHandle_t adcval_mutex;

static int read_AD_simple(int chnn, uint16_t *data);
static int read_ADmv(int chnn, int *vmv, int *dbin);

struct
	{
    struct arg_str *op;
    struct arg_int *arg;
    struct arg_int *arg1;
    struct arg_end *end;
	} ad_args;

static bool IRAM_ATTR adc_timer_callback(gptimer_handle_t ad_timer, const gptimer_alarm_event_data_t *edata, void *args)
	{
    BaseType_t high_task_awoken = pdFALSE;
    int data, d_bin;
    adc_msg_t msg;
    samp_bin[sample_count] = samples[sample_count] = 0;
    if(read_ADmv(s_channel, &data, &d_bin) == ESP_OK)
    	{
    	samp_bin[sample_count] = d_bin;
    	samples[sample_count++] = data;
		if(sample_count >= nr_samples)
			{
			gptimer_stop(adc_timer);
			msg.source = 1;
			msg.val = sample_count;
			xQueueSendFromISR(adc_evt_queue, &msg, NULL);
			}
    	}
    else
    	{
    	gptimer_stop(adc_timer);
		adc_msg_t msg;
		msg.source = 1;
		msg.val = sample_count;
		xQueueSendFromISR(adc_evt_queue, &msg, NULL);
    	}
    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
	}

static void config_adc_timer()
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
	ESP_ERROR_CHECK(gptimer_register_event_callbacks(adc_timer, &cbs, NULL));
	ESP_ERROR_CHECK(gptimer_enable(adc_timer));
	}

int adc_get_data(int chn, int16_t *s_vect, int nr_samp)
	{
	int dummy = 0, dbin = 0, ret = ESP_FAIL;
	adc_msg_t msg;
	int16_t s_bin[200];
	//nr_samples = nr_samp;
	//sample_count = 0;
	//samples = s_vect;
	//s_channel = chn;
	int q_wait = (SAMPLE_PERIOD * NR_SAMPLES_PC) / 1000 + 50;
	if(xSemaphoreTake(adcval_mutex,  q_wait / portTICK_PERIOD_MS ) == pdTRUE)
		{
		nr_samples = nr_samp;
		sample_count = 0;
		samples = s_vect;
		samp_bin = s_bin;
		s_channel = chn;
		if(read_ADmv(chn, &dummy, &dbin) == ESP_OK)
			{
			gptimer_start(adc_timer);
			if(xQueueReceive(adc_evt_queue, &msg, q_wait / portTICK_PERIOD_MS)) //wait qwait msec ADC conversion to complete
				{
				if(msg.source == 1)
					{
					//ESP_LOGI(TAG, "chn: %d / nrs: %d / ret: %d / %d(%d), %d(%d), %d(%d), %d(%d), %d(%d)", chn, nr_samp, nr_samples, s_vect[0], s_bin[0], s_vect[1], s_bin[1], s_vect[2], s_bin[2], s_vect[3], s_bin[3], s_vect[4], s_bin[4]);
					if(msg.val == nr_samp)
						{

						ret = ESP_OK;
						}
					else
						{
						ret = ESP_FAIL;
						ESP_LOGI(TAG, "Error reading ADC data");
						}
					}
				}
			else
				ESP_LOGI(TAG, "adc_get_data: timeout");
			}
		xSemaphoreGive(adcval_mutex);
		}
	else
		ESP_LOGI(TAG, "cannot take adcval_mutex");
	return ret;
	}
void init_ad7811()
	{
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = (1ULL << PIN_NUM_CONVST);
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);
	gpio_set_level(PIN_NUM_CONVST, 1); //no power down between conversions
	adc_evt_queue = xQueueCreate(5, sizeof(adc_msg_t));
	config_adc_timer();
#ifdef EXTREF
	vref = EXTREF;
#endif
#ifdef INTREF
	vref = INTREF;
#endif
	spiadmaster_init();
	adcval_mutex = xSemaphoreCreateMutex();
	if(adcval_mutex == NULL)
		ESP_LOGI(TAG, "error creating adcval_mutex");
	}


/*
 * read specific channel (chnn) from AD7811
 * returns raw binary value or 0xffff if SPI transaction fails
 * cmd structure
 * 	A0 = 0
 * 	PD1|PD0 = 1|1
 * 	VinAGND|DIFF/SGL = 0|0
 * 	CH1|CH0 = x|x
 * 	CONVST	= 0
 * 	EXTREF	= 1 if EXTERF
 * 			= 0 if INTREF
 * when channel changed the first read if for previous channel: hence 2 reads
 */
uint16_t read_AD(int chnn)
	{
	uint16_t cmd, data = 0xffff;
	cmd = 0x3000 | (chnn << 8);
#ifdef EXTREF
	cmd |= 0x0040;
#endif
	//requires 2 read
	// first read returns data for previous channel
	// second read returns data for current channel
	if(spiad_rw(cmd, 13, &data) == ESP_OK)
		if(spiad_rw(cmd, 13, &data) == ESP_OK)
			{
			//ESP_LOGI(TAG, "chnn: %d %0x", chnn, data);
			return data;
			}
	return data;
	}

static int read_AD_simple(int chnn, uint16_t *data)
	{
	uint16_t cmd;
	cmd = 0x3000 | (chnn << 8);
#ifdef EXTREF
	cmd |= 0x0040;
#endif
	return spiad_rw(cmd, 16, data);
	}

/*
 * read specific channel (chnn) from AD7811
 * return read value converted to mV or -1 if SPI transaction fails
 */
static int read_ADmv(int chnn, int *vmv, int *dbin)
	{
	uint16_t data;
	int ret;
	if((ret = read_AD_simple(chnn, &data)) == ESP_OK)
		{
		*vmv = (vref * data) / 1024;
		*dbin = data;
		}
	return ret;
	}

int do_ad(int argc, char **argv)
	{
	int nerrors = arg_parse(argc, argv, (void **)&ad_args);
	int chnn, nrs;
	int16_t s_data[200];
	int data;
	int dbin;
	if (nerrors != 0)
		{
		arg_print_errors(stderr, ad_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(strcmp(ad_args.op->sval[0], "r") == 0)
		{
		if(ad_args.arg->count)
			chnn = ad_args.arg->ival[0];
		else
			chnn = 0;
		if(ad_args.arg1->count)
			nrs = ad_args.arg1->ival[0];
		else
			nrs = 1;
		//adc_get_data(chnn, s_data, nrs);
		for(int i = 0; i < nrs; i++)
			{
			read_ADmv(chnn, &data, &dbin);
			ESP_LOGI(TAG, "chn: %d = %d",  chnn, data);
			vTaskDelay(pdMS_TO_TICKS(50));
			}
		//ESP_LOGI(TAG, "chn: %d = %d / %d, %d, %d, %d",  chnn, s_data[0], s_data[1], s_data[2], s_data[3], s_data[4]);
		}
	else if(strcmp(ad_args.op->sval[0], "mr") == 0)
		{
		if(ad_args.arg->count)
			{
			chnn = ad_args.arg->ival[0];
			if(ad_args.arg1->count)
				{
				nrs = ad_args.arg1->ival[0];
				adc_get_data(chnn, s_data, nrs);
				}
			}
		}
	return ESP_OK;
	}
void register_ad()
	{
	init_ad7811();
	ad_args.op = arg_str1(NULL, NULL, "<op>", "op: r | mr");
	ad_args.arg = arg_int0(NULL, NULL, "<arg>", "value to write");
	ad_args.arg1 = arg_int0(NULL, NULL, "<nrs>", "no of samples");
	ad_args.end = arg_end(1);
	const esp_console_cmd_t ad_cmd =
		{
		.command = "ad",
		.help = "read (r) / write (w) to/from AD",
		.hint = NULL,
		.func = &do_ad,
		.argtable = &ad_args
		};
	ESP_ERROR_CHECK(esp_console_cmd_register(&ad_cmd));
	}
	
#endif
	
