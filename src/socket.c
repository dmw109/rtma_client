#include <stdio.h>
#include "socket.h"

#ifdef _WINDOWS_C

// Initialize Winsock DLL
void winsock_init() {
	WSADATA wsaData;
	int err;

	// Winsock version 2.2
	WORD version_required = MAKEWORD(2, 0);

	err = WSAStartup(version_required, &wsaData);
	switch (err) {
		case WSASYSNOTREADY:
			perror("WSAStartup: Underlying network subsystem is not ready for network communication.\n");
			break;
		case WSAVERNOTSUPPORTED:
			perror("WSAStartup: The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation.\n");
			break;
		case WSAEINPROGRESS:
			perror("WSAStartup: A blocking Windows Sockets 1.1 operation is in progress.\n");
	 		break;
		case WSAEPROCLIM:
			perror("WSAStartup: A limit on the number of tasks supported by the Windows Sockets implementation has been reached.\n");
	 		break;
		case WSAEFAULT:
			perror("WSAStartup: The lpWSAData parameter is not a valid pointer.\n");
			break;
	}

	if (err != 0) {
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Confirm that winsock DLL supports version 2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
		perror("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		exit(EXIT_FAILURE);
	}
}

//Cleanup winsock
void winsock_cleanup() {
	WSACleanup();
}

//Print an error message and kill the program
void socket_error() {
	char    msg_buf[512];	// Buffer for text.
    DWORD   nchars;			// Number of chars returned.
	
	int err_code = WSAGetLastError();
	
    // Try to get the error message from the system errors.
    nchars = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             err_code,
                             0,
                             (LPSTR)msg_buf,
                             512,
                             NULL );
	
	if (nchars > 0)
		fprintf(stderr, "%s", msg_buf);
	else
		fprintf(stderr,"No message for Error Code %d found.\n", err_code);


	winsock_cleanup();
}

#endif //_WINDOWS_C

#ifdef _UNIX_C

void socket_error() {
	perror(strerror(errno));
	exit(EXIT_FAILURE);
}

#endif //_UNIX_C

/* SOCKET FUNCTIONS */

sockfd_t socket_create(int family, int type, int protocol) {
	sockfd_t sockfd = socket(family, type, protocol);

	if (sockfd == INVALID_SOCKET) {
		socket_error();
	}

	return sockfd;
}

void socket_connect(sockfd_t sockfd, const struct sockaddr *servaddr, socklen_t addrlen) {
	if (connect(sockfd, servaddr, addrlen) == SOCKET_ERROR) {
		socket_error();
	}
}

void socket_bind(sockfd_t sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	if (bind(sockfd, addr, addrlen) == SOCKET_ERROR) 	
		socket_error();
}

void socket_listen(sockfd_t sockfd) {
	if(listen(sockfd, 1024) == SOCKET_ERROR)
		socket_error();
}

sockfd_t socket_accept(sockfd_t sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	sockfd_t connfd = accept(sockfd, addr, addrlen);
	if (connfd == SOCKET_ERROR)
		socket_error();

	return connfd;
}

void socket_close(sockfd_t sockfd) {
	if (closesocket(sockfd) == SOCKET_ERROR)
	   	socket_error();
}

void socket_shutdown(sockfd_t sockfd, int how) {
	//how: SD_SEND, SD_RECV, SD_BOTH
	if (shutdown(sockfd, how) == SOCKET_ERROR)
		socket_error();
}

int socket_recv(sockfd_t sockfd,  char *buf, int len, int flags) {
	int nbytes = recv(sockfd, buf, len, flags);
	if (nbytes == SOCKET_ERROR)
		socket_error();
	else
		return nbytes;
}

int socket_send(sockfd_t sockfd, const char *buf, int len, int flags) {
	//flags: MSG_OOB, MSG_DONTROUTE
	int nbytes = send(sockfd, buf, len, flags);
	if (nbytes == SOCKET_ERROR)
		socket_error();
	else
		return nbytes;
}

int socket_sendall(sockfd_t sockfd, const char* buf, int len, int flags) {
	//flags: MSG_OOB, MSG_DONTROUTE
	int bytes_sent = 0;
	int buf_len = len;

	while (bytes_sent < len) {
		bytes_sent += send(sockfd, buf, buf_len, flags);
		buf += bytes_sent;
		buf_len -= bytes_sent;
	}

	return bytes_sent;
}

#ifdef _WINDOWS_C
	#define OPTVAL_CAST(x) (char *)(x)
#else
	#define OPTVAL_CAST(x) (void *)(x)
#endif

void socket_setsockopt(sockfd_t sockfd, int level, int optname, int *optval, socklen_t optlen) {
	if (setsockopt(sockfd, level, optname, OPTVAL_CAST(optval), optlen) == SOCKET_ERROR)
		socket_error();
}

void socket_getsockopt(sockfd_t sockfd, int level, int optname, int *optval, socklen_t* optlen) {
	if (getsockopt(sockfd, level, optname, OPTVAL_CAST(optval), optlen) == SOCKET_ERROR)
		socket_error();
}

/* HELPER FUNCTIONS */

sa_family_t get_address_family(const char *node) {
	struct addrinfo hints;
	struct addrinfo *res = NULL;

	memset(&hints, '\0', sizeof hints);

	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int ret = getaddrinfo(node, NULL, &hints, &res);

	if (ret) {
		perror(gai_strerrorA(ret));
		exit(1);
	}
	
	sa_family_t addr_family = res->ai_family;
	freeaddrinfo(res);

	return addr_family;
}
