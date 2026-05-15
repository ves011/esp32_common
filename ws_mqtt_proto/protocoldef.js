// AUTO-GENERATED FILE
// DO NOT EDIT MANUALLY

// Protocol
const PROTO_VERSION = 1;
const SEP = "\x01";
const MAX_PARAMS = 10;
const MAX_TOKENS = 32;

// Parameters
// Progress indication
const PAR_PROGRESS = "progress";

// Error indication
const PAR_ERROR = "error";

// Operations
// Upload operation status
const OP_UPLOAD = "upload";

// Download operation status
const OP_DOWNLOAD = "download";

// NVS key update status
const OP_UPDATEKEY = "update key";

// Configuration related operation
const OP_CONFIG = "configuration";

// Commands
// Set boot partition
const CMD_SETBOOT = "set boot";

// Erase partition contents
const CMD_ERASEPART = "erase partition";

// Request confirmation from frontend
const CMD_REQCONF = "request confirmation";

// Initialize chunked NVS key update
const CMD_UPDATEKEYREQ = "update key req";

// Transfer one key data chunk
const CMD_UPDATEKEYVAL = "update key val";

// Single frame NVS key update
const CMD_UPDATEKEY = "update key";

// Delete namespace
const CMD_DELTENS = "delete ns";

// Delete NVS key
const CMD_DELETEKEY = "delete key";

// Create new NVS key
const CMD_CREATEKEY = "create key";

// Restart ESP device
const CMD_REBOOT = "esp_reboot";

// Responses
// Generic command response
const RSP_CMD = "rsp_cmd";

// Confirmation response
const RSP_CONFIRMATION = "rsp_conf";

// Urc
// Unsolicited status message
const URC_STATUS = "urc_status";
