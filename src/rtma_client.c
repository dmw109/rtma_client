#include "rtma_client.h"

double rtma_client_get_timestamp(Client *c){
#ifdef _UNIX_C
    struct timeval tim;
    if ( gettimeofday(&tim, NULL)  == 0 )
    {
        double t = tim.tv_sec + (tim.tv_usec/1000000.0);
        return t;
    }else{
        return 0.0;
    }
#else
    LONGLONG current_time;
    QueryPerformanceCounter( (LARGE_INTEGER*) &current_time);
    return (double) current_time / c->perf_counter_freq;
#endif
}

Client* rtma_create_client(MODULE_ID module_id, HOST_ID host_id) {
	#ifdef _WINDOWS_C
	winsock_init();
	#endif //_WINDOWS_C

	Client* c = (Client *)malloc(sizeof(Client));

	if (c == NULL) {
		perror("rtma_create_client:malloc failed");
		exit(EXIT_FAILURE);
	}

	c->sockfd = -1;
	c->module_id = module_id;
	c->host_id = host_id;

	#ifdef _UNIX_C
		c->pid = getpid();
	#endif

	#ifdef _WINDOWS_C
		c->pid = _getpid();
	#endif

	c->msg_count = 0;
	c->connected = 0;

	#ifdef _WINDOWS_C
		LONGLONG freq;
		QueryPerformanceFrequency( (LARGE_INTEGER*) &freq);
		c->perf_counter_freq = (double) freq;
	#endif

	// Start time is set after connect is called
	c->start_time = 0.0;

	return c;
}

void rtma_destroy_client(Client *c) {
	if (c == NULL)
		return;

	// Close the underlying socket
	if (c->sockfd != INVALID_SOCKET) {
		socket_shutdown(c->sockfd, SD_BOTH);
		socket_close(c->sockfd);
	}
	
	// Free the Client struct
	free(c);

#ifdef _WINDOWS_C
	winsock_cleanup();
#endif //_WINDOWS_C
}

void rtma_client_connect(Client *c, char* server_name, uint16_t port) {
	struct addrinfo hints;
	struct addrinfo* res = NULL;

	memset(&hints, '\0', sizeof hints);

	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%d", port);

	int ret = getaddrinfo(server_name, port_str, &hints, &res);

	if (ret) {
		perror(gai_strerror(ret));
		exit(1);
	}

	c->sockfd = socket_create(res->ai_family, res->ai_socktype, res->ai_protocol);
	socket_connect(c->sockfd, res->ai_addr, res->ai_addrlen);
	
	int optval = TRUE;
	socket_setsockopt(c->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	socket_setsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

	int opt = 0;
	socklen_t optlen = sizeof(opt);
	socket_getsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, &optlen);

	if (opt != TRUE) {
		perror("Unable to set TCP_NODELAY socket option.\n");
		exit(1);
	}

	memset(&c->serv_addr, '\0', sizeof(c->serv_addr));
	memcpy(&c->serv_addr, res->ai_addr, res->ai_addrlen);

	freeaddrinfo(res);

	MDF_CONNECT msg = { .logger_status = 0, .daemon_status = 0 };
	rtma_client_send_message(c, MT_CONNECT, &msg, sizeof(MDF_CONNECT));

	Message ack_msg;
	if (rtma_client_wait_for_acknowledgement(c, &ack_msg, DEFAULT_ACK_TIMEOUT)) {
		c->connected = 1;

		if (c->module_id == 0) {
			//Save own module ID from ACK if asked to be assigned dynamic ID
			c->module_id = ack_msg.rtma_header.dest_mod_id;
			//rtma_message_free(ack_msg);
		}
	}
	else {
		rtma_destroy_client(c);
		perror("rtma_client_connect:Failed to receive acknowledgement from server.\n");
	}
}

void rtma_client_disconnect(Client *c) {
	if (c == NULL)
		return;

	// Close the underlying socket
	if (c->sockfd != INVALID_SOCKET) {
		rtma_client_send_signal(c, MT_DISCONNECT);
		socket_shutdown(c->sockfd, SD_BOTH);
		socket_close(c->sockfd);
		c->sockfd = INVALID_SOCKET;
		memset(&c->serv_addr, '\0', sizeof(c->serv_addr));
		c->module_id = 0;
		c->host_id = 0;
		c->start_time = 0.0;
		c->msg_count = 0;
		c->connected = 0;
	}
}

