#ifndef _socket_util_h_
#define _socket_util_h_

#include <netinet/in.h>

#define USER_SPACE_SOCKET_SERVER	1

#ifdef USER_SPACE_SOCKET_SERVER
typedef int socket_t;
#define dms_printf(fmt, args...) printf(fmt, ##args)
#else
typedef struct socket *ksocket_t;
#define dms_printf(fmt, args...) printk(fmt, ##args)
#endif

#define SERVER_MAXCONN		30

socket_t socket_create(int domain, int type, int protocol);
int socket_bind(socket_t sock, struct sockaddr *address, int address_len);
int socket_listen(socket_t socket, int backlog);
socket_t socket_accept(socket_t sock, struct sockaddr *address, int *address_len);
int socket_setsockopt(socket_t sock, int level, int optname, void *optval, int optlen);
int socket_recvfrom(socket_t sock, char * recv_buffer, int recv_len);
int socket_sendto(socket_t sock, char * recv_buffer, int recv_len);

int create_socket_server(int port);
#endif
