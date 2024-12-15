/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/**
 * @file cmd_system.c
 * @brief implements processing functions for "system" commands
 * @brief register functions: register_...() --> register the specific system command and specifies the arguments
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/dirent.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_sleep.h"
#include "spi_flash_mmap.h"
#include "esp_netif.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/task.h"
#include <sys/fcntl.h>
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include <dirent.h>
#include <sys/stat.h>
#include "ping/ping_sock.h"
#include "mqtt_client.h"
#include "esp_flash_partitions.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "common_defines.h"
#include "external_defs.h"
	#include "utils.h"
	#include "ota.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "tcp_log.h"



#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif

struct {
    struct arg_dbl *timeout;
    struct arg_dbl *interval;
    struct arg_int *data_size;
    struct arg_int *count;
    struct arg_int *tos;
    struct arg_str *host;
    struct arg_end *end;
} ping_args;

static const char *TAG = "cmd_system";

static void register_free(void);
static void register_heap(void);
static void register_version(void);
static void register_restart(void);
static void register_deep_sleep(void);
static void register_light_sleep(void);
static void register_uptime(void);
static void register_ls(void);
static void register_console(void);
static void register_boot(void);
static void register_cat(void);
static void register_rm(void);
//static int ota_start(int argc, char **argv);
//static void register_ota(void);
static void register_echo(void);
#if WITH_TASKS_INFO
static void register_tasks(void);
#endif

void register_system_common(void)
{
    register_free();
    register_heap();
    register_version();
    register_restart();
    register_uptime();
    register_ls();
    register_console();
    register_boot();
    register_cat();
    register_rm();
//    register_ota();
    register_echo();
#if WITH_TASKS_INFO
    register_tasks();
#endif
}

void register_system_sleep(void)
{
    register_deep_sleep();
    register_light_sleep();
}

void register_system(void)
{
    register_system_common();
    register_system_sleep();
    //register_ping();

}

/*
static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
	{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    my_printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms",
           recv_len, ipaddr_ntoa((ip_addr_t*)&target_addr), seqno, ttl, elapsed_time);
	}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
	{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    my_printf("From %s icmp_seq=%d timeout",ipaddr_ntoa((ip_addr_t*)&target_addr), seqno);
	}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
	{
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    my_printf("\n--- %s ping statistics ---", inet_ntoa(*ip_2_ip4(&target_addr)));

    my_printf("%d packets transmitted, %d received, %d%% packet loss, time %dms\n",
           transmitted, received, loss, total_time_ms);
    // delete the ping sessions, so that we clean up all resources and can create a new ping session
    // we don't have to call delete function in the callback, instead we can call delete function from other tasks
    esp_ping_delete_session(hdl);
	}*/
/*
static int do_ping_cmd(int argc, char **argv)
	{
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();

    int nerrors = arg_parse(argc, argv, (void **)&ping_args);
    if (nerrors != 0)
    	{
        //arg_print_errors(stderr, ping_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    if(!isConnected())
    	{
    	ESP_LOGI(TAG, "WiFi not connected or in AP mode");
    	return 1;
    	}
    if (ping_args.timeout->count > 0)
        config.timeout_ms = (uint32_t)(ping_args.timeout->dval[0] * 1000);

    if (ping_args.interval->count > 0)
        config.interval_ms = (uint32_t)(ping_args.interval->dval[0] * 1000);

    if (ping_args.data_size->count > 0)
        config.data_size = (uint32_t)(ping_args.data_size->ival[0]);

    if (ping_args.count->count > 0)
        config.count = (uint32_t)(ping_args.count->ival[0]);

    if (ping_args.tos->count > 0)
        config.tos = (uint32_t)(ping_args.tos->ival[0]);

    // parse IP address
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));

    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));
    // convert ip4 string or hostname to ip4 or ip6 address
    if (getaddrinfo(ping_args.host->sval[0], NULL, &hint, &res) != 0)
    	{
        my_printf("ping: unknown host %s\n", ping_args.host->sval[0]);
        return 1;
        }
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);
    config.target_addr = target_addr;
    config.task_stack_size = 3072;

    // set callback functions
    esp_ping_callbacks_t cbs =
    	{
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end,
        .cb_args = NULL
    	};
    esp_ping_handle_t ping;
    esp_ping_new_session(&config, &cbs, &ping);
    esp_ping_start(ping);

    return 0;
	}*/
