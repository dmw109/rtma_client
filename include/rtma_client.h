#ifndef _RTMA_CLIENT_H
#define _RTMA_CLIENT_H

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
	#define __WINDOWS__
#else
	#define __UNIX__
#endif


#include "socket.h"
#include <stdint.h>

#define NO_MESSAGE 0
#define GOT_MESSAGE 1
#define DEFAULT_ACK_TIMEOUT 3.0
#define MAX_LOGGER_FILENAME_LENGTH    256
#define BLOCKING -1
#define NONBLOCKING 0
#define MAX_DATA_BYTES 4096

#ifdef __WINDOWS__
#endif //__WINDOWS__

#ifdef __UNIX__
#define TRUE 1
#define FALSE 0

#endif //__UNIX__

#define MSG_TYPE(x) (x.rtma_header.msg_type)

typedef short MODULE_ID;
typedef short HOST_ID;
typedef int MSG_TYPE;

typedef struct {
	sockfd_t sockfd;
	struct sockaddr_storage serv_addr;
	MODULE_ID module_id;
	HOST_ID host_id;
	double start_time;
	int pid;
	int msg_count;
	int connected;
#ifdef __WINDOWS__
	double perf_counter_freq;
#endif
}Client;

typedef struct {
	MSG_TYPE	msg_type;
	int			msg_count;
	double		send_time;
	double		recv_time;
	HOST_ID		src_host_id;
	MODULE_ID	src_mod_id;
	HOST_ID		dest_host_id;
	MODULE_ID	dest_mod_id;
	int			num_data_bytes;
	int			remaining_bytes;
	int			is_dynamic;
	int			reserved;
} RTMA_MSG_HEADER;


typedef struct {
	RTMA_MSG_HEADER rtma_header;
	char data[MAX_DATA_BYTES];
}Message;

typedef MSG_TYPE Signal;

// Module ID-s of core modules
#define MID_MESSAGE_MANAGER     0
#define MID_COMMAND_MODULE      1
#define MID_APPLICATION_MODULE  2
#define MID_NETWORK_RELAY       3
#define MID_STATUS_MODULE       4
#define MID_QUICKLOGGER         5

#define HID_LOCAL_HOST  0
#define HID_ALL_HOSTS   0x7FFF

// Used for subscribing to all message types
#define ALL_MESSAGE_TYPES  0x7FFFFFFF
// Messages sent by MessageManager to modules
#define MT_EXIT						0
#define MT_KILL						1
#define MT_ACKNOWLEDGE				2
#define MT_FAIL_SUBSCRIBE			6
#define MT_FAILED_MESSAGE			8
#define MT_CONNECT					13
#define MT_DISCONNECT				14
#define MT_SUBSCRIBE				15
#define MT_UNSUBSCRIBE				16
#define MT_PAUSE_SUBSCRIPTION		85
#define MT_RESUME_SUBSCRIPTION		86
#define MT_SHUTDOWN_RTMA			17
#define MT_SHUTDOWN_APP				18
#define MT_FORCE_DISCONNECT			82
#define MT_MODULE_READY				26
#define MT_SAVE_MESSAGE_LOG			56
#define MT_MESSAGE_LOG_SAVED		57
#define MT_PAUSE_MESSAGE_LOGGING	58
#define MT_RESUME_MESSAGE_LOGGING	59
#define MT_RESET_MESSAGE_LOG		60
#define MT_DUMP_MESSAGE_LOG			61


typedef struct { 
	MODULE_ID mod_id;
	short reserved; 
	MSG_TYPE msg_type; 
} MDF_FAIL_SUBSCRIBE;


typedef struct {
	MODULE_ID dest_mod_id;
	short reserved[3];
	double time_of_failure;
	RTMA_MSG_HEADER msg_header;
} MDF_FAILED_MESSAGE;


typedef struct {
	short logger_status;
	short daemon_status; 
} MDF_CONNECT;


typedef MSG_TYPE MDF_SUBSCRIBE;
typedef MSG_TYPE MDF_UNSUBSCRIBE;
typedef MSG_TYPE MDF_PAUSE_SUBSCRIPTION;
typedef MSG_TYPE MDF_RESUME_SUBSCRIPTION;

typedef struct { 
	int mod_id; 
} MDF_FORCE_DISCONNECT;

typedef struct { 
	int pid; 
} MDF_MODULE_READY;

typedef struct {
	char pathname[MAX_LOGGER_FILENAME_LENGTH];
	int pathname_length;
} MDF_SAVE_MESSAGE_LOG;

#ifdef __WINDOWS__
	#ifdef _DYNAMIC_LIB
		#ifdef RTMA_C_EXPORTS
			#define RTMA_C_API __declspec(dllexport)
		#else
			#define RTMA_C_API __declspec(dllimport)
		#endif
	#else
		#define RTMA_C_API 
	#endif
#elif defined(__UNIX__) || defined(__APPLE__) || defined(__MACH__) || defined(__LINUX__) || defined(__FreeBSD__)
		#define RTMA_C_API //nothing
#else
	#define RTMA_C_API //nothing
#endif

#ifdef __cplusplus
extern "C" {
#endif

	RTMA_C_API Client* rtma_create_client(MODULE_ID, HOST_ID);
	RTMA_C_API int rtma_client_wait_for_acknowledgement(Client* c, Message* msg, double timeout);
	RTMA_C_API double rtma_client_get_timestamp(Client* c);
	RTMA_C_API void rtma_client_connect(Client* c, char* server_name, uint16_t port);
	RTMA_C_API void rtma_client_send_module_ready(Client* c);
	RTMA_C_API int rtma_client_send_message_to_module(Client* c, MSG_TYPE msg_type, void* msg, size_t len, int dest_mod_id, int dest_host_id, double timeout);
	RTMA_C_API int rtma_client_send_signal_to_module(Client* c, Signal sig_type, int dest_mod_id, int dest_host_id, double timeout);
	RTMA_C_API int rtma_client_send_message(Client* c, MSG_TYPE msg_type, void* msg, size_t len);
	RTMA_C_API int rtma_client_send_signal(Client* c, Signal s);
	RTMA_C_API int rtma_client_read_message(Client* c, Message* msg, double timeout);
	RTMA_C_API void rtma_client_subscribe(Client* c, MSG_TYPE msg_type);
	RTMA_C_API void rtma_client_unsubscribe(Client* c, MSG_TYPE msg_type);
	RTMA_C_API void rtma_client_resume_subscription(Client* c, MSG_TYPE msg_type);
	RTMA_C_API void rtma_client_pause_subscription(Client* c, MSG_TYPE msg_type);
	RTMA_C_API void rtma_client_disconnect(Client* c);
	RTMA_C_API void rtma_destroy_client(Client* c);

	RTMA_C_API void rtma_message_print(Message* msg);

#ifdef __cplusplus
}
#endif

#endif //_RTMA_CLIENT_H
