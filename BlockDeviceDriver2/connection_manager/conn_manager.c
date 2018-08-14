/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * sock_client.c
 *
 * Term : connection / conn : discoC_connection
 *        sock wrapper : connection_sock_wrapper
 *        sock         : linux socket
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/ctype.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/common.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm.h"
#include "conn_manager.h"
#include "conn_manager_private.h"
#include "socket_reconn_worker.h"
#include "conn_sock_api.h"
#include "conn_hb_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "../housekeeping.h"
#include "../config/dmsc_config.h"
#include "socket_free_worker.h"
#include "conn_mgr_impl.h"
#include "../discoDN_simulator/discoDNSrv_simulator_export.h"
#include "../discoNN_simulator/discoNNSrv_simulator_export.h"
#include "../discoNNOVW_simulator/discoNNOVWSrv_simulator_export.h"

bool connMgr_running (void)
{
    return test_bit(connMgr_S_BIT_Running,
                (const volatile void *)(&(discoC_connMgr.conn_mgr_state)));
}

sk_release_mgr_t *connMgr_get_skfreeMgr (void)
{
    return &discoC_connMgr.sk_free_mgr;
}

static void _add_conn_pool_lockless (struct list_head *lhead, discoC_conn_t *conn)
{
    list_add_tail(&(conn->list_connPool), lhead);
    atomic_inc(&discoC_connMgr.cnt_discoC_conn);
}

static void _rm_conn_pool_lockless (discoC_conn_t *conn)
{
    list_del(&(conn->list_connPool));
    atomic_dec(&discoC_connMgr.cnt_discoC_conn);
}

static discoC_conn_t *_find_conn_lockless (struct list_head *lhead,
        uint32_t ipaddr, int32_t port)
{
    discoC_conn_t *cur, *next, *conn;

    conn = NULL;
    list_for_each_entry_safe (cur, next, lhead, list_connPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN conn list broken when search conn %d %d\n",
                    ipaddr, port);
            break;
        }
        if (cur->ipaddr == ipaddr && cur->port == port) {
            conn = cur;
            break;
        }
    }

    return conn;
}

/*
 * Description: acquire lock of socket for tx.
 *              Prevent multiple threads send their packet in wrong order
 * return value: non zero: acquire fail (-EINTR)
 * NOTE: caller guarantee goal is not NULL
 */
int32_t connMgr_lockConn (discoC_conn_t *goal)
{
    return mutex_lock_interruptible(&goal->sock_mux);
}

void connMgr_unlockConn (discoC_conn_t *goal)
{
    if (unlikely(IS_ERR_OR_NULL(goal))) {
        return;
    }

    mutex_unlock(&goal->sock_mux);
}

int16_t connMgr_get_connState (discoC_conn_t *conn)
{
    conn_stat_t cstat;
    int16_t con_state;

    con_state = SOCK_DISCONN;
    if (unlikely(IS_ERR_OR_NULL(conn))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para when read conn state\n");
        DMS_WARN_ON(true);
        return con_state;
    }

    read_lock(&conn->conn_stlock);

    cstat = get_conn_state(&conn->conn_state);
    if (CONN_S_Connected == cstat) {
        con_state = SOCK_CONN;
    }
#if 0 //TODO: When we allow multiple threads create connection to the same target, enable this
    else if (CONN_S_INIT == cstat ||
            CONN_S_Connecting == cstat) {
        con_state = SOCK_INIT;
    }
#endif
    read_unlock(&conn->conn_stlock);

    return con_state;
}

/*
 * Description: We need this function in following case:
 *     1. When connection broken, we need replace socket wrapper in conn as NULL.
 *        Meanwhile, we need change the state
 */
