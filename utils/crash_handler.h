/*
 * crash_handler.h
 *
 *  Created on: Sep 23, 2026
 *      Author: viorel_serbu
 */

#ifndef ESP32_COMMON_UTILS_CRASH_HANDLER_H_
#define ESP32_COMMON_UTILS_CRASH_HANDLER_H_



#include "esp_private/panic_internal.h"
#include "esp_system.h"
#include <stdint.h>
#include "soc/soc.h"
#include "time.h"

typedef enum 
	{
	CRASH_NONE = 0,
	CRASH_ABORT,
	CRASH_EXCEPTION,
	CRASH_STACK_OVERFLOW,
	CRASH_WDT,
	CRASH_BROWNOUT
	} crash_type_t;

typedef struct 
	{
    uint32_t magic;
    uint32_t boot_count;
    time_t crash_time;
    time_t boot_time;
    uint32_t crash_hb;
    uint32_t boot_hb;
    esp_reset_reason_t reset_reason;
    esp_err_t err;
    crash_type_t  crash_type;
    uint32_t line;
    char file[32];
    char func[32];
    char task[16];
    
    uint32_t panic_core;
    panic_exception_t  panic_exception;
    uint32_t panic_pseudo_excause;
    uintptr_t panic_addr;

    char panic_reason[32];
    char panic_description[64];
    
	} crash_record_t;

#define CRASH_ERROR_CHECK(x)                       \
do {                                               \
    esp_err_t __err = (x);                         \
    if (__err != ESP_OK)                           \
    	{                                          \
        g_crash->reset_reason = 0;                 \
		g_crash->task[0] = 0;                      \
		g_crash->panic_exception = 0;              \
		g_crash->panic_addr = 0;                   \
		g_crash->panic_core = 0;                   \
		g_crash->panic_pseudo_excause = 0;         \
		g_crash->panic_reason[0] = 0;              \
		g_crash->panic_description[0] = 0;         \
        g_crash->crash_time = g_crash->boot_time;  \
	    g_crash->crash_hb = g_crash->boot_hb;      \
    	g_crash->crash_type = CRASH_ABORT;         \
        g_crash->err = __err;                      \
        g_crash->line = __LINE__;                  \
        strlcpy((char *)g_crash->file,             \
                __FILE__,                          \
                sizeof(g_crash->file));            \
        strlcpy((char *)g_crash->func,             \
                __func__,                          \
                sizeof(g_crash->func));            \
    	g_crash->magic = CRASH_MAGIC;              \
        _esp_error_check_failed(                   \
            __err, __FILE__, __LINE__,             \
            __ASSERT_FUNC, #x);                    \
    	}                                          \
} while(0)

#define CRASH_MAGIC 			0x43525348  // "CRSH"

#define PRODUCTION_PART_NAME		"ota_0"
#define RECOVERY_PART_NAME			"ota_1"
	
#define CRASH_RTC_SIZE  0x100
#define CRASH_RTC_PLATFORM_RESERVED_SIZE 0x18
#define CRASH_RTC_END   (SOC_RTC_DATA_HIGH - CRASH_RTC_PLATFORM_RESERVED_SIZE)
#define CRASH_RTC_START (CRASH_RTC_END - CRASH_RTC_SIZE)

#define HEALTH_BOOT_TIME	10000000	// 10 seconds

#define g_crash ((volatile crash_record_t *)CRASH_RTC_START)	

void init_crash_info(bool force);
void clear_crash_info();	
void checkLastCrash();
void logCrashInfo();


#endif /* ESP32_COMMON_UTILS_CRASH_HANDLER_H_ */
