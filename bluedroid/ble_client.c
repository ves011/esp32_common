/*
 * ble_client.c
 *
 *  Created on: Jul 1, 2025
 *      Author: viorel_serbu
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_bt_defs.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "ble_client.h"
#include "btcomm.h"

static char* TAG = "GATTC_op";
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static gattc_profile_inst_t gl_profile_tab[CLIENT_PROFILE_NUM] =
	{
	[CLIENT_PROFILE_A_APP_ID] = 
		{
		.gattc_cb = gattc_profile_event_handler, 
		.gattc_if = ESP_GATT_IF_NONE,				
		},       
	};
	/*
	[CLIENT_PROFILE_B_APP_ID] = 
		{
		.gattc_cb = gatts_profile_b_event_handler, 
		.gattc_if = ESP_GATT_IF_NONE,
		},
	};
	*/
