/*
 * i2ccomm.h
 *
 *  Created on: Jan 13, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_COMM_I2CCOMM_H_
#define ESP32_COMMON_COMM_I2CCOMM_H_


int init_i2c();
int master_transmit(i2c_master_dev_handle_t handle, uint8_t *wr_buf, int size);
int master_receive(i2c_master_dev_handle_t handle, uint8_t *rd_buf, int size);


#endif /* ESP32_COMMON_COMM_I2CCOMM_H_ */
