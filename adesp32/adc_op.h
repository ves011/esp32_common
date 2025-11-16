/*
 * adc_op.h
 *
 *  Created on: Feb 3, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_ADC_OP_H_
#define MAIN_ADC_OP_H_

#define TIMER_SCALE_ADC           	(TIMER_BASE_CLK / TIMER_DIVIDER / 1000000)  // convert counter value to useconds
#define SAMPLE_PERIOD				400


#define LOOP_COUNT					10
#define STDEV_THERSHOLD				30

#define ADC_RESULT_BYTE    			4
#define ADC_CONV_LIMIT_EN   		0

#define GET_UNIT(x)        ((x>>3) & 0x1)

#define JS_REQUEST			1
#define CON_REQUEST			2

#define CONV_DONE_EVT		1


/* ACS712 20A --> 100mv/A
 * min resolution = 15mV --> 150mA
 */


typedef struct
	{
	uint8_t source;
	uint8_t step;
	uint32_t val;
	uint64_t ts;
	} adc_msg_t;

typedef struct
    {
    uint32_t mv;
    uint32_t index;
    } minmax_t;

extern int stdev_c, stdev_p;

//void adc_calibration_init(void);
void adc_init5(int nr_chn, int *chn_no);
void config_adc_timer(void);
void register_ad();

//!!!
// ensure the caller always provides s_vect matrix with exact size of n_chn X nr_samp
//!!!
int adc_get_data(int *chn, int n_chn, int **s_vect, int nr_samp);
int adc_get_data_cont(int **s_vect);

#endif /* MAIN_ADC_OP_H_ */
