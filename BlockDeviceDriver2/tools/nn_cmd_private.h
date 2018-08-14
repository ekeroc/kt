/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * nn_cmd_private.h
 *
 */

#ifndef NN_CMD_PRIVATE_H_
#define NN_CMD_PRIVATE_H_

#define DEFAULT_NN_PORT 1234
#define DEFAULT_NN_HOSTNAME "dms.ccma.itri"

#define DEFAULT_FN_LEN  128

uint64_t nncmd_magic_s;
uint64_t nncmd_magic_e;

uint64_t nncmd_magic_s_netorder;
uint64_t nncmd_magic_e_netorder;

/*
 * packet format for create/clone/delete volume
 * create: vol_para : volume capacity
 *         vol_rsp  : volumd id
 * clone : vol_para : volume id
 *         vol_rsp  : volume id
 * delete: vol_para : volume id
 *         vol_rsp  : delete result
 */
typedef struct reqpkt_nnvolop {
    uint64_t mnum_s;
    uint16_t opcode;
    uint64_t vol_para;
} __attribute__ ((__packed__)) reqpkt_nnvolop_t;

typedef struct rsppkt_nnvolop {
    uint64_t vol_rsp;
} __attribute__ ((__packed__)) rsppkt_nnvolop_t;

// TODO we should add additional opcode at namenode for query volume information
typedef struct reqpkt_attvol {
    uint64_t mnum_s;
    uint16_t opcode;
    uint64_t vol_id;
} __attribute__ ((__packed__)) reqpkt_attvol_t;

typedef reqpkt_attvol_t reqpkt_detvol;

typedef struct rsppkt_attvol {
    uint64_t vol_cap;
    uint32_t num_replica;
    uint16_t is_share;
} __attribute__ ((__packed__)) rsppkt_attvol_t;

#endif /* NN_CMD_PRIVATE_H_ */
