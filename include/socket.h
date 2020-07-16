#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef _WINDOWS_C

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET sockfd_t;
typedef int sa_family_t;
typedef int socklen_t;

#define gai_strerror(x) gai_strerrorA(x)

#pragma comment(lib, "Ws2_32.lib")

void winsock_init();
void winsock_cleanup();

#endif //_WINDOWS_C

#ifdef _UNIX_C

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket(x) close(x)

typedef int sockfd_t;

#define SD_BOTH SHUT_RDWR
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD

#endif //_UNIX_C

#ifdef __cplusplus
extern "C" {
#endif

/* SOCKET FUNCTIONS */

sockfd_t socket_create(int family, int type, int protocol);
void socket_connect(sockfd_t sockfd, const struct sockaddr* servaddr, socklen_t addrlen);
void socket_bind(sockfd_t sockfd, const struct sockaddr* addr, socklen_t addrlen);
void socket_listen(sockfd_t sockfd);
sockfd_t socket_accept(sockfd_t sockfd, struct sockaddr* addr, socklen_t* addrlen);
void socket_close(sockfd_t sockfd);
void socket_shutdown(sockfd_t sockfd, int how);
int socket_recv(sockfd_t sockfd, char* buf, int len, int flags);
int socket_send(sockfd_t sockfd, const char* buf, int len, int flags);
int socket_sendall(sockfd_t sockfd, const char* buf, int len, int flags);
void socket_setsockopt(sockfd_t sockfd, int level, int optname, int* optval, socklen_t optlen);
void socket_getsockopt(sockfd_t sockfd, int level, int optname, int* optval, socklen_t* optlen);

/* HELPER FUNCTIONS */
void socket_error(void);
sa_family_t get_address_family(const char* node);

#ifdef __cplusplus
}
#endif

#endif //_SOCKET_H
