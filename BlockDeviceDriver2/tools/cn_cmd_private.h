/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * cn_cmd_private.h
 *
 */

#ifndef CN_CMD_PRIVATE_H_
#define CN_CMD_PRIVATE_H_

#define DEFAULT_CN_PORT 9898
#define LOCAL_IPADDR_STR    "127.0.0.1"

int8_t *def_instid = "DEFAULT-INSTID";
uint16_t instid_len;

uint32_t local_ipaddr;
uint32_t ReqID_Generator;

/*
 * packet format for create/clone/delete volume
 * attach: vol_para : volume id
 * detach: vol_para : volume id
 *   stop: vol_para : volume id
 *  start: vol_para : volume id
 */
typedef struct reqpkt_cnvolop {
    uint16_t pktlen;
    uint16_t opcode;
    uint64_t vol_para; //volume id
} __attribute__ ((__packed__)) reqpkt_cnvolop_t;

typedef struct rsppkt_cnvolop {
    uint32_t vol_rsp;
} __attribute__ ((__packed__)) rsppkt_cnvolop_t;

typedef struct reqpkt_cnblockIO {
    uint16_t pktlen;
    uint16_t opcode;
    uint32_t reqID;
    uint64_t vol_id; //volume id
} __attribute__ ((__packed__)) reqpkt_cnblockIO_t;

typedef struct rsppkt_cnblockIO {
    uint16_t pktlen;
    uint16_t ackcode;
    uint32_t reqID;
} __attribute__ ((__packed__)) rsppkt_cnblockIO_t;

typedef reqpkt_cnblockIO_t reqpkt_cncovw_t;
typedef rsppkt_cnblockIO_t rsppkt_cncovw_t;

#endif /* CN_CMD_PRIVATE_H_ */
