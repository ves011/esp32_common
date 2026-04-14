/*
 * utils.h
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_UTILS_H_
#define MAIN_UTILS_H_

#include <stddef.h>
#include <stdio.h>
#include "common_defines.h"

extern dev_config_t dev_conf;
extern char *nvs_cl_crt, *nvs_cl_key, *nvs_ca_crt;
extern size_t nvs_cl_crt_sz, nvs_ca_crt_sz, nvs_cl_key_sz;


int my_printf(char *format, ...);
int my_log_vprintf(const char *fmt, va_list arguments);
void my_fputs(char *buf, FILE *f);
int write_tpdata(int rw, char *bufdata);
int spiffs_storage_check();
int get_nvs_cert(char * entry_name, char **cert);
int get_all_nvscerts();
void my_esp_restart();
void get_nvs_conf();

#endif /* MAIN_UTILS_H_ */
