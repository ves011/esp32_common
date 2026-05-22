/**
 * @file cmd_wifi.h
 * @brief Wi-Fi management and console command interface
 *
 * This header defines the public interface for Wi-Fi initialization,
 * connection management, credential handling, and console command
 * integration.
 *
 * Responsibilities:
 *  - Initialize and configure the Wi-Fi subsystem
 *  - Manage STA connect / reconnect operations
 *  - Provide access to current network status
 *  - Implement interactive console commands for Wi-Fi control
 *
 * Design notes:
 *  - Runtime Wi-Fi state is synchronized using comm_event_group
 *  - Console commands are optional and enabled mainly for debugging
 *  - No UI (LVGL) logic is implemented here
 *  - This module does not perform any user interaction directly
 */

#ifndef CMD_WIFI_H__
#define CMD_WIFI_H__

#include <stdbool.h>
#include "esp_netif_types.h"
#include "freertos/idf_additions.h"

/* -------------------------------------------------------------
 * Configuration constants
 * ------------------------------------------------------------- */

/**
 * @brief Maximum number of access points returned by a scan
 */
#define SCAN_LIST_SIZE      30

/**
 * @brief Default timeout for Wi-Fi join attempts (milliseconds)
 */
#define JOIN_TIMEOUT_MS     15000

/**
 * @brief Timeout for Wi-Fi disconnect operations (milliseconds)
 */
#define DISCONNECT_TIMEOUT  1000

/* -------------------------------------------------------------
 * Global synchronization objects
 * ------------------------------------------------------------- */

/**
 * @brief Communication event group
 *
 * Used to signal Wi-Fi, IP, and MQTT connectivity changes.
 * Bits are defined in project_specific.h.
 */
extern EventGroupHandle_t comm_event_group;


/* -------------------------------------------------------------
 * Wi-Fi initialization and status
 * ------------------------------------------------------------- */

/**
 * @brief Initialize the Wi-Fi subsystem
 *
 * Configures Wi-Fi interfaces, event handlers, and default parameters.
 * Must be called before attempting to connect or use Wi-Fi services.
 *
 * @param usenvs        Use or NOT NVS to store WiFi connection parameters
 */
void initialise_wifi(bool usenvs);

/**
 * @brief Register Wi-Fi related console commands
 *
 * Installs CLI commands used to inspect and control Wi-Fi behavior.
 * Intended for debug and maintenance builds.
 */
void register_wifi(void);

/**
 * @brief Check if the device is currently connected to a network
 *
 * @return true if connected and IP address is assigned, false otherwise
 */
bool isConnected(void);

/**
 * @brief Attempt to reconnect using stored credentials
 *
 * Uses credentials previously stored in NVS.
 *
 * @return true if reconnection succeeded, false otherwise
 */
bool wifi_reconnect(void);

/**
 * @brief Join a Wi-Fi network
 *
 * Attempts to connect to the specified access point and waits for
 * completion or timeout.
 *
 * @param ssid        Access point SSID (NULL to use stored credentials)
 * @param pass        Access point password (NULL to use stored credentials)
 * @param timeout_ms  Maximum time to wait for connection
 * @param usenvs      Use or NOT NVS to store WiFi connection parameters
 *
 * @return true if connection succeeded, false otherwise
 */
bool wifi_join(const char *ssid, const char *pass, int timeout_ms, bool usenvs);

/* -------------------------------------------------------------
 * Console command handlers
 * ------------------------------------------------------------- */

/**
 * @brief Execute Wi-Fi console command logic
 *
 * Dispatches Wi-Fi related console subcommands such as scan,
 * connect, disconnect, and status.
 *
 * @param argc Argument count
 * @param argv Argument vector
 */
void do_wifi_cmd(int argc, char **argv);

/**
 * @brief Wi-Fi console command entry point
 *
 * Wrapper invoked by the console subsystem when the `wifi`
 * command is executed.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Command execution status
 */
int do_wifi(int argc, char **argv);

bool get_sta_conf(char *ssid, esp_netif_ip_info_t *ipinfo);

/**
 * @brief Retrieve stored STA credentials from NVS
 *
 * @param ssid    Buffer to receive stored SSID
 * @param passwd Buffer to receive stored password
 *
 * @return Number of credentials retrieved or error code
 */
int get_nvs_stacred(char *ssid, char *passwd);

#endif /* CMD_WIFI_H__ */
