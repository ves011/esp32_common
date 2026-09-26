/*
 * crash_handler.c
 *
 *  Created on: Sep 23, 2026
 *      Author: viorel_serbu
 */

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "freertos/idf_additions.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_private/panic_internal.h"
#include "heap_memory_layout.h"
#include "utils.h"
#include "crash_handler.h"


SOC_RESERVE_MEMORY_REGION(CRASH_RTC_START, CRASH_RTC_END, crash);
static const char *TAG = "LAST_CRASH";


extern void __real_esp_panic_handler(panic_info_t *info);

static void crash_copy_string(volatile char *dst, size_t dst_size, const char *src)
	{
	if (dst_size == 0) return;

	if(src == NULL){*dst = '\0'; return;}
	
    while (--dst_size && *src)
        *dst++ = *src++;

    *dst = '\0';
	}

void __wrap_esp_panic_handler(panic_info_t *info)
	{
    g_crash->crash_type = CRASH_EXCEPTION;
    g_crash->panic_exception = info->exception;
    g_crash->panic_addr = (uintptr_t)info->addr;
    g_crash->panic_core = info->core;
    g_crash->panic_pseudo_excause = info->pseudo_excause;
    crash_copy_string(g_crash->panic_reason, sizeof(g_crash->panic_reason), info->reason);
    crash_copy_string(g_crash->panic_description, sizeof(g_crash->panic_description), info->description);
    g_crash->magic = CRASH_MAGIC;
	//esp_rom_printf("\n*** CUSTOM PANIC WRAPPER ***\n");
    __real_esp_panic_handler(info);
	}

void vApplicationStackOverflowHook(TaskHandle_t task,  char *task_name)
	{
	static char buf[128];
	strcpy(buf, "***ERROR*** A stack overflow in task ");
    strlcat(buf, task_name, sizeof(buf));
    strlcat(buf, " has been detected.", sizeof(buf));
    
    strlcpy((char *)g_crash->task, task_name, sizeof(g_crash->task));
    g_crash->crash_type = CRASH_STACK_OVERFLOW;
    g_crash->magic = CRASH_MAGIC;
	snprintf(buf, sizeof(buf), "***ERROR*** A stack overflow in task %s  has been detected.", task_name);
    esp_system_abort(buf);
	}
	
static void boot_timer_callback(void* arg)
	{
	g_crash->boot_count = 0;
	}
void checkLastCrash()
	{
	if(g_crash->magic != CRASH_MAGIC) //first boot after power on
		{
		memset((void *)g_crash, 0, sizeof(crash_record_t));
		g_crash->reset_reason = esp_reset_reason();
		g_crash->boot_count = 0;
		g_crash->boot_time_us = esp_timer_get_time();
		g_crash->magic = CRASH_MAGIC;
		}
	else
		{
		if(g_crash->boot_count) //reboot to recovery mode
			{
			g_crash->reset_reason = esp_reset_reason();
			if(g_crash->reset_reason == ESP_RST_PANIC ||
				g_crash->reset_reason == ESP_RST_INT_WDT ||    
    			g_crash->reset_reason == ESP_RST_TASK_WDT ||  
    			g_crash->reset_reason == ESP_RST_WDT || 
    			g_crash->reset_reason == ESP_RST_CPU_LOCKUP)
				{
				const esp_partition_t *running = esp_ota_get_running_partition();
				if(!strcmp(running->label, PRODUCTION_PART_NAME))
					{
					const esp_partition_t *np = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, RECOVERY_PART_NAME);
					if(np)
						{
						int err = esp_ota_set_boot_partition(np);
						if(err == ESP_OK)
							RESTART("Restart into recovery mode");
						else
							ESP_LOGE("RECOVERY", "Cannot set boot flag on recovery partition");
						}
					else
						ESP_LOGE("RECOVERY", "Recovery partition not found");
					}
				g_crash->boot_count = 0;
				RESTART("g_crash->boot_count > 0 and invalid recovery");
				}
			}
		else
			{
			static esp_timer_handle_t boot_timer;
			esp_timer_create_args_t timer_args = 
				{
    			.callback = &boot_timer_callback,
        		.name = "boot_timer"
    			};
    		CRASH_ERROR_CHECK(esp_timer_create(&timer_args, &boot_timer));
    		CRASH_ERROR_CHECK(esp_timer_start_once(boot_timer, HEALTH_BOOT_TIME));	
			}
		}
	}
void getCrashInfo()
	{
	if(g_crash->magic == CRASH_MAGIC)
		{
		/*
		esp_rom_printf("crash handler: g_crash section valid\n");
		esp_rom_printf("crash handler: g_crash->boot_time_us: %llu\n", g_crash->boot_time_us);
		esp_rom_printf("crash handler: g_crash->reset_reason: %d\n", g_crash->reset_reason);
		esp_rom_printf("crash handler: g_crash->err: %d\n", g_crash->err);
		esp_rom_printf("crash handler: g_crash->crash_type: %d\n", g_crash->crash_type);
		esp_rom_printf("crash handler: g_crash->line: %d\n", g_crash->line);
		esp_rom_printf("crash handler: g_crash->file: %s\n", g_crash->file);
		esp_rom_printf("crash handler: g_crash->func: %s\n", g_crash->func);
		esp_rom_printf("crash handler: g_crash->task: %s\n", g_crash->task);
		esp_rom_printf("crash handler: g_crash->panic_addr: %u\n", g_crash->panic_addr);
		esp_rom_printf("crash handler: g_crash->panic_exception: %u\n", g_crash->panic_exception);
		esp_rom_printf("crash handler: g_crash->panic_core: %u\n", g_crash->panic_core);
		esp_rom_printf("crash handler: g_crash->panic_pseudo_excause: %u\n", g_crash->panic_pseudo_excause);
		esp_rom_printf("crash handler: g_crash->panic_reason: %s\n", g_crash->panic_reason);
		esp_rom_printf("crash handler: g_crash->panic_description: %s\n", g_crash->panic_description);
		*/
		ESP_LOGI(TAG, "\n  - g_crash section valid");
		ESP_LOGI(TAG, "  - boot_time_us: %llu", g_crash->boot_time_us);
		ESP_LOGI(TAG, "  - reset_reason: %d", g_crash->reset_reason);
		ESP_LOGI(TAG, "  - err: %d", g_crash->err);
		ESP_LOGI(TAG, "  - crash_type: %d", g_crash->crash_type);
		ESP_LOGI(TAG, "  - line: %d", g_crash->line);
		ESP_LOGI(TAG, "  - file: %s", g_crash->file);
		ESP_LOGI(TAG, "  - func: %s", g_crash->func);
		ESP_LOGI(TAG, "  - task: %s", g_crash->task);
		ESP_LOGI(TAG, "  - panic_addr: %u", g_crash->panic_addr);
		ESP_LOGI(TAG, "  - panic_exception: %u", g_crash->panic_exception);
		ESP_LOGI(TAG, "  - panic_core: %u", g_crash->panic_core);
		ESP_LOGI(TAG, "  - panic_pseudo_excause: %u", g_crash->panic_pseudo_excause);
		ESP_LOGI(TAG, "  - panic_reason: %s", g_crash->panic_reason);
		ESP_LOGI(TAG, "  - panic_description: %s", g_crash->panic_description);
		}
	else
		//esp_rom_printf("crash handler: g_crash section invalid %08x", g_crash);
		ESP_LOGI(TAG, "\n  - g_crash section invalid %08x", g_crash);
	}

