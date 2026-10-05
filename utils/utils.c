/*
 * utils.c
 *
 *  Created on: Mar 10, 2023
 *      Author: viorel_serbu
 */


#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "lwip/inet.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "project_specific.h"
#include "common_defines.h"
#include "crash_handler.h"
#include "utils.h"

dev_config_t dev_conf;
char *nvs_cl_crt, *nvs_cl_key, *nvs_ca_crt;
size_t nvs_cl_crt_sz, nvs_ca_crt_sz, nvs_cl_key_sz;

#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
	#include "tcp_log.h"
#endif

static const char *TAG = "SPIFFS_RW";
static led_state_t led_st[NO_FLASH_LEDS] = {0};

void my_esp_restart(backtr_t backt, const char *reason)
	{
	ESP_LOGI("SYSTEM", "Restart triggered by SW: %s", reason);
	ESP_LOGI("---->", "%s - %s line %d", backt.file, backt.func, backt.line);
	restart_in_progress = 1;
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
	}

int my_log_vprintf(const char *fmt, va_list arguments)
	{	
	switch (dev_conf.cs)
    	{
        case CONSOLE_OFF:
            // logs disabled
            return 0;
        case CONSOLE_ON:
            // serial console
            return vprintf(fmt, arguments);
        case CONSOLE_TCP:
        case CONSOLE_MQTT:
        	{
			static char buf[MAX_LOG_LINE_SIZE];
			int n = vsnprintf(buf, sizeof(buf), fmt, arguments);
    		buf[sizeof(buf) - 1] = '\0';
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
            // best‑effort forward; no buffering
            tcp_log_message(buf);
            return strlen(buf);
#endif
			break;
			}
        default:
            break;
    	}
    return 0;
    }

int my_printf(char *format, ...)
	{
	static char buf[MAX_LOG_LINE_SIZE];
	va_list args;
	va_start( args, format );
	vsnprintf( buf, sizeof(buf) - 1, format, args );
	va_end( args );
	buf[sizeof(buf) - 1] = '\0';
	
	if(dev_conf.cs == CONSOLE_ON)
		return puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		{
		tcp_log_message(buf);
		return strlen(buf);
		}
#endif	
	return 0;
	}

void my_fputs(char *buf, FILE *f)
	{
	if(dev_conf.cs == CONSOLE_ON)
		puts(buf);
#if (COMM_PROTO & TCP_PROTO) == TCP_PROTO || (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO	
	else if(dev_conf.cs == CONSOLE_TCP || dev_conf.cs == CONSOLE_MQTT)
		tcp_log_message(buf);
#endif	
	}
#if FILESYSTEM == LITTLEFS
#include "esp_littlefs.h"
int littlefs_storage_check()
	{
	esp_err_t ret;
	esp_vfs_littlefs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    	};
	ESP_LOGI(TAG, "littlefs storage check");
	ret = esp_vfs_littlefs_register(&conf);
	if (ret == ESP_FAIL)
		{
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
		return ret;
		}
	else if (ret == ESP_ERR_NOT_FOUND)
		{
        ESP_LOGE(TAG, "Failed to find LittleFS partition");
		return ret;
		}

	size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) 
		{
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    	} 
	else
		{
		ESP_LOGI(TAG, "LITTLEFS_check() successful");
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
		}
	return ret;
	}
