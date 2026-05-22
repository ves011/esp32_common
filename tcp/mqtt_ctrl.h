/**
 * @file mqtt_ctrl.h
 * @brief MQTT client control and messaging interface
 *
 * This header defines the public interface for the MQTT subsystem.
 * It provides:
 *
 *  - MQTT topic declarations used throughout the application
 *  - MQTT client startup and registration functions
 *  - Publish and subscribe helper functions
 *  - Access to MQTT connection state
 *
 * Design notes:
 *  - This module abstracts MQTT client operations from application logic.
 *  - Topic strings are allocated and managed by the implementation.
 *  - The UI and control layers communicate via MQTT through this interface.
 *  - This header contains no implementation, only declarations.
 */

#ifndef TCP_MQTT_CTRL_H_
#define TCP_MQTT_CTRL_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/* -------------------------------------------------------------
 * MQTT topic declarations
 * ------------------------------------------------------------- */
#define USER_MQTT_SIZE		48
#define MQTT_TOPIC_SIZE		USER_MQTT_SIZE + 16
/**
 * @brief MQTT topic publishing system state
 */
extern char TOPIC_STATE[MQTT_TOPIC_SIZE];

/**
 * @brief MQTT topic publishing error information
 */
extern char TOPIC_ERROR[MQTT_TOPIC_SIZE];

/**
 * @brief MQTT topic publishing monitoring data
 */
extern char TOPIC_MONITOR[MQTT_TOPIC_SIZE];

/**
 * @brief MQTT topic subscribing to control commands
 */
extern char TOPIC_CTRL[MQTT_TOPIC_SIZE];

/**
 * @brief MQTT topic publishing log messages
 */
extern char TOPIC_LOG[MQTT_TOPIC_SIZE];

/**
 * @brief MQTT topic used for keep-alive / heartbeat messages
 */
extern char TOPIC_KA[MQTT_TOPIC_SIZE];

#if ACTIVE_CONTROLLER == WP_CONTROLLER

/**
 * @brief Additional state topic for water pump controller
 */
extern char TOPIC_STATE_A[MQTT_TOPIC_SIZE];

/**
 * @brief Additional monitor topic for water pump controller
 */
extern char TOPIC_MONITOR_A[MQTT_TOPIC_SIZE];

/**
 * @brief Queue used to forward water-specific control messages
 *
 * This queue is consumed by the water pump control task.
 */
extern QueueHandle_t water_cmd_q;

#endif /* ACTIVE_CONTROLLER == WP_CONTROLLER */

/**
 * @brief app specific function prototype to execute received commands on TOPI_CTRL topic 
 *
 * @param argc number of parameters 
 * @return argv array of parameter strings *argv[argc]
 */
typedef void (*app_cmd_handler_t)(int argc, char **argv);

/* -------------------------------------------------------------
 * MQTT message types
 * ------------------------------------------------------------- */

/**
 * @brief Structure representing a received MQTT message
 *
 * Contains the topic string and payload data.
 */
typedef struct
{
    char *topic;    /**< MQTT topic */
    char *payload;  /**< MQTT payload */
} mqtt_rx_msg_t;

/* -------------------------------------------------------------
 * MQTT lifecycle management
 * ------------------------------------------------------------- */

/**
 * @brief Start the MQTT client
 *
 * Initializes and connects the MQTT client according to the
 * current configuration.
 *
 * @return 0 on success, negative value on failure
 */
int mqtt_start(app_cmd_handler_t app_exec_function);

/**
 * @brief Register MQTT-related console commands
 *
 * Installs CLI commands used for debugging and diagnostics.
 */
void register_mqtt(void);

/* -------------------------------------------------------------
 * Publish / subscribe helpers
 * ------------------------------------------------------------- */

/**
 * @brief Publish the current MQTT client status
 *
 * Typically used to announce connection or availability state.
 */
void publish_MQTT_client_status(void);

/**
 * @brief Subscribe to an MQTT topic
 *
 * @param topic Topic string to subscribe to
 */
void subscribe(char *topic);

/**
 * @brief Publish a unique request or device identifier
 */
void publish_reqID(void);

/**
 * @brief Publish a message to an MQTT topic
 *
 * @param topic  Topic to publish to
 * @param msg    Payload message
 * @param qos    Quality of Service level
 * @param retain Retain flag
 */
void publish_topic(char *topic,
                   char *msg,
                   int qos,
                   int retain);

/**
 * @brief Publish a log message via MQTT
 *
 * @param message Log message payload
 */
void publish_MQTT_client_log(char *message);

/* -------------------------------------------------------------
 * Connection state
 * ------------------------------------------------------------- */

/**
 * @brief Retrieve MQTT connection state
 *
 * @param id Optional identifier buffer (may be NULL)
 * @return Non-zero if connected, zero otherwise
 */
int get_MQTT_connection_state(char *id);



#endif /* TCP_MQTT_CTRL_H_ */
