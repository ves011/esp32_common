/*
 * ota.c
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wear_levelling.h"
#include "esp_console.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "errno.h"
#include "spi_flash_mmap.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_app_format.h"
#include "mqtt_client.h"
#include "common_defines.h"
#include "external_defs.h"
#include "ota.h"

#define BUFFSIZE 1024

#define OTA_URL_SIZE 256
//#define FW_UPGRADE_URL "http://proxy.gnet:444/fw_upgrade.bin"
//TaskHandle_t ota_task_handle;

#define TAG "OTA_TASK"

/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_end");

static void http_cleanup(esp_http_client_handle_t client)
	{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
	}

void print_sha256 (const uint8_t *image_hash, const char *label)
	{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    	{
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    	}
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
	}
static esp_partition_t *get_updateable_partition()
	{
	const esp_partition_t *np = NULL;
	const esp_partition_t *running = esp_ota_get_running_partition();
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
    	np = esp_partition_get(pit);
    	if(np)
    		{
    		if(np != running && np->type == ESP_PARTITION_TYPE_APP)
				break;
    		}
    	pit = esp_partition_next(pit);
    	}
	if(np)
		ESP_LOGI(TAG, "Updateable partition %s @ 0x%0lx", np->label, np->address);
	else
		ESP_LOGI(TAG, "No updateable partition found");
	return (esp_partition_t *)np;
	}

void ota_task(const char *url)
	{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA");
	update_partition = get_updateable_partition();
	if(!update_partition)
		return;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    	{
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    	}
	/*
    if(!strcmp(running->label, OTA_PART_NAME))
    		{
    		ESP_LOGI(TAG, "Cannot do upgrade while running from %s partition\nReboot from factory partition first", OTA_PART_NAME);
    		return;
    		}
	*/
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08lx)",
             running->type, running->subtype, running->address);

    esp_http_client_config_t config =
    	{
        .url = url,
        .cert_pem = (char *)server_cert_pem_start,
		.client_cert_pem = (char *)client_cert_pem_start,
		.client_key_pem = (char *)client_key_pem_start,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    	};

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    	{
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return;
    	}
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    	{
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    	}
    esp_http_client_fetch_headers(client);

    //update_partition = esp_ota_get_next_update_partition(NULL);
    //assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset %0lx",
             update_partition->subtype, update_partition->address);

    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    while (1)
    	{
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0)
        	{
            ESP_LOGE(TAG, "Error: SSL data read error");
            http_cleanup(client);
        	}
        else if (data_read > 0)
        	{
            if (image_header_was_checked == false)
            	{
            	if(!strstr(ota_write_data, NOT_FOUND_TAG))
            		{
					esp_app_desc_t new_app_info;
					if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
						{
						// check current version with downloading
						memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
						ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

						esp_app_desc_t running_app_info;
						if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
							{
							ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
							}

						const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
						esp_app_desc_t invalid_app_info;
						if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
							{
							ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
							}

						// check current version with last invalid partition
						if (last_invalid_app != NULL)
							{
							if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
								{
								ESP_LOGW(TAG, "New version is the same as invalid version.");
								ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
								ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
								http_cleanup(client);
								//infinite_loop();
								return;
								}
							}
	/*
	#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
						if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
							{
							ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
							http_cleanup(client);
							infinite_loop();
							}
	#endif
	*/
						image_header_was_checked = true;

						err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
						if (err != ESP_OK)
							{
							ESP_LOGE(TAG, "esp_ota_begin failed %d (%s)", err, esp_err_to_name(err));
							http_cleanup(client);
							esp_ota_abort(update_handle);
							return;
							}
						ESP_LOGI(TAG, "esp_ota_begin succeeded");
						}
					else
						{
						ESP_LOGE(TAG, "received package is not fit len (%d / %d)", data_read, sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t));
						http_cleanup(client);
						esp_ota_abort(update_handle);
						return;
						}
					}
				else
					{
					ESP_LOGI(TAG, "Invalid url: %s", url);
					http_cleanup(client);
					return;
					}
				}
            err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK)
            	{
            	ESP_LOGE(TAG, "esp_ota_write error %d (%s)", err, esp_err_to_name(err));
                http_cleanup(client);
                esp_ota_abort(update_handle);
                return;
            	}
            binary_file_length += data_read;
            ESP_LOGI(TAG, "Written image length %d", binary_file_length);
        	}
        else if (data_read == 0)
        	{
           /*
            * As esp_http_client_read never returns negative error code, we rely on
            * `errno` to check for underlying transport connectivity closure if any
            */
            if (errno == ECONNRESET || errno == ENOTCONN)
            	{
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                break;
            	}
            if (esp_http_client_is_complete_data_received(client) == true)
            	{
                ESP_LOGI(TAG, "All data received. Connection closed");
                break;
            	}
        	}
    	}
    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);
    if (esp_http_client_is_complete_data_received(client) != true)
    	{
        ESP_LOGE(TAG, "Error in receiving complete file");
        http_cleanup(client);
        esp_ota_abort(update_handle);
        return;
    	}

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    	{
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        	{
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        	}
        else
        	{
            ESP_LOGE(TAG, "esp_ota_end failed %d (%s)!", err, esp_err_to_name(err));
        	}
    	}
    else
    	ESP_LOGI(TAG, "Image validation OK");
    http_cleanup(client);
	}

