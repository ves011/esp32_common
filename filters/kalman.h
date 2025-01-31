/*
 * kalman.h
 *
 *  Created on: Jan 28, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_FILTERS_KALMAN_H_
#define ESP32_COMMON_FILTERS_KALMAN_H_


typedef struct 
	{
	float err_m, err_e, p_noise;
	float l_est, k_gain;
	} ks_filter_t;

void ksf_init(float m_error, float e_error, float pn, ks_filter_t *kf);
float ksf_update_est(float m, ks_filter_t *kf);

#endif /* ESP32_COMMON_FILTERS_KALMAN_H_ */
