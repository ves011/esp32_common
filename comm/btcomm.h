/*
 * btcomm.h
 *
 *  Created on: Nov 13, 2023
 *      Author: viorel_serbu
 */

#ifndef COMM_BTCOMM_H_
#define COMM_BTCOMM_H_

#define NOTIFICATION_OK			1
#define INDICATION_OK			2

typedef struct
	{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
	}gatts_profile_inst_t;

typedef struct
	{
	uint8_t	*prepare_buf;
	int     prepare_len;
	} prepare_type_env_t;

int btcomm_init();

#endif /* COMM_BTCOMM_H_ */