cs_wrapper_t *update_conn_state_and_sock (discoC_conn_t *myconn,
        conn_TRevent_t trEvent, cs_wrapper_t *new_sk_wrapper,
        conn_action_t *action)
{
    cs_wrapper_t *old_sk_wrapper;

    *action = CONN_A_NoAction;
    if (unlikely(IS_ERR_OR_NULL(myconn))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para when get conn and inc ref cnt\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    old_sk_wrapper = NULL;
    write_lock(&myconn->conn_stlock);

    *action = update_conn_state(&myconn->conn_state, trEvent);

    if (CONN_A_NoAction != *action) {
        old_sk_wrapper = myconn->current_sock;
        myconn->current_sock = new_sk_wrapper;
    }

    write_unlock(&myconn->conn_stlock);

    return old_sk_wrapper;
}

cs_wrapper_t *get_sock_wrapper (discoC_conn_t *conn_entry)
{
    cs_wrapper_t *sock_wrapper;

    if (unlikely(IS_ERR_OR_NULL(conn_entry))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para when get conn and inc ref cnt\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    sock_wrapper = NULL;
    read_lock(&conn_entry->conn_stlock);

    if (CONN_S_Connected == get_conn_state(&conn_entry->conn_state)) {
        if (!IS_ERR_OR_NULL(conn_entry->current_sock)) {
            sock_wrapper = conn_entry->current_sock;
            atomic_inc(&sock_wrapper->sock_ref_cnt);
        }
    }

    read_unlock(&conn_entry->conn_stlock);

    return sock_wrapper;
}

void deRef_conn_sock (cs_wrapper_t *sock_wrapper)
{
    if (unlikely(IS_ERR_OR_NULL(sock_wrapper))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para when dec conn ref cnt\n");
        DMS_WARN_ON(true);
        return;
    }

    atomic_dec(&sock_wrapper->sock_ref_cnt);
}

discoC_conn_t *connMgr_get_conn (uint32_t ipaddr, int32_t port)
{
    discoC_conn_t *conn;
    int32_t bk_idx;

    bk_idx = ipaddr % CONN_POOL_HASH_SIZE;

    down_read(&discoC_connMgr.conn_plock[bk_idx]);

    conn = _find_conn_lockless(&discoC_connMgr.conn_pool[bk_idx], ipaddr, port);

    up_read(&discoC_connMgr.conn_plock[bk_idx]);

    return conn;
}

static discoC_conn_t *_create_conn (uint32_t ipaddr, int32_t port,
        conn_success_post_func *conn_done_fn,
        gen_conn_hb_packet_func *gen_hb_pkt_fn,
        conn_IMPL_t impl)
{
    discoC_conn_t *conn;

    conn = discoC_mem_alloc(sizeof(discoC_conn_t), GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(conn)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail alloc mem when create conn\n");
        return NULL;
    }

    memset(conn, 0, sizeof(discoC_conn_t));
    ccma_inet_aton(ipaddr, conn->ipv4addr_str, IPV4ADDR_NM_LEN);
    conn->ipaddr = ipaddr;
    conn->port = port;
    conn->current_sock = NULL;
    INIT_LIST_HEAD(&conn->list_connPool);
    rwlock_init(&conn->conn_stlock);
    init_conn_state_fsm(&conn->conn_state);

    conn->skrx_thread_ds = NULL;
    mutex_init(&conn->sock_mux);
    conn->conn_lost_time = 0;

    conn->conn_done_fn = conn_done_fn;
    conn->gen_hb_pkt_fn = gen_hb_pkt_fn;

    switch (impl) {
    case CONN_IMPL_SOCK:
        sockImpl_setConn_errfn(conn);
        break;
    case CONN_IMPL_NN_SIMULATOR:
        discoNNSrv_setConn_errfn(conn);
        break;
    case CONN_IMPL_NNOVW_SIMULAOR:
        discoNNOVWSrv_setConn_errfn(conn);
        break;
    case CONN_IMPL_DN_SIMULATOR:
        discoDNSrv_setConn_errfn(conn);
        break;
    default:
        sockImpl_setConn_errfn(conn);
        break;
    }

    return conn;
}

discoC_conn_t *connMgr_create_conn (uint32_t ipaddr, int32_t port,
        conn_success_post_func *conn_done_fn,
        gen_conn_hb_packet_func *gen_hb_pkt_fn,
        conn_IMPL_t impl)
{
    discoC_conn_t *myconn;
    int32_t bk_idx;
    bool found;

    if (IS_ERR_OR_NULL(conn_done_fn)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN create conn fail null conn_dn_fn\n");
        return NULL;
    }

    bk_idx = ipaddr % CONN_POOL_HASH_SIZE;
    found = false;
    down_write(&discoC_connMgr.conn_plock[bk_idx]);
    myconn = _find_conn_lockless(&discoC_connMgr.conn_pool[bk_idx], ipaddr, port);
    if (NULL == myconn) {
        myconn = _create_conn(ipaddr, port, conn_done_fn, gen_hb_pkt_fn, impl);
        _add_conn_pool_lockless(&(discoC_connMgr.conn_pool[bk_idx]), myconn);
    } else {
        found = true;
    }
    up_write(&discoC_connMgr.conn_plock[bk_idx]);

    if (IS_ERR_OR_NULL(myconn)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create conn\n");
        return NULL;
    }

    if (found) {
        //TODO: someone is creating connection,
        //      caller should check state until connecting or retry
        //      How we pass this information back?? conn state?
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO create conn fail due to someone creating\n");
        return myconn;
    }

    myconn->connOPIntf.connTarget(myconn);

    return myconn;
}

/*
 * NOTE: this function only used in shutdown driver, otherwise we might encounter
 *       crash issue since we don't know whether any thread is reference conneciton
 *
 *       Step 1: _retry_discoC_conn -> update state connRetry
 *       Step 2: close connection update state close
 *       Step 3: close connection free connection
 *       Step 4: _retry_discoC_conn -> set connection lost and stop rx thread -> crash
 * TODO We should add a reference count after refactor NN, NN ovw, DN client
 */
int32_t close_discoC_conn (int32_t ipv4addr, int32_t port)
{
    discoC_conn_t *conn;
    cs_wrapper_t *old_sk_wrapper;
    conn_action_t action;
    int32_t bk_idx;

    bk_idx = ipv4addr % CONN_POOL_HASH_SIZE;

    down_write(&discoC_connMgr.conn_plock[bk_idx]);

    conn = _find_conn_lockless(&discoC_connMgr.conn_pool[bk_idx], ipv4addr, port);
    if (IS_ERR_OR_NULL(conn)) {
        up_write(&discoC_connMgr.conn_plock[bk_idx]);
        return 0;
    }

    old_sk_wrapper = update_conn_state_and_sock(conn, CONN_E_STOP, NULL, &action);
    if (action != CONN_A_DO_Close) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail close conn %s:%d\n",
                conn->ipv4addr_str, conn->port);
        up_write(&discoC_connMgr.conn_plock[bk_idx]);
        return -EPERM;
    }

    _rm_conn_pool_lockless(conn);
    up_write(&discoC_connMgr.conn_plock[bk_idx]);

    set_res_available(conn->ipaddr, conn->port, false);

    conn->connOPIntf.closeConn(conn, old_sk_wrapper);

    update_conn_state_and_sock(conn, CONN_E_STOP_DONE, NULL, &action);
    discoC_mem_free(conn);

    return 0;
}

