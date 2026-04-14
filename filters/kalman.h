/*
 * kalman.h
 *
 *  Created on: Jan 28, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_FILTERS_KALMAN_H_
#define ESP32_COMMON_FILTERS_KALMAN_H_

//180 / PI
#define RAD_TO_DEG 57.29577951308233f 


typedef struct 
	{
	float err_m, err_e, p_noise;
	float l_est, k_gain;
	} ks_filter_t;

void ksf_init(float m_error, float e_error, float pn, ks_filter_t *kf);
float ksf_update_est(float m, ks_filter_t *kf);

void sensor_fusion_init();
void MadgwickUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz, float deltat);
void MadgwickUpdate_nom(float gx, float gy, float gz, float ax, float ay, float az, float deltat);
void MahonyUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz, float deltat);
float get_yaw();
float get_roll();
float get_pitch();


#endif /* ESP32_COMMON_FILTERS_KALMAN_H_ */