/*
void register_ping(void)
	{
    ping_args.timeout = arg_dbl0("W", "timeout", "<t>", "Time to wait for a response, in seconds");
    ping_args.interval = arg_dbl0("i", "interval", "<t>", "Wait interval seconds between sending each packet");
    ping_args.data_size = arg_int0("s", "size", "<n>", "Specify the number of data bytes to be sent");
    ping_args.count = arg_int0("c", "count", "<n>", "Stop after sending count packets");
    ping_args.tos = arg_int0("Q", "tos", "<n>", "Set Type of Service related bits in IP datagrams");
    ping_args.host = arg_str1(NULL, NULL, "<host>", "Host address");
    ping_args.end = arg_end(1);
    const esp_console_cmd_t ping_cmd =
    	{
        .command = "ping",
        .help = "send ICMP ECHO_REQUEST to network hosts",
        .hint = NULL,
        .func = &do_ping_cmd,
        .argtable = &ping_args
    	};
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
	}*/
/* 'version' command */
static int get_version(int argc, char **argv)
	{
	const char *model;
	esp_chip_info_t info;
	uint32_t flash_size;
	esp_chip_info(&info);

	switch(info.model)
		{
		case CHIP_ESP32:
			model = "ESP32";
			break;
		case CHIP_ESP32S2:
			model = "ESP32-S2";
			break;
		case CHIP_ESP32S3:
			model = "ESP32-S3";
			break;
		case CHIP_ESP32C3:
			model = "ESP32-C3";
			break;
		case CHIP_ESP32H2:
			model = "ESP32-H2";
			break;
		case CHIP_ESP32C2:
			model = "ESP32-C2";
			break;
		default:
			model = "Unknown";
			break;
		}

	if(esp_flash_get_size(NULL, &flash_size) != ESP_OK)
		{
		printf("Get flash size failed");
		return 1;
		}
	printf("IDF Version:%s\r\n", esp_get_idf_version());
	printf("Chip info:\r\n");
	printf("\tmodel:%s\r\n", model);
	printf("\tcores:%d\r\n", info.cores);
	printf("\tfeature:%s%s%s%s%"PRIu32"%s%s\r\n",
		   info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
		   info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
		   info.features & CHIP_FEATURE_BT ? "/BT" : "",
		   info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
		   flash_size / (1024 * 1024), " MB",
		   info.features & CHIP_FEATURE_EMB_PSRAM ? "/PSRAM" : "/no PSRAM");
	printf("\trevision number:%d\r\n", info.revision);
	return 0;
	}

