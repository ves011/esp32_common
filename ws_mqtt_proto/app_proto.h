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
#include "protocoldef.h"

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