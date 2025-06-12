/*
 * spicom.c
 *
 *  Created on: Nov 7, 2023
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include "project_specific.h"

#ifdef ADC_AD7811
#include "soc/gpio_reg.h"
#include "soc/dport_access.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_rom_sys.h"
#include "common_defines.h"

#include "gpios.h"
#include "spicom.h"

static const char* TAG = "SPI op:";

struct
	{
    struct arg_str *op;
    struct arg_str *arg;
    struct arg_str *arg1;
    struct arg_end *end;
	} spi_args;

static spi_device_handle_t spiad;

/*
 * pre transaction callback
 * used to toogle CONVST/RFS/TFS
 */
void spiad_pt_callback(spi_transaction_t *t)
	{
	uint32_t v = 1 << PIN_NUM_CONVST;
	DPORT_REG_WRITE(GPIO_OUT_W1TC_REG, v);
	esp_rom_delay_us(1);
	DPORT_REG_WRITE(GPIO_OUT_W1TS_REG, v);

	// approximately 3 usec delay to allow ad7811 to wake-up
	esp_rom_delay_us(1);
	}
void spiadmaster_init()
	{
	spi_bus_config_t buscfg=
		{
		.miso_io_num=PIN_NUM_MISO,
		.mosi_io_num=PIN_NUM_MOSI,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.data4_io_num = -1,
		.data5_io_num = -1,
		.data6_io_num = -1,
		.data7_io_num = -1
		};
	spi_device_interface_config_t devcfg=
		{
		.address_bits = 0,
		.command_bits = 0,
		.clock_speed_hz = 1*1000*1000,           	// Clock out at 1 MHz
		.mode = 1,                             		// SPI mode 1 CPOL = 0/CPHA = 1
		.spics_io_num = -1,      		         	// CS pin not used
		.queue_size = 1,                          	//
		.pre_cb = spiad_pt_callback,				// pre-transaction callback: toogle CONVST/RFS/TFS
	    };
	ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
	ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &spiad));
	}

int spiad_rw(uint16_t cmd, int len_tx, uint16_t *data)
	{
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
	t.length = len_tx;
	t.tx_data[1] = cmd & 0xff;
	t.tx_data[0] = cmd >> 8;
	spi_device_acquire_bus(spiad, portMAX_DELAY);
	ret=spi_device_polling_transmit(spiad, &t);  //start transaction
	spi_device_release_bus(spiad);
	if(ret != ESP_OK)
		ESP_LOGI(TAG, "Error starting transaction %d", ret);
	else
		{
		//ESP_LOGI(TAG, "SPI read length = %d, data[0] = %x, data[1] = %x, data[2] = %x, data[3] = %x",
		//		t.rxlength, t.rx_data[0], t.rx_data[1], t.rx_data[2], t.rx_data[3]);
		*data = (t.rx_data[0] << 8) & 0xff00;
		*data |= t.rx_data[1];
		*data = (*data >> 6) & 0x3ff;
		}
	return ret;
	}
static int do_spi(int argc, char **argv)
	{
	int len;
	uint16_t data = 0x3040, rdata[4];
	int nerrors = arg_parse(argc, argv, (void **)&spi_args);
	if (nerrors != 0)
		{
		arg_print_errors(stderr, spi_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(strcmp(spi_args.op->sval[0], "rw") == 0)
		{
		if(spi_args.arg->count)
			len = atoi(spi_args.arg->sval[0]);
		else
			len = 13;
		if(spi_args.arg1->count)
			data = strtol(spi_args.arg1->sval[0], NULL, 16);
		spiad_rw(data, len, rdata);
		}
	return ESP_OK;
	}
void register_spi()
	{
	spiadmaster_init();
	spi_args.op = arg_str1(NULL, NULL, "<op>", "op: r | w");
	spi_args.arg= arg_str0(NULL, NULL, "<arg>", "length");
	spi_args.arg1= arg_str0(NULL, NULL, "<arg>", "cmd to send");
	spi_args.end = arg_end(1);
	const esp_console_cmd_t spi_cmd =
		{
		.command = "spi",
		.help = "rotate steering motor - cw or ccw - by number of steps",
		.hint = NULL,
		.func = &do_spi,
		.argtable = &spi_args
		};
	ESP_ERROR_CHECK(esp_console_cmd_register(&spi_cmd));
	}
#endif
