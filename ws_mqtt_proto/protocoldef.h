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
/* Progress indication */
#define PAR_PROGRESS                 "progress"

/* Error indication */
#define PAR_ERROR                    "error"

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

/* Responses */
/* Generic command response */
#define RSP_CMD                      "rsp_cmd"

/* Confirmation response */
#define RSP_CONFIRMATION             "rsp_conf"

/* Urc */
/* Unsolicited status message */
#define URC_STATUS                   "urc_status"

#endif /* PROTOCOLDEF_H_ */
