/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * ipcsocket.h
 */
#ifndef _IPCSOCKET_H_
#define _IPCSOCKET_H_

#define SERVER_PORT 9898

//TODO fix the hard code in ipsocket.c
#define DMS_CMD_ATTACH_VOLUME                   1
#define DMS_CMD_DETACH_VOLUME                   2
#define DMS_CMD_ATTACH_VOLUME_FOR_MIGRATION     3
#define DMS_CMD_DETACH_VOLUME_FOR_MIGRATION     4
#define DMS_CMD_CANCEL_ATTACH_OPERATION         5
#define DMS_CMD_ACK_POLICY_DISK_RESPONSE        8
#define DMS_CMD_RESET_OVERWRITTEN_FLAG          11
#define DMS_CMD_RESET_METADATA_CACHE            12
#define DMS_CMD_BLOCK_VOLUME_IO                 13
#define DMS_CMD_UNBLOCK_VOLUME_IO               14
#define DMS_CMD_SET_VOLUME_SHARE_FEATURE        15
#define DMS_CMD_STOP_VOL                        16
#define DMS_CMD_RESTART_VOL                     17
#define DMS_CMD_GET_CN_INFO                     18

#define DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK    19
#define DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK       20
#define DMS_CMD_FLUSH_VOLUME_FREE_CHUNK         21
#define DMS_CMD_PREALLOC                        22

#define DMS_CLIENT_TEST                         101
#define READ_DATA_PAGE_SIZE                     4096

#define TIME_WAIT_DRV_UNLOAD  30

#define MAX_GETHOSTNAME_RETRY_COUNT 5
#define MAX_CONNECTION_RETRY_COUNT  5

#define REQ_REQUEST_FOR_VOLUME_INFO             1003
#define REQ_DETCH_VOLUME                        1005

extern int disk_read_ahead;

extern void check_namenode_connection(struct hostent * host);
extern void create_socket_server(void);
extern void stop_socket_server(void);

#endif
