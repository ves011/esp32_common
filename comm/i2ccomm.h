/*
 * i2ccomm.h
 *
 *  Created on: Jan 13, 2025
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_COMM_I2CCOMM_H_
#define ESP32_COMMON_COMM_I2CCOMM_H_


int init_i2c(int bus_no);
int master_transmit(i2c_master_dev_handle_t handle, uint8_t *wr_buf, int size);
int master_receive(i2c_master_dev_handle_t handle, uint8_t *rd_buf, int size);
int master_transmit_receive(i2c_master_dev_handle_t handle, uint8_t *wr_buf, int wr_size, uint8_t *rd_buf, int rd_size);
int i2c_probe_device(int bus_no, uint16_t dev_addr);
i2c_master_dev_handle_t i2c_add_device(int bus_no, uint16_t dev_address);


#endif /* ESP32_COMMON_COMM_I2CCOMM_H_ */
