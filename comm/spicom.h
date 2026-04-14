/*
 * spicom.h
 *
 *  Created on: Nov 7, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_SPICOM_H_
#define MAIN_SPICOM_H_

void spiadmaster_init();
int spiad_rw(uint16_t cmd, int len_tx, uint16_t *data);
void register_spi();

#endif /* MAIN_SPICOM_H_ */