static void register_version(void)
{
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Get version of chip and SDK",
        .hint = NULL,
        .func = &get_version,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'restart' command restarts the program */

static int restart(int argc, char **argv)
	{
    ESP_LOGI(TAG, "Restarting");
    restart_in_progress = 1;
    esp_restart();
    return 1;
	}

static void register_restart(void)
{
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Software reset of the chip",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

/** 'free' command prints available heap memory */

static int free_mem(int argc, char **argv)
{
    my_printf("%d", esp_get_free_heap_size());
    return 0;
}

static void register_free(void)
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

struct
	{
    struct arg_str *path;
    struct arg_end *end;
	} ls_args;

struct
	{
    struct arg_str *fname;
    struct arg_end *end;
	} cat_args;

static int ls_files(int argc, char **argv)
	{
	esp_err_t ret;
	size_t total = 0, used = 0;
	DIR *dir = NULL;
	struct stat st;
	struct dirent *ent;
	char ftype;
	char path[64], pfname[64], outputm[300];
	int nerrors = arg_parse(argc, argv, (void **)&ls_args);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, ls_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
	if(ls_args.path->count  == 1)
		{
		strcpy(path, ls_args.path->sval[0]);
		}
	else
		{
		path[0] = 0;
		}
	esp_vfs_spiffs_conf_t conf =
		{
		.base_path = "/var",
		.partition_label = "user",
		.max_files = MAX_NO_FILES,
		.format_if_mount_failed = true
		};

   	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK)
		{
		ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
		ret = esp_spiffs_format(conf.partition_label);
		if(ret != ESP_OK)
			{
			ESP_LOGI(TAG, "Failed to format SPIFFS partition (%s)", esp_err_to_name(ret));
			esp_vfs_spiffs_unregister(conf.partition_label);
			return ret;
			}
		ret = esp_spiffs_info(conf.partition_label, &total, &used);
		if(ret != ESP_OK)
			{
			ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s).", esp_err_to_name(ret));
			esp_vfs_spiffs_unregister(conf.partition_label);
			return ret;
			}
		}
	my_printf("Partition name: %s / total size: %-d / used: %-d\n", conf.partition_label, total, used);
	strcpy(pfname, "/var");
	strcat(pfname, path);
	strcpy(path, pfname);
	dir = opendir(path);
	if (!dir)
		{
		my_printf("Error opening %s directory\n", path);
		return ESP_FAIL;
		}
	my_printf("%s", path);
	while ((ent = readdir(dir)) != NULL)
		{
		if (ent->d_type == DT_REG)
			ftype  = '-';
		else
			ftype = 'd';
		strcpy(pfname, path);
		strcat(pfname, "/");
		strcat(pfname, ent->d_name);
		sprintf(outputm, " %c    %-25s   ", ftype, ent->d_name);
		if(stat(pfname, &st) == 0)
			{
			sprintf(pfname, "%6lu    ", (uint32_t)(st.st_size));
			strcat(outputm, pfname);
			strcat(outputm, ctime (&st.st_mtim.tv_sec));
			}
// remove ending \n character, because it is added by fputs()
		if(outputm[strlen(outputm) - 1] == '\n')
			outputm[strlen(outputm) - 1] = 0;
		my_printf("%s", outputm);
		}
	my_printf("\n");
	closedir(dir);
	return ret;
	}
static void register_ls(void)
	{
	ls_args.path = arg_str0(NULL, NULL, "path", "path");
	ls_args.end = arg_end(1);
	const esp_console_cmd_t cmd =
		{
		.command = "ls",
		.help = "list files in \"user\" partition",
		.hint = NULL,
		.func = &ls_files,
		.argtable = &ls_args,
		};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}

static int cat_file(int argc, char **argv)
	{
	char path[64], outputm[300];
	struct stat st;
	int i;
	int nerrors = arg_parse(argc, argv, (void **)&cat_args);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, ls_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    strcpy(path, "/var/");
	strcat(path, cat_args.fname->sval[0]);
	if (stat(path, &st) != 0)
		{
		// file does no exists
		my_printf("file %s does not exists\n", path);
		return 0;
		}
	else
		{
		FILE *f = fopen(path, "r");
		if (f != NULL)
			{
			i = 0;
			while(!feof(f))
				{
				if(fgets(outputm, 298, f))
					{
					//if(feof(f))
					//	break;
					if(outputm[strlen(outputm) - 1] == '\n')
						outputm[strlen(outputm) - 1] = 0;
					my_printf("%s", outputm);
					i++;
					}
				}
			fclose(f);
			my_printf("\nEOF %d lines\n", i);
			}
		}
	return 0;
	}
static void register_cat(void)
	{
	cat_args.fname = arg_str1(NULL, NULL, "<file_name>", "file name");
	cat_args.end = arg_end(1);
	const esp_console_cmd_t cmd =
		{
		.command = "cat",
		.help = "show content of file in \"user\" partition",
		.hint = NULL,
		.func = &cat_file,
		.argtable = &cat_args,
		};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}

