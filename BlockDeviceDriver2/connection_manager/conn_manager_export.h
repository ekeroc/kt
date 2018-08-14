/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_manager_export.h
 *
 * for external components include
 */

#ifndef CONN_MANAGER_EXPORT_H_
#define CONN_MANAGER_EXPORT_H_

typedef enum {
    SOCK_DISCONN,
    SOCK_CONN,
    SOCK_INIT,
} conn_state_t;

typedef enum {
    CONN_IMPL_SOCK,
    CONN_IMPL_NN_SIMULATOR,
    CONN_IMPL_NNOVW_SIMULAOR,
    CONN_IMPL_DN_SIMULATOR,
} conn_IMPL_t;

typedef enum {
    CONN_S_INIT,
    CONN_S_Connecting,
    CONN_S_Connected,
    CONN_S_ConnectRetry,
    CONN_S_Closing,
    CONN_S_End,
    CONN_S_Unknown,
} conn_stat_t;

typedef struct conn_state_fsm {
    conn_stat_t conn_state;
} connStat_fsm_t;

typedef struct connection_sock_wrapper {
    struct socket *sock;
    atomic_t sock_ref_cnt;
    struct list_head list_free;
    void *private_data;
} cs_wrapper_t;

struct discoC_connection;

typedef int32_t tx_fn_t (struct discoC_connection *goal, void *buf, size_t len, bool lock_sock, bool do_simulation);
typedef int32_t rx_fn_t (cs_wrapper_t *sock_wrapper, void *buf, size_t len);
typedef void conn_target_fn_t(struct discoC_connection *myconn);
typedef int32_t resetConn_fn_t (struct discoC_connection *myconn);
typedef void reconn_fn_t (struct discoC_connection *myconn);
typedef void closeConn_fn_t (struct discoC_connection *conn, cs_wrapper_t *sock_wrapper);

/*
 * Define connection interface function
 */
typedef struct disco_conn_interface_op {
    tx_fn_t           *tx_send;
    rx_fn_t           *rx_rcv;
    resetConn_fn_t    *resetConn;
    reconn_fn_t       *reconnect;
    closeConn_fn_t    *closeConn;
    reconn_fn_t       *connTarget;
} conn_intf_fn_t;

typedef int32_t (conn_success_post_func) (struct discoC_connection *conn, bool do_req_retry);
typedef int32_t (gen_conn_hb_packet_func) (int8_t *pkt_buffer, int32_t max_buff_size);
typedef struct discoC_connection {
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint32_t ipaddr;
    uint32_t port;
    cs_wrapper_t *current_sock;
    struct list_head list_connPool;

    connStat_fsm_t conn_state;
    rwlock_t conn_stlock;
    struct dmsc_thread *skrx_thread_ds;
    struct mutex sock_mux;
    uint64_t conn_lost_time;

    void (*tx_error_handler)(void);
    void (*rx_error_handler)(struct discoC_connection *conn);
    conn_success_post_func *conn_done_fn;

    gen_conn_hb_packet_func *gen_hb_pkt_fn;

    conn_intf_fn_t connOPIntf;
} discoC_conn_t;

extern void deRef_conn_sock(cs_wrapper_t *sock_wrapper);
extern int16_t connMgr_get_connState(discoC_conn_t *conn_entry);

extern void connMgr_unlockConn(discoC_conn_t *goal);
extern int32_t connMgr_lockConn(discoC_conn_t *goal);

extern cs_wrapper_t *get_sock_wrapper(discoC_conn_t *conn_entry);

extern discoC_conn_t *connMgr_create_conn(uint32_t ipaddr, int32_t port,
        conn_success_post_func *conn_done_fn,
        gen_conn_hb_packet_func *gen_hb_pkt_fn,
        conn_IMPL_t impl);

extern discoC_conn_t *connMgr_get_conn(uint32_t ipaddr, int32_t port);
extern int32_t connMgr_dump_connStatus(int8_t *buffer, int32_t buf_size);

extern void init_conn_manager(void);
extern void rel_conn_manager(void);

#endif /* CONN_MANAGER_EXPORT_H_ */
