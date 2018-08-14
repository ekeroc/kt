/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_manager.h
 *
 */

#ifndef _SOCK_CLIENT_H_
#define _SOCK_CLIENT_H_

#include <linux/ctype.h>
#include <net/tcp.h>
#include <net/sock.h>

#define HB_PKT_BUFFER_SIZE  1024

typedef enum {
    connMgr_S_BIT_Running,
} connMgr_stat_bit;

typedef struct socket_release_manager {
    struct list_head sock_rel_pools;
    spinlock_t sock_rel_plock;
    atomic_t num_socks;
    wait_queue_head_t sock_rel_waitq;
    dmsc_thread_t *sk_free_worker;
    atomic_t num_running_worker;
} sk_release_mgr_t;

#define CONN_POOL_HASH_SIZE 64
typedef struct discoC_connection_manager {
    struct list_head conn_pool[CONN_POOL_HASH_SIZE];
    struct rw_semaphore conn_plock[CONN_POOL_HASH_SIZE];
    atomic_t cnt_discoC_conn;
    sk_release_mgr_t sk_free_mgr;
    dmsc_thread_t *conn_hb_wker;
    int32_t conn_mgr_state;
    int8_t *conn_hb_pkt_buffer; //buffer for connection heartbeat
} discoC_conn_mgr_t;

extern bool connMgr_running(void);
extern sk_release_mgr_t *connMgr_get_skfreeMgr(void);
extern cs_wrapper_t *update_conn_state_and_sock(discoC_conn_t *myconn,
        conn_TRevent_t trEvent, cs_wrapper_t *new_sk_wrapper,
        conn_action_t *action);
#endif /* _SOCK_CLIENT_H_ */

