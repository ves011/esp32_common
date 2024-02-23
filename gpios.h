/*
 * gpios.h
 *
 *  Created on: Jan 28, 2024
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_GPIOS_H_
#define ESP32_COMMON_GPIOS_H_

#if ACTIVE_CONTROLLER == WP_CONTROLLER //based on esp32 dev kit
//ADC channels provided by AD7811 channels
	#define SENSOR_CHN			1		//VIN2
	#define CURRENT_CHN			0		//VIN1
	#define MOTSENSE_CHN		2		//VIN3

	#define PIN_NUM_CONVST		26		//

//SPI communication
	#define PIN_NUM_MISO		32
	#define PIN_NUM_MOSI		33
	#define PIN_NUM_CLK			25
//push buttons

//LEDs


	#define PUMP_ONOFF_LED			(16)	//j4/3
	#define PUMP_ONLINE_LED			(21)//j4/1
	#define PUMP_FAULT_LED			(4)//j4/2
	#define DV0_ON_LED				(2)
	#define DV1_ON_LED				(15)


//ctrl GPIOs
/** Pump ON/OF control pin */
	#define PUMP_ONOFF_PIN			(23)	//output - pump on/off relay cmd
	#define PUMP_ONLINE_CMD			(34)	//input - pump online/ofline push button
	#define PINEN_DV0				(19)	//output - dv open/close cmd goes to dv0
	#define PINEN_DV1				(18)	//output - dv open/close cmd goes to dv1
	/* A1 ON 	/ B1 OFF 	--> open
	 * A1 OFF 	/ B1 ON 	--> close
	 * A1 OFF 	/ B1 OFF 	--> inactive
	 */
	#define PINMOT_A1				(5)		//output - H bridge cmd
	#define PINMOT_B1				(17)	//output - H bridge cmd
	#define DV0_CMD					(36)	//input - dv0 open/close push button
	#define DV1_CMD					(39)	//input - dv1 open/close push button
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


#endif /* ESP32_COMMON_GPIOS_H_ */
