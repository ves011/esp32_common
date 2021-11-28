/**
 * @file tcp_server.h
 * @brief Structures and functions used by TCP server tasks group
 *
 */

#ifndef TCP_SERVER_H__
#define TCP_SERVER_H__


#define CONNECTION_IN_USE				"\\1"		/**< string sent to client on connection attempt if another client is already connected */

/** @brief messages IDs on task communication */
#define IDENT_REQUEST					0x01		/**< Identification request from user_device, send by broadcast message. Parameters: IP:port of listening TCP server on user device	*/
#define IDENT_ACK						0x02		/**< Identification ACK sent by control device in response to IDENT_REQUEST to IP:port received in IDENT_REQUEST params. Parameters: IP:port of listening TCP server on user device */
#define ADC_VALUE_REQUEST				0x03		/**< Request ADC values. */
#define ADC_VALUE_RESPONSE				0x04		/**< Respose for ADC values request. */
#define TEMP_MON_START					0x05		/**< request start tem monitoring, */
#define TEMP_MON_STOP					0x06		/**< request stop tem monitoring, */
#define SET_GPIO_HIGH					0x07		/**< Set specific GPIO HIGH. */
#define SET_GPIO_LOW					0x07		/**< Set specific GPIO LOW. */
#define GET_GPIO_STATE					0x08		/**< Get GPIO state for a range of GPIOs. */
#define READ_ADC_CHANNEL				0x09		/**< Read specific ADC channel. */

/** @breief priorities for user tasks */
#define USER_TASK_PRIORITY			5

/** @breief events used to wake-up mon_task */
#define CLIENT_IN_EVENT				0x001
#define TIMEOUT_EVENT				0x002
#define CLIENT_OUT_EVENT			0x004
#define STOP_TCP_SERVER				0x008
#define STOP_BCAST_LISTENER			0x010
#define NEW_MESSAGE_IN_QUEUE		0x020




 /** @brief how long a client can remain connected -> seconds */
#define CLIENT_CONNECTION_WINDOW	25

/** @brief tcp socket parameters */
#define DEFAULT_TCP_SERVER_PORT     8001
#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

/** @brief default TCP listening port */
#define CTRL_DEVICE_TCP_PORT		3001
#define USER_DEVICE_TCP_PORT		3000


/** @brief handles for cretated tasks */
extern TaskHandle_t mon_task_handle, cmd_task_handle, server_task_handle, bcl_task_handle;
extern int client_in;
extern esp_netif_ip_info_t local_ip_info;
extern uint32_t valid_token;

/** @struct tcp_server_args
 *  @brief command line arguments for tcp_server command */
struct
	{
    struct arg_str *op;		/**< operation */
	struct arg_int *port;	/**< port to accept incoming connections */
    struct arg_end *end;	/**< linenoise standard end argument */
	} tcp_server_args;

/** @brief struct passed in pvParameters to tcp_server_task  */
typedef struct
	{
	int addr_family;		/*!< AF_INET or AF_INET6 */
	int port;				/*!< port to accept incoming connections */
	} server_task_args_t;

typedef struct
	{
    int timer_group;
    int timer_idx;
    int alarm_interval;
    bool auto_reload;
	} client_timer_info_t;

typedef struct
	{
    client_timer_info_t info;
    uint64_t timer_counter_value;
	} client_timer_event_t;

typedef struct
	{
	uint16_t			messageID;
	uint32_t			token;
	esp_ip4_addr_t		sender_ip;
	uint16_t			sender_port;
	}hmessage_t;

union mval
	{
	uint8_t		bval[8];
	uint16_t	sval[4];
	uint32_t	lval[2];
	uint64_t	llval;
	};

typedef struct 
	{
	hmessage_t	header;
	union mval	messageVal;
	} message_t;

/** @brief Event message queue template */
typedef struct
	{
	message_t	*message;			/**< current message */
	void		*next;				/**< pointer to next message in queue */
	} evt_message_list_t;

typedef struct
	{
	int8_t low_th;
	int8_t high_th;
	uint16_t interval;
	} temp_set_t;

extern evt_message_list_t *message_q;

bool IRAM_ATTR client_timer_callback(void *args);
void cmd_task(void *pvParameters);
void tcp_server_task(void *pvParameters);
void monitor_task(void *pvParameters);

/**
  * @brief TCP server listener task.
  *
  *        Listens on port for incoming connections and spawns cmd_task if client connection is accepted.
  *        While a client is connected other connect request is rejected by sending \1
  *
  * @param argc number of args
  * @param argv pointer
  *
  * @return
  *    - tcp_server_task never returns
  *    - can be killed by tcp_server stop command
  */

int tcp_server(int argc, char **argv);
void tcp_server_task(void *pvParameters);
void monitor_task(void *pvParameters);

void register_tcp_server(void);

#endif
