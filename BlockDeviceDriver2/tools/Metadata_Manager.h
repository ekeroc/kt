/*
 * Copyright (C) 2010-2014 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Metadata_Manager.h
 *
 */

#ifndef METADATA_MANAGER_H_
#define METADATA_MANAGER_H_

#define MAX_NUM_REPLICA 3
#define MAX_NUM_NN_REQ  1024

typedef enum {
    MDLOC_IS_VALID = 0,
    MDLOC_IS_INVALID,
    MDLOC_IS_VALID_BUT_LESS_REPLICA
} mdloc_stat_t;

typedef enum {
    REQ_INIT,
    REQ_WAIT_FOR_HANDLE,
    REQ_WAIT_FOR_SEND,
    REQ_WAIT_RSP,
    REQ_HANDLE_RSP,
    REQ_DONE,
} mdreq_stat;

typedef enum {
    MDREQ_ENQ_DONE,
    MDREQ_SEND_DONE,
    MDREQ_GET_RSP,
    MDREQ_FINISH,
} mdreq_stat_event;

typedef enum {
    REQ_MD_Q_FOR_READ = 0,
    REQ_MD_Q_TRUE_FOR_READ,
    REQ_MD_ALLOC_AND_GETOLD,
    REQ_MD_ALLOC,
    REQ_MD_Q_FOR_WRITE,
    REQ_MD_REPORT_READ_STATUS,
    REQ_MD_REPORT_WRITE_STATUS,
    REQ_MD_REPORT_WRITE_STATUS_AND_REPLY,
    REQ_MD_UPDATE_HB
} mdreq_op_t;

/*
typedef struct lun_loc {
    uint64_t rbid;
    uint32_t len;
    uint32_t offset;
} lun_loc_t;
*/

typedef struct dn_loc {
    uint32_t ipaddr;
    int32_t port;
    uint16_t num_phy_locs;
    //lun_loc_t *phy_locs;
    uint64_t *phy_locs;
} dn_loc_t;

typedef struct md_item
{
    uint64_t LB_s;
    uint32_t LB_len;
    uint64_t *HBIDs;
    uint16_t num_replica;
    dn_loc_t dnLocs[MAX_NUM_REPLICA];
    uint16_t state;
} md_item_t;

typedef struct disco_NN_metadata {
    uint64_t req_id;
    uint64_t rsp_id;
    struct md_request *md_req;
    uint16_t num_md_items;
    md_item_t *md_items;
} disco_NN_metadata_t;

typedef struct md_request {
    uint64_t req_id;
    uint64_t vol_id;
    uint64_t startLB;
    uint32_t LBlens;

    struct list_head list_pq;
    struct list_head list_wq;

    mdreq_op_t op;
    mdreq_stat stat;

    void *user_data;
    disco_NN_metadata_t *metadata;

    int32_t (*md_rsp_fn)(void *data, disco_NN_metadata_t *md_data);
} mdreq_t;

void set_mdreq_op(mdreq_t *md_req, mdreq_op_t op);
void set_mdreq_stat(mdreq_t *md_req, mdreq_stat stat);

bool gen_and_do_mdreq(uint64_t vol_id, mdreq_op_t op,
        uint64_t startLB, uint32_t LBlen,
        void *user_data, int32_t (*md_rsp_fn)(void *data, disco_NN_metadata_t *md_data));
bool add_mdreq(mdreq_t *md_req);

void destroy_md_manager();
void init_md_manager();

#endif /* METADATA_MANAGER_H_ */
