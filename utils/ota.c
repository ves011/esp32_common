/*
 * ota.c
 *
 *  Created on: Mar 19, 2023
 *      Author: viorel_serbu
 */


#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "errno.h"
#include "esp_app_format.h"
#include "common_defines.h"
#include "project_specific.h"
#if (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
    #include "mqtt_client.h"
    #include "mqtt_ctrl.h"
#endif
#include "utils.h"
#include "ota.h"

#define BUFFSIZE 1024

#define OTA_URL_SIZE 256

#define TAG "OTA"

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
static esp_partition_t *get_updateable_partition(const char *plabel)
	{
	const esp_partition_t *np = NULL, *p = NULL;
	const esp_partition_t *running = esp_ota_get_running_partition();
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
    	p= esp_partition_get(pit);
    	if(p)
    		{
    		if(!strcmp(p->label, plabel) && p != running && p->type == ESP_PARTITION_TYPE_APP)
                {
                np = p;
				break;
                }
    		}
    	pit = esp_partition_next(pit);
    	}
	if(np)
		ESP_LOGI(TAG, "Updateable partition %s @ 0x%0lx", np->label, (unsigned long)np->address);
	else
		ESP_LOGI(TAG, "%s partition not found or not cannot be updated", plabel);
	return (esp_partition_t *)np;
	}

int ota_task(const char *url, const char *pname)
	{
    int ret_val = ESP_OK, mqtt_on = 0;
    esp_err_t err;
    
    /* Handles that require careful resource tracking and cleanup */
    esp_ota_handle_t update_handle = 0;
    esp_http_client_handle_t client = NULL;
    
    // Static buffer allocation keeps 1024 bytes completely off the task stack
    #define BUFFSIZE 1024
    static char ota_write_data[BUFFSIZE + 1]; 
    
    const esp_partition_t *update_partition = NULL;
    const esp_partition_t *running = NULL;
    int64_t len = 0;
    int binary_read = 0;
    ESP_LOGI(TAG, "url:%s:  pname:%s:", url, pname);
    ESP_LOGI(TAG, "Starting OTA");
    update_partition = get_updateable_partition(pname);
    if (!update_partition) 
		{
        ESP_LOGE(TAG, "Failed to find updateable partition for %s", pname);
        ret_val = ESP_FAIL;
		} 
    else 
		{
#if (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
        if (mqtt_client_handle) 
            {
            mqtt_on = 1;
            ESP_LOGI(TAG, "Disconnecting and DESTROYING MQTT TLS Client to free up SRAM...");
            
            // This stops the client and frees ALL MbedTLS and transmission heap buffers
            esp_mqtt_client_destroy(mqtt_client_handle); 
            mqtt_client_handle = NULL; // Zero out the handle globally to prevent dangling pointers
            
            // Optional: Wait a few milliseconds for FreeRTOS to clean up the deleted task memory
            vTaskDelay(pdMS_TO_TICKS(100)); 
            ESP_LOGI(TAG, "Free heap space after killing MQTT: %lu bytes", (unsigned long)esp_get_free_heap_size());
            }
#endif
        running = esp_ota_get_running_partition();
        ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08lx)",
                 running->type, running->subtype, (unsigned long)running->address);

        esp_http_client_config_t config = {
            .url = url,
            .cert_pem = nvs_ca_crt,
            .cert_len = nvs_ca_crt_sz,
            .client_cert_pem = nvs_cl_crt,
            .client_cert_len = nvs_cl_crt_sz,
            .client_key_pem = nvs_cl_key,
            .client_key_len = nvs_cl_key_sz,
            .timeout_ms = 10000,
            .keep_alive_enable = true,
			};
            
        ret_val = ESP_FAIL;
        client = esp_http_client_init(&config);
        if (client)
			{
            err = esp_http_client_open(client, 0);
            if (err == ESP_OK) 
				{
                len = esp_http_client_fetch_headers(client);
                ESP_LOGI(TAG, "Server reported image size: %lld bytes. Destination partition max size: %ld bytes", 
                    len, update_partition->size);
                
                if (len > 0 && len <= update_partition->size)
					{
                    update_handle = 0;
                    err = esp_ota_begin(update_partition, (size_t)len, &update_handle);
                    if (err == ESP_OK) 
						{
                        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%0lx. Safe to begin download loop.",
                            update_partition->subtype, (unsigned long)update_partition->address);
                        
                        binary_read = 0;
                        int last_printed_percentage = -1;
                        while (1) 
							{
                            int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
                            if (data_read > 0) 
								{
                                if(binary_read == 0)
                                    {
                                    ota_write_data[data_read] = 0;
                                    if(strnstr(ota_write_data, NOT_FOUND_TAG, data_read))
                                        {
                                        ESP_LOGE(TAG, "invalid URL - %s", url);
                                        break;
                                        }
                                    }
                                err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
                                if (err != ESP_OK) 
									{
                                    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                                    break;
									}
                                binary_read += data_read;
                                int current_percentage = (int)((binary_read * 100) / len);
                                if (current_percentage != last_printed_percentage) 
                                    {
                                    // \r drops the cursor back to the start of the line. 
                                    // fflush forces the stdout buffer to push to the UART line immediately.
                                    my_printf("\r\t\t[OTA] Progress: %3d%% (%d / %lld bytes)", current_percentage, binary_read, len);
                                    fflush(stdout); 
                                    last_printed_percentage = current_percentage;
                                    }
								} 
                            else if (data_read == 0) 
								{
                                if (esp_http_client_is_complete_data_received(client)) 
									{
                                    printf("\n"); 
                                    fflush(stdout);
                                    ESP_LOGI(TAG, "Connection closed. Whole image downloaded successfully.");
                                    ret_val = ESP_OK;
									}
                                else
									{
                                    if (errno == ECONNRESET || errno == ENOTCONN)
                                        {
                                        printf("\n"); 
                                        fflush(stdout);
                                        ESP_LOGE(TAG, "Connection closed prematurely, errno = %d", errno);
                                        }
                                    }
                                break;
								}	 
                            else 
								{
                                printf("\n"); 
                                fflush(stdout);
                                ESP_LOGE(TAG, "SSL/HTTP read error occurred mid-download");
                                break;
								}
							}

                        // Complete validation path
                        if (ret_val == ESP_OK)
							{
                            err = esp_ota_end(update_handle);
                            update_handle = 0; // Prevent downstream abort cleanup call
                            printf("\n"); 
                            fflush(stdout);
                            if (err == ESP_OK) 
                                ESP_LOGI(TAG, "OTA Download finished successfully! (Boot flag switch deferred)");
                            else
								{
                                ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
                                ret_val = ESP_FAIL;
								}
							}
						}
                    else
                        ESP_LOGE(TAG, "ota_begin_update() failed, err = %d", err);
                    
                    // Safe cleanup if loop aborted or ended in error
                    if (update_handle != 0) 
						{
                        esp_ota_abort(update_handle);
                        update_handle = 0; 
						}
					}
                else
                    ESP_LOGE(TAG, "Image size error: size (%lld) does not fit partition size (%ld)", len, update_partition->size);
				}    
            else
                ESP_LOGE(TAG, "Cannot open http session %d", err);
            esp_http_client_cleanup(client);
			}
        else
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
#if (COMM_PROTO & MQTT_PROTO) == MQTT_PROTO
        if (mqtt_on) 
            mqtt_start(NULL);
#endif            
		}
    return ret_val;
	}