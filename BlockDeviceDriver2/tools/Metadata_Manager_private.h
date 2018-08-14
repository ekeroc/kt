/*
 * Copyright (C) 2010-2014 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Metadata_Manager_private.h
 *
 */

#ifndef METADATA_MANAGER_PRIVATE_H_
#define METADATA_MANAGER_PRIVATE_H_

#define MAX_MDREQ_HANDLE    128
#define MAGIC_NUM_MDREQ     0xa5a5a5a5a5a5a5a5ll

uint32_t g_mdreq_id;
bool running;

typedef struct mdreq_pkt {
    uint64_t magic_number1;
    uint16_t rw;
    uint64_t startLB;
    uint64_t nnreq_id;
    uint64_t vol_id;
    uint32_t LB_cnt;
    uint64_t magic_number2;
} __attribute__ ((__packed__)) mdreq_pkt_t;

typedef struct mdrsp_pkt {
    uint64_t magic_number1;
    uint16_t rsp_type;
    uint16_t total_bytes;
} __attribute__ ((__packed__)) mdrsp_pkt_t;

typedef struct md_ci_header {
    uint64_t mnum_s;
    uint16_t op;
    uint64_t vol_id;
    uint64_t req_id;
    uint32_t numItems;
    uint64_t mnum_e;
} __attribute__ ((__packed__)) md_cihd_t;


typedef enum {
    RESPONSE_METADATA_REQUEST,
    RESPONSE_METADATA_COMMIT
} mdrsp_type_t;


struct list_head mdreq_workq;
struct list_head mdreq_pendq;

uint64_t mnum_net_order;

pthread_mutex_t mdreq_wq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mdreq_pq_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t num_req_wq;
uint32_t num_req_pq;

pthread_t mdreq_wker_thread;
pthread_attr_t req_worker_attr;

pthread_t mdrsp_wker_thread;
pthread_attr_t rsp_worker_attr;

socket_t nn_sock;

#endif /* METADATA_MANAGER_PRIVATE_H_ */
