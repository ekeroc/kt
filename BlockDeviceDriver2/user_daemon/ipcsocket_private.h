/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 * ipcsocket_private.h
 *
 */

#ifndef _IPCSOCKET_PRIVATE_H_
#define _IPCSOCKET_PRIVATE_H_

#define htonll(x) \
((((x) & 0xff00000000000000LL) >> 56) | \
(((x) & 0x00ff000000000000LL) >> 40) | \
(((x) & 0x0000ff0000000000LL) >> 24) | \
(((x) & 0x000000ff00000000LL) >> 8) | \
(((x) & 0x00000000ff000000LL) << 8) | \
(((x) & 0x0000000000ff0000LL) << 24) | \
(((x) & 0x000000000000ff00LL) << 40) | \
(((x) & 0x00000000000000ffLL) << 56))

#define ntohll(x)   htonll(x)
#define MAX_NUM_OF_ACCEPT_SOCKET    1024
#define MAGIC_NUM 0xa5a5a5a5a5a5a5a5ll

#define NN_REQ_PACKET_SIZE  14
#define NN_VOL_INFO_RESP_SIZE   14
#define INVALID_SOCK_FD -1
#define MAX_WAIT_TIME_DEV_FILE  300
#define DEV_FULL_PATH_PREFIX    "/dev/"

#define DO_ATTACH   1
#define DO_DETACH   0

#define MAX_CMD_PKTLEN  255

pthread_mutex_t g_client_conn_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_attach_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_nn_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

int global_client_fd[MAX_NUM_OF_ACCEPT_SOCKET];
int disk_read_ahead;

int dms_sk_server_fd;

detach_response_t detach_volume(int8_t *nnName, uint32_t nnIPAddr, int32_t vol_id, int8_t *instid);
#endif /* _IPCSOCKET_PRIVATE_H_ */
