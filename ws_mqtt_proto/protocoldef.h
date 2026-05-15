/*
 * AUTO-GENERATED FILE
 * DO NOT EDIT MANUALLY
 */

#ifndef PROTOCOLDEF_H_
#define PROTOCOLDEF_H_

/* Application protocol used between ESP32 backend and browser frontend */
#define PROTO_VERSION             1
#define SEP                       '\1'
#define MAX_PARAMS                10
#define MAX_TOKENS                32

/* Parameters */
/* Progress indication - used by URC_STATUS */
#define PAR_PROGRESS                 "progress"

/* Error indication - used by URC_STATUS */
#define PAR_ERROR                    "error"

/* AP IP - used by URC_DEVINFO */
#define PAR_APIP                     "ap_ip"

/* STA IP or NA - used by URC_DEVINFO */
#define PAR_STAIP                    "sta_ip"

/* STA SSID or NA - used by URC_DEVINFO */
#define PAR_STASSID                  "sta_ssid"

/* STA RSSI or NA - used by URC_DEVINFO */
#define PAR_STARSSI                  "sta_rssi"

/* device time (retuned by time()) - used by URC_DEVINFO */
#define PAR_DEVTIME                  "dev_time"

/* device internal parameter
should not really be here
	  - used to generate WifFi state messages */
#define PAR_WIFI                     "check_wifi"

/* Operations */
/* Upload operation status */
#define OP_UPLOAD                    "upload"

/* Download operation status */
#define OP_DOWNLOAD                  "download"

/* NVS key update status */
#define OP_UPDATEKEY                 "update key"

/* Configuration related operation */
#define OP_CONFIG                    "configuration"

/* Commands */
/* Set boot partition */
#define CMD_SETBOOT                  "set boot"

/* Erase partition contents */
#define CMD_ERASEPART                "erase partition"

/* Request confirmation from frontend */
#define CMD_REQCONF                  "request confirmation"

/* Initialize chunked NVS key update */
#define CMD_UPDATEKEYREQ             "update key req"

/* Transfer one key data chunk */
#define CMD_UPDATEKEYVAL             "update key val"

/* Single frame NVS key update */
#define CMD_UPDATEKEY                "update key"

/* Delete namespace */
#define CMD_DELTENS                  "delete ns"

/* Delete NVS key */
#define CMD_DELETEKEY                "delete key"

/* Create new NVS key */
#define CMD_CREATEKEY                "create key"

/* Restart ESP device */
#define CMD_REBOOT                   "esp_reboot"

/*
 * request device to synchronize its internal time with frontend
 * useful when device has no access to internet time
 * no parameters: embedded side will use [ts] field in the header
 */
#define CMD_SYNCTIME                 "sync_tyme"

/* Responses */
/* Generic command response */
#define RSP_CMD                      "rsp_cmd"

/* Confirmation response */
#define RSP_CONFIRMATION             "rsp_conf"

/* Urcs */
/* Unsolicited status message */
#define URC_STATUS                   "urc_status"

/* unsolicited device info message */
#define URC_DEVINFO                  "urc_devinfo"

#endif /* PROTOCOLDEF_H_ */