#elif FILESYSTEM == SPIFFS
#include "esp_spiffs.h"
int spiffs_storage_check()
	{
	esp_err_t ret;
    size_t total = 0, used = 0;
    esp_vfs_spiffs_conf_t conf_spiffs =
		{
		.base_path = BASE_PATH,
		.partition_label = PARTITION_LABEL,
		.max_files = 5,
		.format_if_mount_failed = true
		};

    ret = esp_vfs_spiffs_register(&conf_spiffs);
    if (ret != ESP_OK)
		{
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        RESTART("Failed to initialize SPIFFS");
		}
    ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
    if (ret != ESP_OK)
    	{
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        if(esp_spiffs_format(conf_spiffs.partition_label) != ESP_OK)
           	return ret;
        
        ret = esp_spiffs_info(conf_spiffs.partition_label, &total, &used);
		if(ret != ESP_OK)
        	return ret;
			
		// Check consistency of reported partiton size info.
		if (used > total)
			{
			ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
			ret = esp_spiffs_check(conf_spiffs.partition_label);
			// Could be also used to mean broken files, to clean unreferenced pages, etc.
			// More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
			if (ret != ESP_OK)
				{		
				ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
				return ret;
				}
			}
    	}
	ESP_LOGI(TAG, "SPIFFS_check() successful");
    return ESP_OK;
    }
#endif
int get_nvs_cert(char * entry_name, char **cert)
	{
	char *t = "NVS";
	size_t sz = 0;
	nvs_handle handle;
	if(nvs_open(NVS_CERT_NS, NVS_READONLY, &handle) == ESP_OK)
		{
		if(nvs_get_str(handle, entry_name, NULL, &sz) == ESP_OK)
			{
			if(*cert)
				free(*cert);
			*cert = calloc(sz, 1);
			if(cert)
				{
				if(nvs_get_str(handle, entry_name, *cert, &sz) != ESP_OK)
					{
					ESP_LOGE(t, "error reading %s from NVS", entry_name);
					free(*cert);
					*cert = NULL;
					}
				}
			else
				ESP_LOGE(t, "could not allocate memory for %s", entry_name);
			}
		else
			ESP_LOGI(t, "%s entry not found in certificates namespace", entry_name);
		
		nvs_close(handle);
		}
	else
		ESP_LOGI(t, "certificates namespace not found");
	ESP_LOGI(t, "%s - size: %d", entry_name, sz);
	return sz;
	}

int get_all_nvscerts()
	{
	int ret = ESP_FAIL;
	nvs_ca_crt_sz = get_nvs_cert(NVSCACRT, &nvs_ca_crt);
	if(nvs_ca_crt_sz)
		{
		nvs_cl_crt_sz = get_nvs_cert(NVSCLCRT, &nvs_cl_crt);
		if(nvs_cl_crt_sz)
			{
			nvs_cl_key_sz = get_nvs_cert(NVSCLKEY, &nvs_cl_key);
			if(nvs_cl_key_sz)
				ret = ESP_OK;
			}
		}
	return ret;
	}
