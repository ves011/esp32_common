/*
 * gpios.h
 *
 *  Created on: Jan 28, 2024
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_GPIOS_H_
#define ESP32_COMMON_GPIOS_H_

#include "project_specific.h"
#include "common_defines.h"


#if ACTIVE_CONTROLLER == WP_CONTROLLER //based on esp32 dev kit
//ctrl GPIOs
/** DV control pin */
	/* A1 ON 	/ B1 OFF 	--> open
	 * A1 OFF 	/ B1 ON 	--> close
	 * A1 OFF 	/ B1 OFF 	--> inactive
	 */
//ADC channels provided by AD7811 channels
	#define PUMP_ONOFF_PIN			(23)	//output - pump on/off relay cmd
#ifdef WP_HW_V1
	
	#define SENSOR_CHN				(2)		//VIN3 - pressure sensor
	#define CURRENT_CHN				(0)		//VIN1 - pump current ACS712
	#define MOTSENSE_CHN			(1)		//VIN2 - DV current sense
	#define PINEN_DV0				(16)	//output - dv open/close cmd goes to dv0
	#define PINEN_DV1				(4)		//output - dv open/close cmd goes to dv1
	#define PINMOT_A1				(2)		//output - H bridge cmd
	#define PINMOT_B1				(15)	//output - H bridge cmd
	#define PIN_NUM_CONVST			(14)		//
	#define PIN_NUM_QMETER			(34)		//

//SPI communication AD7811
	#define PIN_NUM_MISO			(27)
	#define PIN_NUM_MOSI			(26)
	#define PIN_NUM_CLK				(25)


//rotary encoder
	#define ROT_ENC_S1				(35)
	#define ROT_ENC_S2				(32)
	#define ROT_ENC_KEY				(33)
	
#elifdef WP_HW_V2
	#define SENSOR_CHN				(2)		//VIN3 - pressure sensor
	#define CURRENT_CHN				(1)		//VIN2 - pump current ACS712
	#define MOTSENSE_CHN			(3)		//VIN4 - DV current sense
	#define PINEN_DV0				(16)	//output - dv open/close cmd goes to dv0
	#define PINEN_DV1				(15)	//output - dv open/close cmd goes to dv1
	#define PINEN_DV2				(14)	//output - dv open/close cmd goes to dv2
	#define PINEN_DV3				(13)	//output - dv open/close cmd goes to dv3
	#define PINMOT_A1				(4)		//output - H bridge cmd
	#define PINMOT_B1				(2)		//output - H bridge cmd
	#define PIN_NUM_CONVST			(26)	//
	#define PIN_NUM_QMETER			(36)	//

//SPI communication AD7811
	#define PIN_NUM_MISO			(25)
	#define PIN_NUM_MOSI			(33)
	#define PIN_NUM_CLK				(32)
//rotary encoder
	#define ROT_ENC_S1				(39)
	#define ROT_ENC_S2				(34)
	#define ROT_ENC_KEY				(35)
#endif

//LEDs
#ifdef LEDS
	#define PUMP_ONOFF_LED			(16)	//j4/3
	#define PUMP_ONLINE_LED			(21)	//j4/1
	#define PUMP_FAULT_LED			(4)		//j4/2
	#define DV0_ON_LED				(2)
	#define DV1_ON_LED				(15)
	#define PROG_ON_LED				(8)
	#define PUMP_ONLINE_CMD			(22)	//input - pump online/ofline push button
#endif
#endif

#if ACTIVE_CONTROLLER == PUMP_CONTROLLER
	/** Pressure sensor */
	#define SENSOR_PIN				(5)	//j5/5
	 /** ACS712 current sensor */
	#define CURRENT_PIN				(4)	//j5/6 --> hardwired
	 /** Pump ON/OF control pin */
	#define PUMP_ONOFF_PIN			(6)	//j5/8 --> hardwired

	#define PUMP_ONOFF_LED			(1)	//j4/3
	#define PUMP_ONLINE_LED			(8)//j4/1

	#define PUMP_FAULT_LED			(3)//j4/2
	#define PUMP_ONLINE_CMD			(7)	//j5/7 --> hardwired / not used if single relay populated
#endif

#if ACTIVE_CONTROLLER == WATER_CONTROLLER
	#define PINEN_DV0				(6)
	#define PINEN_DV1				(7)
	/* A1 ON 	/ B1 OFF 	--> open
	 * A1 OFF 	/ B1 ON 	--> close
	 * A1 OFF 	/ B1 OFF 	--> inactive
	 */
	#define PINMOT_A1				(8)
	#define PINMOT_B1				(3)
	/** current sense pin for actuators */
	#define PINSENSE_MOT			(4)
#endif

#if ACTIVE_CONTROLLER == WESTA_CONTROLLER
	#define DHT_DATA_PIN 			5
	#define I2C_MASTER_SCL_IO_0     4      					/*!< GPIO number used for I2C master clock */
	#define I2C_MASTER_SDA_IO_0     1      					/*!< GPIO number used for I2C master data  */
	#define I2C_MASTER_SCL_IO_1     99     					/*!< GPIO number used for I2C master clock */
	#define I2C_MASTER_SDA_IO_1     99    					/*!< GPIO number used for I2C master data  */
	#define RG_GPIO					7
	#define DS18B20_PIN 			6
#endif

#if ACTIVE_CONTROLLER == ESP32_TEST
	#define I2C_MASTER_SCL_IO       4      					/*!< GPIO number used for I2C master clock */
	#define I2C_MASTER_SDA_IO       5      					/*!< GPIO number used for I2C master data  */
	#define HMC_DRDY_PIN			6
#endif

#if ACTIVE_CONTROLLER == NAVIGATOR
	#define I2C_MASTER_SCL_IO_0     16      					/*!< GPIO number used for I2C master clock */
	#define I2C_MASTER_SDA_IO_0     15      					/*!< GPIO number used for I2C master data  */
	#define HMC_DRDY_PIN			17
	#define MPU_DRDY_PIN			18
	#define NW_CONNECT_ON			9
	#define NW_CONNECT_OFF			11
	#define REMOTE_CONNECT_ON		46
	#define REMOTE_CONNECT_OFF		10
#endif

#ifdef ADC_AD7811
	#if ACTIVE_CONTROLLER == FLOOR_HC
		#define PIN_NUM_CONVST		11		//
	//SPI communication AD7811
		#define PIN_NUM_MISO		12
		#define PIN_NUM_MOSI		3
		#define PIN_NUM_CLK			8
	#endif
#endif

#endif /* ESP32_COMMON_GPIOS_H_ */
