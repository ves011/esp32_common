/*
 * ad7811.h
 *
 *  Created on: Nov 30, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_AD7811_H_
#define MAIN_AD7811_H_

/*
 * using simplified serial if with SPI2_HOST
 *
 *			SPI				   AD7811
 *			-------------------------
 * 			MISO (IO9) -------DOUT
 * 			MOSI (IO11)-------DIN
 * 			CS --x
 * 			SCLK (IO12)-------SCLK
 * 						   |--CONVST
 * 			CONVST (IO6)---|--RFS
 * 						   |--TFS
 *
 */

#define EXTREF					3300			//external VRef
//#define INTREF				2500			//internal VRef
#define NR_SAMPLES_PC			200
#define NR_SAMPLES_PS			10
#define NR_SAMPLES_DV			10
#define SAMPLE_PERIOD			500				//ADC sampling rate (us)


typedef struct
	{
	uint8_t source;
	uint8_t step;
	uint32_t val;
	uint64_t ts;
	} adc_msg_t;

void config_gpios();
void register_ad();
int adc_get_data(int chn, int16_t *s_vect, int nr_samp);
int do_ad(int argc, char **argv);

#endif /* MAIN_AD7811_H_ */