void get_nvs_conf()
	{
	size_t sz = 0;
	nvs_handle handle;
	char b[128];
	int ret;
	dev_conf.cs = DEFAULT_CONSOLE_STATE;
	dev_conf.dev_id = DEFAULT_DEVICE_ID;
	strcpy(dev_conf.dev_name, DEFAULT_DEVICE_NAME);
	strcpy(dev_conf.sta_ssid, DEFAULT_STA_SSID);
	strcpy(dev_conf.sta_pass, DEFAULT_STA_PASS);
	strcpy(dev_conf.ap_ssid, DEFAULT_AP_SSID);
	strcpy(dev_conf.ap_pass, DEFAULT_AP_PASS);
	strcpy(dev_conf.ap_hostname, DEFAULT_AP_HOSTNAME);
	dev_conf.ap_ip = DEFAULT_AP_IP;
	dev_conf.nvs80211.sta_pass[0] = dev_conf.nvs80211.sta_ssid[0] = 0;
	if(nvs_open(NVS_CFG_NS, NVS_READONLY, &handle) == ESP_OK)
		{
		ret = nvs_get_str(handle, NVSSTASSID, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.sta_ssid))
			{
			ret = nvs_get_str(handle, NVSSTASSID, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.sta_ssid, b);
			}
		ret = nvs_get_str(handle, NVSSTAPASS, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.sta_pass))
			{
			ret = nvs_get_str(handle, NVSSTAPASS, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.sta_pass, b);
			}
		ret = nvs_get_str(handle, NVSAPSSID, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.ap_ssid))
			{
			ret = nvs_get_str(handle, NVSAPSSID, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_ssid, b);
			}
		ret = nvs_get_str(handle, NVSAPPASS, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.ap_pass))
			{
			ret = nvs_get_str(handle, NVSAPPASS, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_pass, b);
			}
		ret = nvs_get_str(handle, NVSAPHOSTNAME, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.ap_hostname))
			{
			ret = nvs_get_str(handle, NVSAPHOSTNAME, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.ap_hostname, b);
			}
		ret = nvs_get_str(handle, NVSAPIP, NULL, &sz);
		if(ret == ESP_OK && sz < 128)
			{
			ret = nvs_get_str(handle, NVSAPIP, b, &sz);
			if(ret == ESP_OK)
				dev_conf.ap_ip = inet_addr(b);
			}
		ret = nvs_get_str(handle, NVSDEVNAME, NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.dev_name))
			{
			ret = nvs_get_str(handle, NVSDEVNAME, b, &sz);
			if(ret == ESP_OK)
				strcpy(dev_conf.dev_name, b);
			}
		ret = nvs_get_str(handle, NVSDEVID, NULL, &sz);
		if(ret == ESP_OK && sz < 128)
			{
			ret = nvs_get_str(handle, NVSDEVID, b, &sz);
			if(ret == ESP_OK)
				dev_conf.dev_id = atoi(b);
			}
		ret = nvs_get_str(handle, NVSCONSTATE, NULL, &sz);
		if(ret == ESP_OK && sz < 128)
			{
			ret = nvs_get_str(handle, NVSCONSTATE, b, &sz);
			if(ret == ESP_OK)
				dev_conf.cs = atoi(b);
			}
		nvs_close(handle);
		}
	if(nvs_open("nvs.net80211", NVS_READONLY, &handle) == ESP_OK)
		{
		ret = nvs_get_blob(handle, "sta.pswd", NULL, &sz);
		if(ret == ESP_OK && sz < sizeof(dev_conf.nvs80211.sta_pass) - 1)
			{
			ret = nvs_get_blob(handle, "sta.pswd", dev_conf.nvs80211.sta_pass, &sz);
			dev_conf.nvs80211.sta_pass[sz] = 0;
			}
		ret = nvs_get_blob(handle, "sta.ssid", NULL, &sz);
		if(ret == ESP_OK  && sz < sizeof(dev_conf.nvs80211.sta_ssid) - 1)
			{
			ret = nvs_get_blob(handle, "sta.ssid", b, &sz);
			if(ret == ESP_OK)
				{
				size_t s = *(uint32_t *)b;
				if(s < sizeof(dev_conf.nvs80211.sta_ssid))
					{
					memcpy(dev_conf.nvs80211.sta_ssid, b + 4, s);
					dev_conf.nvs80211.sta_ssid[s] = 0;
					}
				}
			}
		nvs_close(handle);
		}
	}
	

// Include target-specific headers automatically based on the active build target
#if defined(CONFIG_IDF_TARGET_ESP32)
	#include "hal/gpio_hal.h"
	#include "soc/io_mux_reg.h"
#else
	#include "hal/gpio_ll.h"
	#include "soc/gpio_struct.h"
#endif

static esp_timer_handle_t flash_timer = NULL;
static uint32_t tick_count = 0;
/**
 * @brief Helper function to round up any integer to the next power of 2.
 *        e.g., 3 -> 4, 5 -> 8, 8 -> 8.
 */
static inline uint32_t next_power_of_2(uint32_t n) 
	{
    if (n == 0) return 0; 
	n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4; n |= n >> 8; n |= n >> 16;
    return n + 1;
	}
