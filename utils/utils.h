/*
 * utils.h
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_UTILS_H_
#define MAIN_UTILS_H_

extern dev_config_t dev_conf;
extern char *nvs_cl_crt, *nvs_cl_key, *nvs_ca_crt;
extern size_t nvs_cl_crt_sz, nvs_ca_crt_sz, nvs_cl_key_sz;


void my_printf(char *format, ...);
int my_log_vprintf(const char *fmt, va_list arguments);
void my_fputs(char *buf, FILE *f);
//int rw_params(int rw, int param_type, void * param_val);
int write_tpdata(int rw, char *bufdata);
int spiffs_storage_check();
//int rw_console_state(int rw, console_state_t *cs);
int rw_dev_config(int rw);
int get_nvs_cert(char * entry_name, char **cert);
int get_all_nvscerts();
void my_esp_restart();

#endif /* MAIN_UTILS_H_ */