static int rm_file(int argc, char **argv)
	{
	char path[64];
	struct stat st;
	int nerrors = arg_parse(argc, argv, (void **)&cat_args);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, ls_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    strcpy(path, "/var/");
	strcat(path, cat_args.fname->sval[0]);
	if (stat(path, &st) != 0)
		{
		// file does no exists
		my_printf("file %s does not exists", path);
		return 0;
		}
	else
		{
		int res = unlink(path);
		if(res < 0)
			my_printf("Error deleting file %d", errno);
		}
	return 0;
	}
static void register_rm(void)
	{
	cat_args.fname = arg_str1(NULL, NULL, "<file_name>", "file name");
	cat_args.end = arg_end(1);
	const esp_console_cmd_t cmd =
		{
		.command = "rm",
		.help = "remove the file in \"user\" partition",
		.hint = NULL,
		.func = &rm_file,
		.argtable = &cat_args,
		};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}
struct
	{
    struct arg_str *str;
    struct arg_str *ot;
    struct arg_str *fname;
    struct arg_end *end;
	} echo_args;

static int echo(int argc, char **argv)
	{
	char path[64];
	FILE *f = NULL;
	int nerrors = arg_parse(argc, argv, (void **)&echo_args);
	if (nerrors != 0)
    	{
        //arg_print_errors(stderr, ls_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
	if(echo_args.str->count && echo_args.ot->count && echo_args.fname->count)
		{
		strcpy(path, "/var/");
		strcat(path, echo_args.fname->sval[0]);
		if(!strcmp(echo_args.ot->sval[0], ">"))
			f = fopen(path, "w");
		else if(!strcmp(echo_args.ot->sval[0], ">>"))
			f = fopen(path, "a");
		if(f)
			{
			char buf[100];
			strcpy(buf, echo_args.str->sval[0]);
			strcat(buf, "\n");
			fputs(buf,f);
			fclose(f);
			}
		else
			my_printf("Error opening %s file %d", path, errno);
		}
	else
		{
		my_printf("arguments missing \n -- echo \"str to write\" > [| >>] filename");
		}
	return 0;
	}
static void register_echo(void)
	{
	echo_args.str = arg_str0(NULL, NULL, "str", "<string to be written>");
	echo_args.ot = arg_str0(NULL, NULL, "> | >>", "write(>) or append(>>)");
	echo_args.fname = arg_str0(NULL, NULL, "<file_name>", "file name");
	echo_args.end = arg_end(1);
	const esp_console_cmd_t cmd =
		{
		.command = "echo",
		.help = "output a string to a file",
		.hint = NULL,
		.func = &echo,
		.argtable = &echo_args,
		};
	ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}

/* 'heap' command prints minumum heap size */
static int heap_size(int argc, char **argv)
{
    uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    my_printf("min heap size: %u", heap_size);
    return 0;
}

static void register_heap(void)
{
    const esp_console_cmd_t heap_cmd = {
        .command = "heap",
        .help = "Get minimum size of free heap memory that was available during program execution",
        .hint = NULL,
        .func = &heap_size,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&heap_cmd) );

}

/** 'tasks' command prints the list of tasks and related information */
#if WITH_TASKS_INFO

static int tasks_info(int argc, char **argv)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    my_fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    my_fputs("\n", stdout);
    vTaskList(task_list_buffer);
    my_fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

