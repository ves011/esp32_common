/*
frame format:
  - header (manadtory) | payload (optional)
  - all fields in the frame are separated by \1 (\x01\..) including header from payload
  - all fields in the header are ASCII UTF-8
*/

#ifndef APP_PROTO_H_
#define APP_PROTO_H_
  
#include "inttypes.h"  

#define SEP '\1'
#define MAX_PARAMS 		10
#define MAX_TOKENS 		32
#define PROTO_VERSION	1

typedef struct 
  	{
    int version;
    uint8_t hdr_fields;
    uint32_t payload_len;
    uint64_t timestamp;
    char *command;
    uint8_t nparams;
    char *params[MAX_PARAMS];
    uint8_t *payload;
	} app_proto_t;
	

/*
header format

version\1number of fields in the header\1length of payload\1timestamp\1command\1parmeter[1]\1...parameter[n]\1<payload...>

  - version --> shall be convertible to number to allow simple assesment
  - numer of fields in the header 	--> number of \1 separated fields in the header including version, numer of fields, length...
  									--> it shall be >= 5
  - length of payload -> number of bytes following last parameter excluding \1 sparator
  - timestamp	--> some kind of timestamp which can be used to order partial chunk messages (ms from boot or unix time) 
  - command --> command
  - parameters --> list of parametres associated with the command
  - payload --> stream of bytes of payload length bytes  
*/  

//Parameters
	#define	PAR_PROGRESS	"progress"
	#define PAR_ERROR		"error"
	
//Operations
	#define OP_UPLOAD		"upload"
	#define OP_DOWNLOAD		"download"
	#define OP_UPDATEKEY	"update key"
	
//Commands
#define CMD_SETBOOT						"set boot"
	//number of fields in the header	= 6
	//param1							= part name
	//payload							= none
	
#define CMD_ERASEPART					"erase partition"
	//number of fields in the header	= 6
	//param1							= part name
	//payload							= none
	
#define CMD_REQCONF						"request confirmation"
	//number of fields in the header	= 7
	//param1							= cmd to ask
	//param2							= param1 of cmd to ask
	//payload							= none
	
#define CMD_UPDATEKEYREQ				"update key req"
	//number of fields in the header	= 9
	//param1							= namespace ID
	//param2							= key ID
	//param3							= key len
	//param4							= no of chunks --> to be removed
	//payload							= none
	
#define CMD_UPDATEKEYVAL				"update key val"
	//number of fields in the header	= 9
	//param1							= namespace ID
	//param1							= key ID
	//param2							= chunk no 		--> to be replaced by byte offset
	//param3							= chunk size 	--> overlap with payload_length - to be removed
	//payload							= stream of bytes; size = of chunk size
	
#define CMD_UPDATEKEY					"update key"		
	//number of fields in the header	= 8
	//param1							= namespace ID
	//param2							= key ID
	//param3							= key type
	//payload							= stream of bytes: key value


#define CMD_DELTENS						"delete ns"
	//number of fields in the header	= 6 
	//param1							= namespace name
	//payload							= none

#define CMD_DELETEKEY					"delete key"
	
#define CMD_CREATEKEY					"create key"
	//number of fields in the header	= 10
	//param1							= namespace name
	//param2							= key name
	//param3							= key type
	//param4							= key length
	//payload							= placeholder or value
	//									  if payload length < key length placeholder is duplicated up to key length
	//									  if payload length > key length placeholder is truncated up to key length
	
#define CMD_REBOOT					"esp_reboot"
	//number of fields in the header	= 5
	//params							= none
	//payload							= none


//Responses	
#define	RSP_CMD							"rsp_cmd"
	//number of fields in the header	= 9
	//param1							= responded command
	//param2							= param1 of responded command
	//param3							= error code
	//param4							= error txt
	//payload							= none
	
#define	RSP_CONFIRMATION				"rsp_conf"
	//number of fields in the header	= 8
	//param1							= command asked for
	//param2							= param1 of command asked for
	//param3							= OK|KO
	//payload							= none

//Unsolicited responses
#define	URC_STATUS						"urc_status"
	//number of fields in the header	= 9
	//param1							= operation (upload/download/...)
	//param1							= PAR_PROFRESS | PAR_ERROR
	//param2							= %completed | error code
	//param3							= error txt
	//payload							= none

void build_app_proto_msg(app_proto_t *msg, char *command, int nparams, char **params, int payload_len, uint8_t *payload);
int build_app_proto(uint8_t *out_buf, int max_len, const app_proto_t *msg, int *out_len);
int parse_app_proto(uint8_t *buf, int len, app_proto_t *out);


#endif	/*APP_PROTO_H_*/
	