/**
 * @file app_proto.h
 * @brief Lightweight ASCII header + binary payload application protocol.
 *
 * Frame format:
 *      <header><SEP><payload>
 *
 * All header fields are UTF-8 ASCII strings separated by SEP ('\1').
 * Payload is optional and may contain arbitrary binary data.
 *
 * Header format:
 *      version
 *      SEP hdr_fields
 *      SEP payload_len
 *      SEP timestamp
 *      SEP command
 *      SEP param1
 *      SEP ...
 *      SEP paramN
 *      SEP payload(optional)
 *
 * Example:
 *      1\18\132\11747000000\1set boot\1factory\1
 *
 * Notes:
 *  - hdr_fields includes ALL header fields including version.
 *  - Minimum valid hdr_fields value is 5.
 *  - payload_len specifies number of bytes after final separator.
 *  - Payload may contain binary data including '\0'.
 */

#ifndef APP_PROTO_H_
#define APP_PROTO_H_

#include <inttypes.h>

/** Header field separator */
#define SEP '\1'

/** Maximum number of parameters */
#define MAX_PARAMS         10

/** Maximum parsed tokens */
#define MAX_TOKENS         32

/** Protocol version */
#define PROTO_VERSION      1


/**
 * @brief Parsed application protocol frame.
 */
typedef struct
{
    /** Protocol version */
    int version;

    /** Number of header fields */
    uint8_t hdr_fields;

    /** Payload length in bytes */
    uint32_t payload_len;

    /** Message timestamp */
    uint64_t timestamp;

    /** Command string */
    char *command;

    /** Number of parameters */
    uint8_t nparams;

    /** Parameter array */
    char *params[MAX_PARAMS];

    /** Optional binary payload */
    uint8_t *payload;

} app_proto_t;


/* --------------------------------------------------------------------------
 * Common parameters
 * -------------------------------------------------------------------------- */

/** Progress indication */
#define PAR_PROGRESS       "progress"

/** Error indication */
#define PAR_ERROR          "error"


/* --------------------------------------------------------------------------
 * Operations
 * -------------------------------------------------------------------------- */

/**
 * Upload operation status
 *
 * params[0] = PAR_PROGRESS | PAR_ERROR
 * params[1] = error code
 * params[2] = progress percentage | error text
 */
#define OP_UPLOAD          "upload"

/**
 * Download operation status
 *
 * params[0] = PAR_PROGRESS | PAR_ERROR
 * params[1] = error code
 * params[2] = progress percentage | error text
 */
#define OP_DOWNLOAD        "download"

/**
 * Key update status
 *
 * params[0] = PAR_PROGRESS
 * params[1] = status/error code
 * params[2] = update identifier
 */
#define OP_UPDATEKEY       "update key"

/** Configuration operation */
#define OP_CONFIG          "configuration"


/* --------------------------------------------------------------------------
 * Commands
 * -------------------------------------------------------------------------- */

/**
 * Set boot partition.
 *
 * params[0] = partition name
 */
#define CMD_SETBOOT        "set boot"

/**
 * Erase partition.
 *
 * params[0] = partition name
 */
#define CMD_ERASEPART      "erase partition"

/**
 * Request confirmation.
 *
 * params[0] = command requiring confirmation
 * params[1] = command argument
 */
#define CMD_REQCONF        "request confirmation"

/**
 * Begin chunked key update.
 *
 * params[0] = namespace ID
 * params[1] = key ID
 * params[2] = key type
 * params[3] = total key length
 */
#define CMD_UPDATEKEYREQ   "update key req"

/**
 * Send key update chunk.
 *
 * params[0] = namespace ID
 * params[1] = key ID
 * params[2] = byte offset
 * params[3] = chunk length
 *
 * payload  = chunk binary data
 */
#define CMD_UPDATEKEYVAL   "update key val"

/**
 * Update key using single-frame payload.
 *
 * params[0] = namespace ID
 * params[1] = key ID
 * params[2] = key type
 *
 * payload = complete key value
 */
#define CMD_UPDATEKEY      "update key"

/**
 * Delete namespace.
 *
 * params[0] = namespace name
 */
#define CMD_DELTENS        "delete ns"

/**
 * Delete key.
 */
#define CMD_DELETEKEY      "delete key"

/**
 * Create key.
 *
 * params[0] = namespace name
 * params[1] = key name
 * params[2] = key type
 * params[3] = key length
 *
 * payload = initial value / placeholder
 */
#define CMD_CREATEKEY      "create key"

/**
 * Reboot ESP device.
 */
#define CMD_REBOOT         "esp_reboot"


/* --------------------------------------------------------------------------
 * Responses
 * -------------------------------------------------------------------------- */

/**
 * Generic command response.
 *
 * params[0] = original command
 * params[1] = command argument
 * params[2] = error code
 * params[3] = error text
 */
#define RSP_CMD            "rsp_cmd"

/**
 * Confirmation response.
 *
 * params[0] = original command
 * params[1] = command argument
 * params[2] = OK | KO
 */
#define RSP_CONFIRMATION   "rsp_conf"


/* --------------------------------------------------------------------------
 * Unsolicited responses
 * -------------------------------------------------------------------------- */

/**
 * Generic unsolicited status indication.
 *
 * params[0] = operation
 * params[1] = PAR_PROGRESS | PAR_ERROR
 * params[2] = progress percentage | error code
 * params[3] = text
 */
#define URC_STATUS         "urc_status"


/**
 * @brief Initialize application protocol message structure.
 *
 * @param[out] msg          Message structure
 * @param[in]  command      Command string
 * @param[in]  nparams      Number of parameters
 * @param[in]  params       Parameter array
 * @param[in]  payload_len  Payload length
 * @param[in]  payload      Optional payload
 */
void build_app_proto_msg(app_proto_t *msg,
                         char *command,
                         int nparams,
                         char **params,
                         int payload_len,
                         uint8_t *payload);


/**
 * @brief Build serialized protocol frame.
 *
 * @param[out] out_buf   Output buffer
 * @param[in]  max_len   Output buffer size
 * @param[in]  msg       Message to serialize
 * @param[out] out_len   Serialized frame length
 *
 * @return 0 on success, negative value on error
 */
int build_app_proto(uint8_t *out_buf,
                    int max_len,
                    const app_proto_t *msg,
                    int *out_len);


/**
 * @brief Parse serialized protocol frame.
 *
 * @param[in]  buf   Input buffer
 * @param[in]  len   Buffer length
 * @param[out] out   Parsed message
 *
 * @return 0 on success, negative value on error
 */
int parse_app_proto(uint8_t *buf,
                    int len,
                    app_proto_t *out);

#endif /* APP_PROTO_H_ */