static void register_tasks(void)
{
    const esp_console_cmd_t cmd = {
        .command = "tasks",
        .help = "Get information about running tasks",
        .hint = NULL,
        .func = &tasks_info,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#endif // WITH_TASKS_INFO

/** 'deep_sleep' command puts the chip into deep sleep mode */

static struct {
    struct arg_int *wakeup_time;
#if SOC_PM_SUPPORT_EXT_WAKEUP
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
#endif
    struct arg_end *end;
} deep_sleep_args;


static int deep_sleep(int argc, char **argv)
	{
    int nerrors = arg_parse(argc, argv, (void **) &deep_sleep_args);
    if (nerrors != 0)
    	{
        //arg_print_errors(stderr, deep_sleep_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    if (deep_sleep_args.wakeup_time->count)
    	{
        uint64_t timeout = 1000ULL * deep_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    	}

#if SOC_PM_SUPPORT_EXT_WAKEUP
    if (deep_sleep_args.wakeup_gpio_num->count)
    	{
        int io_num = deep_sleep_args.wakeup_gpio_num->ival[0];
        if (!esp_sleep_is_valid_wakeup_gpio(io_num))
        	{
            ESP_LOGE(TAG, "GPIO %d is not an RTC IO", io_num);
            return 1;
        	}
        int level = 0;
        if (deep_sleep_args.wakeup_gpio_level->count)
        	{
            level = deep_sleep_args.wakeup_gpio_level->ival[0];
            if (level != 0 && level != 1)
            	{
                ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
                return 1;
            	}
        	}
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << io_num, level) );
        ESP_LOGE(TAG, "GPIO wakeup from deep sleep currently unsupported on ESP32-C3");
    	}
#endif // SOC_PM_SUPPORT_EXT_WAKEUP

#if CONFIG_IDF_TARGET_ESP32
    rtc_gpio_isolate(GPIO_NUM_12);
#endif //CONFIG_IDF_TARGET_ESP32

    esp_deep_sleep_start();
    return 0;
	}

static void register_deep_sleep(void)
	{
    int num_args = 1;
    deep_sleep_args.wakeup_time =
        arg_int0("t", "time", "<t>", "Wake up time, ms");
#if SOC_PM_SUPPORT_EXT_WAKEUP
    deep_sleep_args.wakeup_gpio_num =
        arg_int0(NULL, "io", "<n>",
                 "If specified, wakeup using GPIO with given number");
    deep_sleep_args.wakeup_gpio_level =
        arg_int0(NULL, "io_level", "<0|1>", "GPIO level to trigger wakeup");
    num_args += 2;
#endif
    deep_sleep_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd =
    	{
        .command = "deep_sleep",
        .help = "Enter deep sleep mode. "
#if SOC_PM_SUPPORT_EXT_WAKEUP
        "Two wakeup modes are supported: timer and GPIO. "
#else
        "Timer wakeup mode is supported. "
#endif
        "If no wakeup option is specified, will sleep indefinitely.",
        .hint = NULL,
        .func = &deep_sleep,
        .argtable = &deep_sleep_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}

/** 'light_sleep' command puts the chip into light sleep mode */

static struct {
    struct arg_int *wakeup_time;
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
    struct arg_end *end;
} light_sleep_args;

static int light_sleep(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &light_sleep_args);
    if (nerrors != 0) {
        //arg_print_errors(stderr, light_sleep_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (light_sleep_args.wakeup_time->count) {
        uint64_t timeout = 1000ULL * light_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }
    int io_count = light_sleep_args.wakeup_gpio_num->count;
    if (io_count != light_sleep_args.wakeup_gpio_level->count) {
        ESP_LOGE(TAG, "Should have same number of 'io' and 'io_level' arguments");
        return 1;
    }
    for (int i = 0; i < io_count; ++i) {
        int io_num = light_sleep_args.wakeup_gpio_num->ival[i];
        int level = light_sleep_args.wakeup_gpio_level->ival[i];
        if (level != 0 && level != 1) {
            ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
            return 1;
        }
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        ESP_ERROR_CHECK( gpio_wakeup_enable(io_num, level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL) );
    }
    if (io_count > 0) {
        ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
    }
    if (CONFIG_ESP_CONSOLE_UART_NUM >= 0 && CONFIG_ESP_CONSOLE_UART_NUM <= UART_NUM_1) {
        ESP_LOGI(TAG, "Enabling UART wakeup (press ENTER to exit light sleep)");
        ESP_ERROR_CHECK( uart_set_wakeup_threshold(CONFIG_ESP_CONSOLE_UART_NUM, 3) );
        ESP_ERROR_CHECK( esp_sleep_enable_uart_wakeup(CONFIG_ESP_CONSOLE_UART_NUM) );
    }
    fflush(stdout);
    fsync(fileno(stdout));
    esp_light_sleep_start();
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char *cause_str;
    switch (cause) {
    case ESP_SLEEP_WAKEUP_GPIO:
        cause_str = "GPIO";
        break;
    case ESP_SLEEP_WAKEUP_UART:
        cause_str = "UART";
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        cause_str = "timer";
        break;
    default:
        cause_str = "unknown";
        my_printf("%d\n", cause);
    }
    ESP_LOGI(TAG, "Woke up from: %s", cause_str);
    return 0;
}

static void register_light_sleep(void)
{
    light_sleep_args.wakeup_time =
        arg_int0("t", "time", "<t>", "Wake up time, ms");
    light_sleep_args.wakeup_gpio_num =
        arg_intn(NULL, "io", "<n>", 0, 8,
                 "If specified, wakeup using GPIO with given number");
    light_sleep_args.wakeup_gpio_level =
        arg_intn(NULL, "io_level", "<0|1>", 0, 8, "GPIO level to trigger wakeup");
    light_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "light_sleep",
        .help = "Enter light sleep mode. "
        "Two wakeup modes are supported: timer and GPIO. "
        "Multiple GPIO pins can be specified using pairs of "
        "'io' and 'io_level' arguments. "
        "Will also wake up on UART input.",
        .hint = NULL,
        .func = &light_sleep,
        .argtable = &light_sleep_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int uptime()
	{
	uint64_t tmsec = esp_timer_get_time() /1000000;
	uint32_t td, th, tm, ts;
	td = tmsec / 86400;
	tmsec = tmsec % 86400;
	th = tmsec / 3600;
	tmsec = tmsec % 3600;
	tm = tmsec / 60;
	ts = tmsec % 60;
	my_printf("Uptime: %d days %02u hours %02u min %02u sec since last boot", td, th, tm, ts);
	return 0;
	}
static void register_uptime(void)
	{
    const esp_console_cmd_t cmd = {
        .command = "uptime",
        .help = "Get uptime duration since last boot",
        .hint = NULL,
        .func = &uptime,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}

static struct
	{
    struct arg_str *op;
    struct arg_end *end;
	} console_args;
int set_console(int argc, char **argv)
	{
	console_state_t cs;
	int nerrors = arg_parse(argc, argv, (void **)&console_args);
    if (nerrors != 0)
    	{
        //arg_print_errors(stderr, console_args.end, argv[0]);
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    if(console_args.op->count)
    	{
		if(strcmp(console_args.op->sval[0], "on") == 0)
			cs = CONSOLE_ON;
		else if(strcmp(console_args.op->sval[0], "off") == 0)
			cs = CONSOLE_OFF;
		else if(strcmp(console_args.op->sval[0], "tcp") == 0)
			cs = CONSOLE_TCP;
		else if(strcmp(console_args.op->sval[0], "mqtt") == 0)
			cs = CONSOLE_MQTT;
		else
			{
			my_printf("console: unknown option [%s]", console_args.op->sval[0]);
			return 0;
			}
		if(console_state != cs)
			rw_console_state(PARAM_WRITE, &cs);
		console_state = cs;
		tcp_log_init();
		}
	else
		my_printf("Console state: %d\n", console_state);
    return 0;
    }
static void register_console(void)
	{
	console_args.op = arg_str0(NULL, NULL, "on | off | tcp", "console output: enable | disable | send to tcp logserver");
	console_args.end = arg_end(1);
	const esp_console_cmd_t cmd = {
        .command = "console",
        .help = "Set on/off/tcp console",
        .hint = NULL,
        .func = &set_console,
        .argtable = &console_args

    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}
/*
static struct
	{
    struct arg_str *url;
    struct arg_end *end;
	} ota_args;

static int ota_start(int argc, char **argv)
	{
	int nerrors = arg_parse(argc, argv, (void **)&ota_args);
	if (nerrors != 0)
    	{
        my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    if(ota_args.url->count)
    	{
    	ota_task(ota_args.url->sval[0]);
    	}
	return 0;
	}
static void register_ota(void)
	{
	ota_args.url = arg_str1(NULL, NULL, "<url>", "https://server:port/file_name");
	ota_args.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "ota",
        .help = "Perform OTA fw upgrade",
        .hint = NULL,
        .func = &ota_start,
        .argtable = &ota_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}
*/
static struct
	{
    struct arg_str *pname;
    struct arg_end *end;
	} boot_args;
static int boot_from(int argc, char **argv)
	{
	int nerrors = arg_parse(argc, argv, (void **)&boot_args);
	if (nerrors != 0)
		{
		my_printf("%s arguments error", argv[0]);
        return 1;
    	}
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *bootp = esp_ota_get_boot_partition();
    const esp_partition_t *np = NULL, *sbp = NULL;
    int bp = 0, rp = 0;
    esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    my_printf("\nAvailable boot partitions\n");
    my_printf("name\toffset hex(dec)\t\tsize hex(dec)\t\tboot\t running");
    my_printf("-------------------------------------------------------------------------");
    while(pit)
    	{
    	np = esp_partition_get(pit);
    	if(np)
    		{
    		if(boot_args.pname->count)
    			{
    			if(!strcmp(np->label, boot_args.pname->sval[0]))
    			sbp = np;
    			}
    		bp = 0, rp = 0;
    		if(np == running)
    			rp = 1;
    		if(np == bootp)
    			bp = 0;
    		my_printf("%s\t%06x(%8d)\t%06x(%8d)\t%2d\t%5d", np->label, np->address, np->address, np->size, np->size, bp, rp);
    		}
    	pit = esp_partition_next(pit);
    	}
    my_printf("\n");
    if(boot_args.pname->count)
    	{
		if(sbp)
			{
			int err = esp_ota_set_boot_partition(sbp);
			if(err == ESP_OK)
				{
				my_printf("Boot partition set to \"%s\" @ %06x", sbp->label, sbp->address);
				my_printf("Reboot now to run it\n");
				}
			else
				my_printf("Could not set boot partition to \"%s\" @ %06x", sbp->label, sbp->address);
			}
		else
			my_printf("Partition \"%s\", not found in partition list", boot_args.pname->sval[0]);
		}
	return 0;
	}
static void register_boot(void)
	{
	boot_args.pname = arg_str0(NULL, NULL, "<part_name>", "select boot partition");
	boot_args.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "boot",
        .help = "set boot partition = <part_name>",
        .hint = NULL,
        .func = &boot_from,
        .argtable = &boot_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
	}
void do_system_cmd(int argc, char **argv)
	{
	ESP_LOGI(TAG, "%d, %s", argc, argv[0]);
	if(!strcmp(argv[0], "console"))
		set_console(argc, argv);
	else if(!strcmp(argv[0], "uptime"))
		uptime();
#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
	else if(!strcmp(argv[0], "tasks"))
		tasks_info(argc, argv);
#endif
	else if(!strcmp(argv[0], "heap"))
		heap_size(argc, argv);
	else if(!strcmp(argv[0], "ls"))
		ls_files(argc, argv);
	else if(!strcmp(argv[0], "free"))
		free_mem(argc, argv);
	else if(!strcmp(argv[0], "restart"))
		restart(argc, argv);
	else if(!strcmp(argv[0], "version"))
		get_version(argc, argv);
	//else if(!strcmp(argv[0], "ping"))
	//	do_ping_cmd(argc, argv);
	else if(!strcmp(argv[0], "boot"))
		boot_from(argc, argv);
	else if(!strcmp(argv[0], "cat"))
		cat_file(argc, argv);
	else if(!strcmp(argv[0], "rm"))
		rm_file(argc, argv);
/*
	else if(!strcmp(argv[0], "ota"))
		ota_start(argc, argv);
*/
	else if(!strcmp(argv[0], "echo"))
		echo(argc, argv);
	}