int rtma_client_send_message(Client *c, MSG_TYPE msg_type, void* data, size_t len) {
	Message msg;
	
	msg.rtma_header.msg_type = msg_type;
	msg.rtma_header.msg_count = ++(c->msg_count);
	msg.rtma_header.send_time = rtma_client_get_timestamp(c);
	msg.rtma_header.recv_time = 0.0;
	msg.rtma_header.src_host_id = c->host_id;
	msg.rtma_header.src_mod_id = c->module_id;
	msg.rtma_header.dest_host_id = 0;
	msg.rtma_header.dest_mod_id = 0;
	msg.rtma_header.num_data_bytes = len;
	msg.rtma_header.is_dynamic = 0;
	msg.rtma_header.reserved = 0;

	// Copy the user data into message struct buffer.
	if (len > 0){
		if (len < sizeof(msg.data))
			memcpy(msg.data, data, len);
		else {
			perror("rtma_client_send_message: data is too large.\n");
			exit(1);
		}
	}

	struct timeval wait, * pWait;
	double timeout = -1;
	if (timeout < 0) { // Negative timeout value means we are willing to wait forever
		pWait = NULL;
	}
	else {
		wait.tv_sec = (long)timeout;
		double remainder = timeout - ((double)(wait.tv_sec));
		wait.tv_usec = (long)(remainder * 1000000.0);
		pWait = &wait;
	}

	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(c->sockfd, &writefds);
	int nfds = c->sockfd + 1;

	int nbytes = 0;

	// Wait for socket
	int status = select(nfds, NULL, &writefds, NULL, pWait);

	if (status == SOCKET_ERROR)
		socket_error();
	if (status == 0)
		return NO_MESSAGE;
	if (status > 0) {
		if (FD_ISSET(c->sockfd, &writefds)) {
			nbytes = socket_sendall(c->sockfd, (char*)&msg, sizeof(msg.rtma_header) + len, 0);
		}
		else {
			//Socket could not accept data without blocking, data discarded!
			printf("x");
		}
	}
	
	return nbytes;
}

int rtma_client_send_message_to_module(Client *c, MSG_TYPE msg_type, void* data, size_t len, int dest_mod_id, int dest_host_id) {
	Message msg;

	msg.rtma_header.msg_type = msg_type;
	msg.rtma_header.msg_count = ++(c->msg_count);
	msg.rtma_header.send_time = rtma_client_get_timestamp(c);
	msg.rtma_header.recv_time = 0.0;
	msg.rtma_header.src_host_id = c->host_id;
	msg.rtma_header.src_mod_id = c->module_id;
	msg.rtma_header.dest_host_id = dest_host_id;
	msg.rtma_header.dest_mod_id = dest_mod_id;
	msg.rtma_header.num_data_bytes = len;
	msg.rtma_header.is_dynamic = 0;
	msg.rtma_header.reserved = 0;

	// Copy the user data into message struct buffer.
	if (len > 0){
		if (len < sizeof(msg.data))
			memcpy(msg.data, data, len);
		else {
			perror("rtma_client_send_message: data is too large.\n");
			exit(1);
		}
	}

	rtma_client_send_message(c, msg_type, data, len);
}

int  rtma_client_send_signal(Client *c, Signal sig_type) {
	int nbytes = rtma_client_send_message(c, sig_type, NULL, 0);

	return nbytes;
}

int rtma_client_read_message(Client *c, Message *msg, double timeout) {
	struct timeval wait, * pWait;
	if (timeout < 0) { // Negative timeout value means we are willing to wait forever
		pWait = NULL;
	}
	else {
		wait.tv_sec = (long)timeout;
		double remainder = timeout - ((double)(wait.tv_sec));
		wait.tv_usec = (long)(remainder * 1000000.0);
		pWait = &wait;
	}

	//Setup select file descriptors
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(c->sockfd, &readfds);
	int nfds = c->sockfd + 1; //This argument is ignored in windows

	// Wait for message header
	int status = select(nfds, &readfds, NULL, NULL, pWait);
	if (status == SOCKET_ERROR)
		socket_error();
	if (status == 0)
		return NO_MESSAGE;
	if (status > 0) {
		if (FD_ISSET(c->sockfd, &readfds)) {
			int bytes_left = sizeof(RTMA_MSG_HEADER);
			char* buf = (char*)&(msg->rtma_header);
			int bytes_read = socket_recv(c->sockfd, buf, bytes_left, MSG_WAITALL);

			if (bytes_read != bytes_left) {
				fprintf(stderr, "Something went wrong in recv:header\n");
				exit(-1);
			}
		}
		else
			return NO_MESSAGE;
	}

	// Check for data portion of message (Always blocking)
	if (msg->rtma_header.num_data_bytes > 0) {
		int bytes_left = msg->rtma_header.num_data_bytes;
		char* buf = msg->data;
		int bytes_read = socket_recv(c->sockfd, buf, bytes_left, MSG_WAITALL);
		
		if (bytes_read != bytes_left) {
			fprintf(stderr, "Something went wrong recv:data\n");
			exit(-1);
		}

	}

	// Add timestamp to header
	msg->rtma_header.recv_time = rtma_client_get_timestamp(c);

	return GOT_MESSAGE;
}