static void flash_timer_callback(void* arg)
    {
	tick_count++;
    for(int i = 0; i < NO_FLASH_LEDS; i++)
		{
		if(led_st[i].led_pin && led_st[i].flash_tick )
			{
			if((tick_count & led_st[i].flash_tick) == 0)
				{
				gpio_set_level(led_st[i].led_pin, led_st[i].state);
				led_st[i].state = !led_st[i].state;
				//ESP_LOGI("flash_timer_callback", "led %d state %d", led_st[i].led_pin, led_st[i].state);
				}
			}
		}
    }
/**
 * @brief Checks if a GPIO pin is currently configured as an output at run time.
 *        Works on ESP32, ESP32-S3, ESP32-C3, and ESP32-C6.
 */
bool is_gpio_output(gpio_num_t gpio_num)
	{
#if defined(CONFIG_IDF_TARGET_ESP32)
    // Classic ESP32 uses split enable banks handled via legacy HAL structures
    gpio_dev_t *hw = GPIO_HAL_GET_HW(GPIO_PORT_0);
    if (gpio_num < 32)
        return (hw->enable >> gpio_num) & 0x1;
    else
        return (hw->enable1.data >> (gpio_num - 32)) & 0x1;
#else
    // ESP32-S3, C3, and C6 can all leverage the unified Low-Level (LL) API safely
	gpio_io_config_t io_conf = {0};	
    gpio_ll_get_io_config(&GPIO, gpio_num, &io_conf);
    return io_conf.oe;
#endif
	}

/**
 * @brief Checks if a GPIO pin is currently configured as an input at run time.
 */
bool is_gpio_input(gpio_num_t gpio_num)
	{
#if defined(CONFIG_IDF_TARGET_ESP32)
	uint32_t pin_param = REG_READ(GPIO_PIN_MUX_REG[gpio_num]);
    return (pin_param & FUN_IE) ? true : false;
#else
	gpio_io_config_t io_conf = {0};
    gpio_ll_get_io_config(&GPIO, gpio_num, &io_conf);
    return io_conf.ie;
#endif
	}

int set_flash_led(int led_no, int led_state, int tick)
	{
	int i, ret = ESP_FAIL;
	// Normalize the input tick to the nearest power of 2, then calculate mask
    uint32_t normalized_tick = next_power_of_2(tick);
	for(i = 0; i < NO_FLASH_LEDS; i++)
		{
		if(led_st[i].led_pin == led_no)
			{
			//led_st[i].led_pin = led_no;
			led_st[i].flash_tick = normalized_tick;
			led_st[i].state = led_state;
			gpio_set_level(led_st[i].led_pin, led_st[i].state);
			return ESP_OK;
			}
		if(led_st[i].led_pin == 0)
			break;
		}
	if(i < NO_FLASH_LEDS)
		{
		if(is_gpio_output(led_no))
			{
			led_st[i].led_pin = led_no;
			led_st[i].flash_tick = normalized_tick;
			led_st[i].state = led_state;
			gpio_set_level(led_st[i].led_pin, led_st[i].state);
			ESP_LOGI("set_flash_led", "led %d added to flash vector (index: %d)", led_no, i);
			ret = ESP_OK;
			}
		else
			{
			ESP_LOGI("set_flash_led", "led %d is not configured for output", led_no);
			ret = ESP_ERR_INVALID_ARG;
			}
		}
	else 
		{
		ESP_LOGI("set_flash_led", "no more free slots for led %d / max slots = %d", led_no, NO_FLASH_LEDS);
		ret = ESP_FAIL;
		}
	if(flash_timer == NULL)
		{
		esp_timer_create_args_t timer_args = 
			{
	    	.callback = &flash_timer_callback,
	        .name = "flash_timer"
	    	};
	    CRASH_ERROR_CHECK(esp_timer_create(&timer_args, &flash_timer));
	    CRASH_ERROR_CHECK(esp_timer_start_periodic(flash_timer, 100000));
		ESP_LOGI("set_flash_led", "flash_timer started");
		}	
	return ret;
	}

