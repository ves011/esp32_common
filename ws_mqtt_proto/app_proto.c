/*
 * app_proto.c
 *
 *  Created on: May 9, 2026
 *      Author: viorel_serbu
 */

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "app_proto.h"

static const char *TAG = "app-proto";

static inline int append(char *buf, int pos, int max, const char *s)
	{
    int n = strlen(s);
    if (pos + n >= max) return -1;
    memcpy(buf + pos, s, n);
    return pos + n;
	}

static void log_output_proto(app_proto_t *p)
	{
	ESP_LOGI(TAG, "version: %d", p->version);
	ESP_LOGI(TAG, "hdr_fields: %d", p->hdr_fields);
	ESP_LOGI(TAG, "payload_len: %d", p->payload_len);
	ESP_LOGI(TAG, "timestamp: %llu", p->timestamp);
	ESP_LOGI(TAG, "command: %s", p->command);
	for(int i = 0; i < p->nparams; i++)
		ESP_LOGI(TAG, "param[%d]: %s", i, p->params[i]);
	}
void build_app_proto_msg(app_proto_t *msg, char *command, int nparams, char **params, int payload_len, uint8_t *payload)
	{
	msg->version = PROTO_VERSION;
	
	}	
int build_app_proto(uint8_t *out_buf, int max_len, const app_proto_t *msg, int *out_len)
	{
    if (!out_buf || !msg || !out_len)
        return -1;

    char tmp[32];
    int pos = 0;

    // -------------------------
    // HEADER
    // -------------------------

    snprintf(tmp, sizeof(tmp), "%d", msg->version);
    pos = append((char *)out_buf, pos, max_len, tmp);
    if (pos < 0) 
    	return -2;
    out_buf[pos++] = SEP;

    snprintf(tmp, sizeof(tmp), "%d", msg->hdr_fields);
    pos = append((char *)out_buf, pos, max_len, tmp);
    if (pos < 0) 
    	return -2;
    out_buf[pos++] = SEP;

    snprintf(tmp, sizeof(tmp), "%u", (unsigned int)msg->payload_len);
    pos = append((char *)out_buf, pos, max_len, tmp);
    if (pos < 0) 
    	return -2;
    out_buf[pos++] = SEP;

    snprintf(tmp, sizeof(tmp), "%" PRIu64, msg->timestamp);
    pos = append((char *)out_buf, pos, max_len, tmp);
    if (pos < 0) 
    	return -2;
    out_buf[pos++] = SEP;

    pos = append((char *)out_buf, pos, max_len, msg->command);
    if (pos < 0) 
    	return -2;
    out_buf[pos++] = SEP;

    // -------------------------
    // PARAMETERS
    // -------------------------

    for (int i = 0; i < msg->nparams; i++)
    	{
        pos = append((char *)out_buf, pos, max_len, msg->params[i]);
        if (pos < 0) 
        	return -3;
        out_buf[pos++] = SEP;
    	}

    // -------------------------
    // PAYLOAD (optional)
    // -------------------------

    if (msg->payload_len > 0 && msg->payload)
    	{
        if (pos + msg->payload_len > max_len)
            return -4;

        memcpy(out_buf + pos, msg->payload, msg->payload_len);
        pos += msg->payload_len;
    	}

    *out_len = pos;
    out_buf[pos] = 0;
    return 0;
	}
	
int parse_app_proto(uint8_t *buf, int len, app_proto_t *out)
	{
    int ret = -1;
    char *p, *end;
    char *tokens[MAX_TOKENS];
    int ntokens = 0;

	if (!buf || !out || len <= 0)
        return ret;

	memset(out, 0, sizeof(*out));
	
    p = (char *)buf;
    end = (char *)buf + len;

    char *cur = p;
    int hdr_fields = -1;

    while (cur < end && ntokens < MAX_TOKENS)
		{
        tokens[ntokens++] = cur;
        char *sep = memchr(cur, SEP, end - cur);
        if (!sep)
            break;
        *sep = 0;
        cur = sep + 1;
        if (ntokens == 2) 
			{
            hdr_fields = atoi(tokens[1]);
            if (hdr_fields < 5 || hdr_fields > MAX_TOKENS)
				{
				ret = -2;
                break;
				}
			}

		if (hdr_fields > 0 && ntokens == hdr_fields)
            break;
		}

    if (ntokens >= 5 && ntokens == hdr_fields  && ntokens - 5 < MAX_PARAMS)
		{
		ret = 0;
        out->version     = atoi(tokens[0]);
        out->hdr_fields  = atoi(tokens[1]);
        out->payload_len = (uint32_t)strtoul(tokens[2], NULL, 10);
        out->timestamp   = strtoull(tokens[3], NULL, 10);
        out->command     = tokens[4];
		out->nparams = ntokens - 5;

		for (int i = 0; i < out->nparams; i++)
			out->params[i] = tokens[5 + i];
		char *payload_start = cur;
		if (out->payload_len > 0)
			{
			if ((payload_start + out->payload_len) <= end)
				out->payload = (uint8_t *)payload_start;
			else
				ret = -4;
			}
		else
			{
			out->payload = NULL;
			}
		}
    else
    	{
        ret = -3;
        }
	ESP_LOGI(TAG, "protocol parser result: %d", ret);
	if(ret == 0)
		log_output_proto(out);
    return ret;
	}

