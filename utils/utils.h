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
int rw_params(int rw, int param_type, void * param_val);

#endif /* MAIN_UTILS_H_ */