int rtma_client_wait_for_acknowledgement(Client *c, Message *msg, double timeout) {
	double start = rtma_client_get_timestamp(c);
	double time_remaining = start;

	//printf("Waiting for Ack...\n");

	while (time_remaining > 0) {
		if (rtma_client_read_message(c, msg, 3.0)) {
			if (msg->rtma_header.msg_type == MT_ACKNOWLEDGE) {
				//printf("Got ACK!\n");
				return GOT_MESSAGE;
			}
		}
		double time_waited = rtma_client_get_timestamp(c) - start;
		time_remaining = timeout - time_waited;
	}

	return NO_MESSAGE;
}

void rtma_client_send_module_ready(Client *c) {
	MDF_MODULE_READY msg;
	msg.pid = c->pid;
	rtma_client_send_message(c, MT_MODULE_READY, &msg, sizeof(msg));
}

void rtma_client_subscribe(Client *c, MSG_TYPE msg_type) {
	MDF_SUBSCRIBE msg = msg_type;
	rtma_client_send_message(c, MT_SUBSCRIBE, &msg, sizeof(msg));
	Message ack_msg;
	rtma_client_wait_for_acknowledgement(c, &ack_msg, DEFAULT_ACK_TIMEOUT);
}

void rtma_client_unsubscribe(Client *c, MSG_TYPE msg_type) {
	MDF_UNSUBSCRIBE msg = msg_type;
	rtma_client_send_message(c, MT_UNSUBSCRIBE, &msg, sizeof(msg));
	Message ack_msg;
	rtma_client_wait_for_acknowledgement(c, &ack_msg, DEFAULT_ACK_TIMEOUT);
}

void rtma_client_resume_subscription(Client *c, MSG_TYPE msg_type) {
	MDF_RESUME_SUBSCRIPTION msg = msg_type;
	rtma_client_send_message(c, MT_RESUME_SUBSCRIPTION, &msg, sizeof(msg));
	Message ack_msg;
	rtma_client_wait_for_acknowledgement(c, &ack_msg, DEFAULT_ACK_TIMEOUT);
}

void rtma_client_pause_subscription(Client *c, MSG_TYPE msg_type) {
	MDF_PAUSE_SUBSCRIPTION msg = msg_type;
	rtma_client_send_message(c, MT_PAUSE_SUBSCRIPTION, &msg, sizeof(msg));
	Message ack_msg;
	rtma_client_wait_for_acknowledgement(c, &ack_msg, DEFAULT_ACK_TIMEOUT);
}

void rtma_message_print(Message* msg) {
	if (msg == NULL)
		return;

	printf("-----RTMA Header-----\n\n");
	printf("msg_type:\t\t%d\n", msg->rtma_header.msg_type);
	printf("msg_count:\t\t%d\n", msg->rtma_header.msg_count);
	printf("send_time:\t\t%0.6lf\n", msg->rtma_header.send_time);
	printf("recv_time:\t\t%0.6lf\n", msg->rtma_header.recv_time);
	printf("src_host_id:\t\t%d\n", msg->rtma_header.src_host_id);
	printf("src_mod_id:\t\t%d\n", msg->rtma_header.src_mod_id);
	printf("dest_host_id:\t\t%d\n", msg->rtma_header.dest_host_id);
	printf("dest_mod_id:\t\t%d\n", msg->rtma_header.dest_mod_id);
	printf("num_data_bytes:\t\t%d\n", msg->rtma_header.num_data_bytes);
	printf("is_dynamic:\t\t%d\n", msg->rtma_header.is_dynamic);
	printf("reserved:\t\t%d\n", msg->rtma_header.reserved);

	if (!msg->rtma_header.num_data_bytes)
		return;

	printf("\n-----Data-----\n\n");

	char* data = msg->data;
	int num_rows = msg->rtma_header.num_data_bytes / 8;
	for (int i = 0; i < num_rows; i++) {
		printf("%04d: %02x %02x %02x %02x %02x %02x %02x %02x | %c%c%c%c%c%c%c%c\n",
			i * 8,
			data[i * 8],
			data[i * 8 + 1],
			data[i * 8 + 2],
			data[i * 8 + 3],
			data[i * 8 + 4],
			data[i * 8 + 5],
			data[i * 8 + 6],
			data[i * 8 + 7],
			data[i * 8],
			data[i * 8 + 1],
			data[i * 8 + 2],
			data[i * 8 + 3],
			data[i * 8 + 4],
			data[i * 8 + 5],
			data[i * 8 + 6],
			data[i * 8 + 7]);
	}

	int remainder = msg->rtma_header.num_data_bytes - (num_rows * 8);
	if (remainder) {
		printf("%04d: ", num_rows * 8);
		for (int i = 0; i < remainder; i++) {
			printf("%02x", data[num_rows * 8 + i]);
		}
	}
		
	printf("\n");
	

}