static void _close_bucket_conn (int32_t bk_idx)
{
    discoC_conn_t *cur, *next;
    int32_t ipv4addr, port;
    bool do_close;

PEEK_NEXT_CONN:
    ipv4addr = 0;
    port = 0;
    do_close = false;

    down_write(&discoC_connMgr.conn_plock[bk_idx]);

    list_for_each_entry_safe (cur, next, &discoC_connMgr.conn_pool[bk_idx], list_connPool) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN connection pool list broken\n");
            break;
        }

        ipv4addr = cur->ipaddr;
        port = cur->port;
        do_close = true;
        break;
    }

    up_write(&discoC_connMgr.conn_plock[bk_idx]);

    if (do_close) {
        if (close_discoC_conn(ipv4addr, port)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN close conn fail when close all conn\n");
        } else {
            goto PEEK_NEXT_CONN;
        }
    }
}

static void _close_all_conn (void)
{
    int32_t i;

    for (i = 0; i < CONN_POOL_HASH_SIZE; i++) {
        _close_bucket_conn(i);
    }
}

/*
 * close and release connection pool,
 * release connection retry list first before release the connection pool
 */
void rel_conn_manager (void)
{
    clear_bit(connMgr_S_BIT_Running, (volatile void *)(&(discoC_connMgr.conn_mgr_state)));

    stop_hb_worker(&discoC_connMgr);

    //release the retry work queue and retry list first.
    dms_printk(LOG_LVL_INFO, "DMSC INFO try to clean all cr data\n");
    rel_conn_retry_manager();

    dms_printk(LOG_LVL_INFO, "DMSC INFO try to clean all conn data\n");
    _close_all_conn();

    rel_sock_release_mgr(&discoC_connMgr.sk_free_mgr);
    dms_printk(LOG_LVL_INFO, "DMSC INFO release conn pool done\n");

    discoC_mem_free(discoC_connMgr.conn_hb_pkt_buffer);
    discoC_connMgr.conn_hb_pkt_buffer = NULL;
}

/*
 * Connection Pool Initialization
 */
void init_conn_manager (void)
{
    int32_t i, ret;
    discoC_connMgr.conn_mgr_state = 0;
    discoC_connMgr.conn_hb_pkt_buffer =
            discoC_mem_alloc(sizeof(int8_t)*HB_PKT_BUFFER_SIZE, GFP_KERNEL);

    for (i = 0; i < CONN_POOL_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&discoC_connMgr.conn_pool[i]);
        init_rwsem(&discoC_connMgr.conn_plock[i]);
    }
    atomic_set(&discoC_connMgr.cnt_discoC_conn, 0);

    do {
        if ((ret = init_sock_release_mgr(&discoC_connMgr.sk_free_mgr))) {
            break;
        }

        init_conn_retry_manager();
        create_hb_worker(&discoC_connMgr);

        set_bit(connMgr_S_BIT_Running,
                (volatile void *)(&(discoC_connMgr.conn_mgr_state)));
    } while (0);
}

int32_t connMgr_dump_connStatus (int8_t *buffer, int32_t buf_size)
{
    discoC_conn_t *cur, *next;
    int32_t len, i;

    len = 0;

    for (i = 0; i < CONN_POOL_HASH_SIZE; i++) {
        down_read(&discoC_connMgr.conn_plock[i]);
        list_for_each_entry_safe (cur, next, &(discoC_connMgr.conn_pool[i]),
                list_connPool) {
            len += sprintf(buffer + len, "%s:%d connected=%d\n",
                    cur->ipv4addr_str, cur->port, connMgr_get_connState(cur));
        }
        up_read(&discoC_connMgr.conn_plock[i]);
    }

    return len;
}
