#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include "socket_util.h"

//Note: domain: AF_INET, type: SOCK_STREAM, protocol: IPPROTO_TCP
socket_t socket_create(int domain, int type, int protocol) {
	socket_t sk;
	int ret;

#ifdef USER_SPACE_SOCKET_SERVER
	sk = 0;
	sk = socket(domain, type, protocol);
	ret = sk;
#else
	sk = NULL;
	ret = sock_create(domain, type, protocol, &sk);
#endif

	if(ret < 0) {
		dms_printf("socket_create fail\n");
	}


	return sk;
}

int socket_bind(socket_t sock, struct sockaddr *address, int address_len) {
	int ret = 0;

#ifdef USER_SPACE_SOCKET_SERVER
	ret = bind(sock, address, address_len);
#else
	ret = sock->ops->bind(sock, address, address_len);
#endif

	if(ret < 0) {
		dms_printf("socket_bind fail\n");
	}

	return ret;
}

int socket_listen(socket_t sock, int backlog) {
	int ret = 0;
	if ((unsigned)backlog > SERVER_MAXCONN) {
		dms_printf("socket_listen over max server connection limit, using default %d\n", SERVER_MAXCONN);
		backlog = SERVER_MAXCONN;
	}

#ifdef USER_SPACE_SOCKET_SERVER
	ret = listen(sock, backlog);
#else
	ret = sk->ops->listen(sock, backlog);
#endif

	if(ret < 0) {
		dms_printf("socket_listen fail\n");
	}

	return ret;
}

socket_t socket_accept(socket_t sock, struct sockaddr *address, int *address_len) {
	struct sockaddr_in client_addr;
	int addrlen = sizeof(client_addr);
	int ret = 0;
	socket_t clientfd;

#ifdef USER_SPACE_SOCKET_SERVER
	clientfd = accept(sock, (struct sockaddr*)&client_addr, &addrlen);
	ret = clientfd;
#else
	ret = sock_create(sock->sk->sk_family, sock->type, sock->sk->sk_protocol, &clientfd);
	if (ret < 0) {
		dms_printf("socket_accept fail\n");
		return NULL;
	}
	if (!clientfd) {
		dms_printf("socket_accept fail\n");
		return NULL;
	}

	if (ret < 0)
		goto error_kaccept;

	clientfd->type = sock->type;
	clientfd->ops = sock->ops;

	ret = sock->ops->accept(sock, clientfd, 0 /*sk->file->f_flags*/);

	if (address)
	{
		ret = clientfd->ops->getname(clientfd, address, address_len, 2);
		if (ret < 0)
			goto error_kaccept;
	}
#endif

	if(ret < 0) {
		dms_printf("socket_accept fail\n");
	}

	return clientfd;
}

int socket_setsockopt(socket_t sock, int level, int optname, void *optval, int optlen) {
	int ret = 0;

	ret = setsockopt(sock, level, optname, (char *)optval, optlen);

	if(ret < 0) {
		dms_printf("socket_setsockopt fail\n");
	}

	return ret;
}

int socket_recvfrom(socket_t sock, char * recv_buffer, int recv_len) {
	char * recv_ptr = recv_buffer;
	int local_rcv_len = 0, res = 0;

	while(local_rcv_len < recv_len) {
		res = recv(sock, recv_ptr, recv_len - local_rcv_len, MSG_WAITALL);
		if(res < 0) {
			if(errno != EINTR && errno != EAGAIN) {
				syslog(LOG_ERR, "socket_recvfrom ERROR: errno %d\n", errno);
				break;
			}
		} else if(res == 0) {
			syslog(LOG_ERR, "socket_recvfrom receive 0 bytes and errno %d\n", errno);
			return 0;
		}

		local_rcv_len = local_rcv_len + res;
		recv_ptr = recv_ptr + res;
	}

	return local_rcv_len;
}

int socket_sendto(socket_t sock, char * send_buffer, int send_len) {
	char * send_ptr = send_buffer;
	int local_send_len = 0, res = 0, ret = 0;

	while(local_send_len < send_len) {
		res = write(sock, send_ptr, send_len - local_send_len);

		if(res < 0) {
			if(errno != EINTR) {
				syslog(LOG_ERR, "socket_sendto ERROR: errno %d\n", errno);
				break;
			}
		}

		local_send_len += res;
		send_ptr = send_ptr + res;
	}

	if(local_send_len != send_len)
		return -1;
	return send_len;
}

int create_socket_server(int port) {
	socket_t serv_sock;
	struct sockaddr_in dest;
	int sock_opt = 0;

	/* create socket */
	serv_sock = socket_create(AF_INET, SOCK_STREAM, 0);

	if(serv_sock == -1)
		return -1;

	socket_setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sock_opt, sizeof(sock_opt));

	/* initialize structure dest */
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);

	/* this line is different from client */
	dest.sin_addr.s_addr = INADDR_ANY;

	/* assign a port number to socket */
	socket_bind(serv_sock, (struct sockaddr*)&dest, sizeof(dest));

	/* make it listen to socket with max 20 connections */
	socket_listen(serv_sock, SERVER_MAXCONN);

	return serv_sock;
}

int shutdown_socket_server(socket_t sock) {
	int res;

	res = shutdown(sock, SHUT_RDWR);
	res = close(sock);

	return res;
}

int socket_connect_to(char * ipaddr, int port) {
	struct sockaddr_in address;
	socket_t sockfd;
	int ret;

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = inet_addr(ipaddr);
	bzero( &(address.sin_zero), 8 );

	sockfd = socket_create(AF_INET, SOCK_STREAM, 0);

	if(sockfd < 0) {
		syslog(LOG_ERR, "Cannot create socket");
		return -1;
	}

	ret = connect(sockfd, (struct sockaddr*)&address, sizeof(struct sockaddr));
	if(ret < 0) {
		syslog(LOG_ERR, "Cannot connect to target\n");
		close(sockfd);
		sockfd = -1;
	}

	return sockfd;
}

int socket_close(socket_t sock) {
	int ret;

	ret = shutdown(sock, SHUT_RDWR);
	ret = close(sock);

	return ret;
}
