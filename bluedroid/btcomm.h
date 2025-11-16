/*
 * btcomm.h
 *
 *  Created on: Nov 13, 2023
 *      Author: viorel_serbu
 */

#ifndef COMM_BTCOMM_H_
#define COMM_BTCOMM_H_

#include "esp_bt_defs.h"
#include "freertos/idf_additions.h"
#include "inttypes.h"

#define NOTIFICATION_OK			1
#define INDICATION_OK			2

#define BLE_CLIENT					(1)
#define BLE_SERVER					(2)

#define BT_RAW_PDU					0xfff01
#define SENT_PDU					0x10
#define ECHO_PDU					0x20
#define ECHO_NONE					0	// no echo back
#define ECHO_8						1	// echo back first 8 bytes
#define ECHo_32						2	// echo back first 32 bytes

typedef struct
	{
	uint8_t	*prepare_buf;
	int     prepare_len;
	} prepare_type_env_t;

typedef struct
	{
	uint8_t dev_bda[6];
	esp_ble_addr_type_t bda_type;
	char dnamae[32];
	int in_use;
	}ble_dev_list_t;
	
// keep the same structure and size like socket_message_t
// it is using the same queues	
typedef struct
	{
	uint64_t ts;
	uint32_t id;
	uint16_t len;
	uint16_t flags;
	uint8_t pdu[192];
	}bt_raw_data_t;
	
extern uint32_t scan_d;
extern int n_scan_results;
extern ble_dev_list_t *ble_dev;
extern QueueHandle_t bt_receive_queue, bt_send_queue;
extern QueueHandle_t bt_client_receive_queue, bt_client_send_queue;
extern int btEnabled;

int btcomm_init();
void register_bt();
void start_bt(int role);
void get_advp();
void set_advp(int min, int max);
#endif /* COMM_BTCOMM_H_ */
