/*
 * i2ccomm.c
 *
 *  Created on: Jan 13, 2025
 *      Author: viorel_serbu
 */

//#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "driver/i2c_master.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "sys/stat.h"
#include "math.h"
#include "../main/project_specific.h"
#include "gpios.h"	
#include "i2ccomm.h"

static char *TAG = "I2C_app";

i2c_master_bus_handle_t i2c_bus_handle_0, i2c_bus_handle_1;

int init_i2c()
	{
	i2c_master_bus_config_t i2c_mst_config = 
		{
	    .clk_source = I2C_CLK_SRC_DEFAULT,
	    .i2c_port = 0,
	    .scl_io_num = I2C_MASTER_SCL_IO_0,
	    .sda_io_num = I2C_MASTER_SDA_IO_0,
	    .glitch_ignore_cnt = 7,
	    .flags.enable_internal_pullup = true
		};
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle_0));
	return ESP_OK;
	}
int master_transmit(i2c_master_dev_handle_t handle, uint8_t *wr_buf, int size)
	{
	int ret = i2c_master_transmit(handle, wr_buf, size, 50);
	if(ret != ESP_OK)
		ESP_LOGI(TAG, "i2c trasmit error: %d", ret);
	return ret;
	}
int master_receive(i2c_master_dev_handle_t handle, uint8_t *rd_buf, int size)
	{
	int ret = i2c_master_receive(handle, rd_buf, size, 50);
	if(ret != ESP_OK)
		ESP_LOGI(TAG, "i2c receive error: %d", ret);
	return ret;
	}

