# app_proto

Application protocol used between ESP32 backend and browser frontend

## Protocol

- Version: `1`
- Separator: `0x01`
- Max params: `10`
- Max tokens: `32`

## Parameters

### PAR_PROGRESS

- Value: `progress`
- Description: Progress indication - used by URC_STATUS

### PAR_ERROR

- Value: `error`
- Description: Error indication - used by URC_STATUS

### PAR_APIP

- Value: `ap_ip`
- Description: AP IP - used by URC_DEVINFO

### PAR_STAIP

- Value: `sta_ip`
- Description: STA IP or NA - used by URC_DEVINFO

### PAR_STASSID

- Value: `sta_ssid`
- Description: STA SSID or NA - used by URC_DEVINFO

### PAR_STARSSI

- Value: `sta_rssi`
- Description: STA RSSI or NA - used by URC_DEVINFO

### PAR_DEVTIME

- Value: `dev_time`
- Description: device time (retuned by time()) - used by URC_DEVINFO

### PAR_WIFI

- Value: `check_wifi`
- Description: device internal parameter
should not really be here
	  - used to generate WifFi state messages

## Operations

### OP_UPLOAD

- Value: `upload`
- Description: Upload operation status

| Parameter | Type | Description |
|---|---|---|
| status | - | - |
| error_code | - | - |
| progress_or_text | - | - |

### OP_DOWNLOAD

- Value: `download`
- Description: Download operation status

| Parameter | Type | Description |
|---|---|---|
| status | - | - |
| error_code | - | - |
| progress_or_text | - | - |

### OP_UPDATEKEY

- Value: `update key`
- Description: NVS key update status

| Parameter | Type | Description |
|---|---|---|
| status | - | - |
| status_code | - | - |
| update_identifier | - | - |

### OP_CONFIG

- Value: `configuration`
- Description: Configuration related operation

## Commands

### CMD_SETBOOT

- Value: `set boot`
- Description: Set boot partition
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| partition | string | Partition label |

### CMD_ERASEPART

- Value: `erase partition`
- Description: Erase partition contents
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| partition | string | Partition label |

### CMD_REQCONF

- Value: `request confirmation`
- Description: Request confirmation from frontend
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| command | string | Command requiring confirmation |
| argument | string | Associated command argument |

### CMD_UPDATEKEYREQ

- Value: `update key req`
- Description: Initialize chunked NVS key update
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| namespace_id | int | Namespace index |
| key_id | int | Key index |
| key_type | int | NVS key type |
| total_length | int | Total update length |

### CMD_UPDATEKEYVAL

- Value: `update key val`
- Description: Transfer one key data chunk
- Payload: `True`
- Payload type: `binary`

| Parameter | Type | Description |
|---|---|---|
| namespace_id | int | Namespace index |
| key_id | int | Key index |
| offset | int | Offset inside destination buffer |
| chunk_length | int | Chunk length in bytes |

### CMD_UPDATEKEY

- Value: `update key`
- Description: Single frame NVS key update
- Payload: `True`
- Payload type: `binary`

| Parameter | Type | Description |
|---|---|---|
| namespace_id | int | Namespace index |
| key_id | int | Key index |
| key_type | int | NVS key type |

### CMD_DELTENS

- Value: `delete ns`
- Description: Delete namespace
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| namespace_name | string | Namespace name |

### CMD_DELETEKEY

- Value: `delete key`
- Description: Delete NVS key
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| namespace_id | int | Namespace index |
| key_id | int | Key index |

### CMD_CREATEKEY

- Value: `create key`
- Description: Create new NVS key
- Payload: `True`
- Payload type: `binary`

| Parameter | Type | Description |
|---|---|---|
| namespace_name | string | Namespace name |
| key_name | string | Key name |
| key_type | int | NVS key type |
| key_length | int | Key length |

### CMD_REBOOT

- Value: `esp_reboot`
- Description: Restart ESP device
- Payload: `False`

### CMD_SYNCTIME

- Value: `sync_tyme`
- Description:
  - request device to synchronize its internal time with frontend
  - useful when device has no access to internet time
  - no parameters: embedded side will use [ts] field in the header
- Payload: `False`

## Responses

### RSP_CMD

- Value: `rsp_cmd`
- Description: Generic command response
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| command | string | Responded command |
| argument | string | Primary command argument |
| error_code | int | ESP-IDF or application error code |
| error_text | string | Human readable error text |

### RSP_CONFIRMATION

- Value: `rsp_conf`
- Description: Confirmation response
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| command | string | Confirmed command |
| argument | string | Associated argument |
| answer | string | Confirmation result |

## Urcs

### URC_STATUS

- Value: `urc_status`
- Description: Unsolicited status message
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| operation | string | Operation type: OP_UPLOAD | OP_DOWNLOAD | ... |
| PAR_PROGRESS | PAR_ERROR | string | type of status |
| code | int | Error or status code |
| text | string | Human readable status text |

### URC_DEVINFO

- Value: `urc_devinfo`
- Description: unsolicited device info message
- no of header fields: 7
- Payload: `False`

| Parameter | Type | Description |
|---|---|---|
| type of info in params[1] | string |  one of PAR_APIP .. PAR_DEVTIME |
| info | string | depends on params[0] |
