/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * user_command.h
 */

#ifndef USER_DAEMON_USER_COMMAND_H_
#define USER_DAEMON_USER_COMMAND_H_

typedef struct disco_userDaemon_prealloc_resp {
    uint16_t pkt_len;
    uint16_t ack_code;
    uint32_t requestID;
} __attribute__((__packed__)) UserD_prealloc_resp_t;

typedef struct disco_userDaemon_invalfs_resp {
    uint16_t pkt_len;
    uint16_t ack_code;
    uint32_t requestID;
    uint64_t maxUnuse_hbid;
} __attribute__((__packed__)) UserD_invalfs_resp_t;

#endif /* USER_DAEMON_USER_COMMAND_H_ */
