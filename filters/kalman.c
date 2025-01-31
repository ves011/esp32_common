/*
 * kalman.c
 *
 *  Created on: Jan 28, 2025
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include "math.h"
#include "kalman.h"


/*
Kalman simple filter
*/
void ksf_init(float m_error, float e_error, float pn, ks_filter_t *kf)
	{
	kf->err_m = m_error;
	kf->err_e = e_error;
	kf->p_noise = pn;
	kf->l_est = kf->k_gain = 0;	
	}

float ksf_update_est(float m, ks_filter_t *kf)
	{
  	kf->k_gain = kf->err_e / (kf->err_e + kf->err_m);
  	float c_est = kf->l_est + kf->k_gain * (m - kf->l_est);
  	kf->err_e = (1.0f - kf->k_gain) * kf->err_e + fabsf(kf->l_est - c_est) * kf->p_noise;
  	kf->l_est = c_est;
  	return c_est;
	}
