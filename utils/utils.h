/*
 * utils.h
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */

#ifndef MAIN_UTILS_H_
#define MAIN_UTILS_H_

void my_printf(char *format, ...);
int my_log_vprintf(const char *fmt, va_list arguments);
void my_fputs(char *buf, FILE *f);
//int rw_params(int rw, int param_type, void * param_val);
int write_tpdata(int rw, char *bufdata);
int spiffs_storage_check();
int rw_console_state(int rw, console_state_t *cs);

#endif /* MAIN_UTILS_H_ */
