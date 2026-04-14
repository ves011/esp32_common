/*
 * ota.h
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */

#ifndef COMMON_UTILS_OTA_H_
#define COMMON_UTILS_OTA_H_

extern TaskHandle_t ota_task_handle;;
#define HASH_LEN 32 /* SHA-256 digest length */

#define NOT_FOUND_TAG "404 Not Found"

void print_sha256 (const uint8_t *image_hash, const char *label);
void ota_task(const char *url);

#endif /* COMMON_UTILS_OTA_H_ */
