/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_mgr_impl.c
 *
 * conn_mgr_impl.h is an implementation of conn_intf_fn_t.
 * Provide a socket based connection manager
 *
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
#include "../common/dms_client_mm.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_mgr_impl.h"
#include "conn_state_fsm.h"
#include "socket_reconn_worker.h"
#include "conn_sock_api.h"
#include "conn_manager.h"
#include "../flowcontrol/FlowControl.h"
#include "../housekeeping.h"
#include "socket_free_worker.h"

static int32_t sockImpl_connDONE_fn (discoC_conn_t *conn, cs_wrapper_t *discoC_sock,
        bool do_req_retry)
{
    int32_t ret;
    conn_action_t action;

    if (IS_ERR_OR_NULL(conn) || IS_ERR_OR_NULL(discoC_sock)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN wrong para for conn success\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    ret = 0;
    update_conn_state_and_sock(conn, CONN_E_Connect_OK, discoC_sock, &action);
    if (action != CONN_A_DO_Service) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail change conn state: CONN_E_Connect_OK\n");
        return -EPERM;
    }

    conn->conn_lost_time = 0; //Lego: reset to 0 indiciate connect now
    discoC_sock->sock->sk->sk_user_data = conn;

    if (IS_ERR_OR_NULL(conn->conn_done_fn) == false) {
        if (conn->conn_done_fn(conn, do_req_retry)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN conn_don_fn fail after re-connect to %s:%d success\n",
                    conn->ipv4addr_str, conn->port);
            //TODO: error handling whe socket is ok but fail to create rx
        }
    }

    return ret;
}

static int32_t _reconn_success_post_func (uint32_t ipv4addr, uint32_t port,
        cs_wrapper_t *discoC_sock)
{
    discoC_conn_t *conn;
    int32_t ret;

    ret = 0;

    if (!connMgr_running()) {
        return -EPERM;
    }

    conn = connMgr_get_conn(ipv4addr, port);

    if (!IS_ERR_OR_NULL(conn)) {
        ret = sockImpl_connDONE_fn(conn, discoC_sock, true);
    } else {
        ret = -EPERM;
    }

    return ret;
}

static void sockImpl_reconnect (discoC_conn_t *myconn)
{
    if (myconn->conn_lost_time == 0) {
        myconn->conn_lost_time = jiffies_64;
    }

    if (add_conn_retry_task(myconn->ipaddr, myconn->port,
            _reconn_success_post_func, myconn)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail trigger reconn task %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
    } else {
        if (!IS_ERR_OR_NULL(myconn->skrx_thread_ds)) {
            dms_printk(LOG_LVL_INFO, "DMSC conn stop thread with %s:%d\n",
                    myconn->ipv4addr_str, myconn->port);
            stop_sk_rx_thread(myconn->skrx_thread_ds);
            myconn->skrx_thread_ds = NULL;
        }
    }
}

/*
 * caller guarantee goal is not NULL
 */
static int32_t _retry_discoC_conn (discoC_conn_t *conn, conn_TRevent_t event,
        bool is_sk_bh)
{
    cs_wrapper_t *old_sk;
    int32_t ret;
    conn_action_t action;

    ret = -EPERM;
    old_sk = update_conn_state_and_sock(conn, event, NULL, &action);

    if (CONN_A_DO_Retry == action) {
        set_res_available(conn->ipaddr, conn->port, false);
        if (!IS_ERR_OR_NULL(old_sk)) {
            enq_release_sock(old_sk, connMgr_get_skfreeMgr());
        }

        if (is_sk_bh) {
            add_hk_task(TASK_CONN_RETRY, (uint64_t *)conn);
        } else {
            conn->connOPIntf.reconnect(conn);
        }
        ret = 0;
    }

    return ret;
}

/*
 * Description: when fail to send data, caller can use reset_discoC_conn to
 *              close current connection and create a new one.
 */
static int32_t sockImpl_reset_conn (discoC_conn_t *myconn)
{
    int32_t ret;

    ret = 0;

    if (IS_ERR_OR_NULL(myconn)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN reset null ptr dn conn\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    dms_printk(LOG_LVL_WARN, "DMSC INFO conn broken reset (%s:%d)\n",
            myconn->ipv4addr_str, myconn->port);

    if (_retry_discoC_conn(myconn, CONN_E_Connect_Broken, false)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN conn reset fail conn already lost\n");
    }

    return ret;
}

static void _sock_tx_err_func (void)
{
    dms_printk(LOG_LVL_DEBUG, "dn_tx_error_handler called");
}

static void _sock_rx_err_func (struct discoC_connection *conn)
{
    if (unlikely(IS_ERR_OR_NULL(conn))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN nn rx err conn entry is null\n");
        return;
    }

    dms_printk(LOG_LVL_WARN, "DMSC INFO conn broken (%s:%d)\n",
            conn->ipv4addr_str, conn->port);

    if (_retry_discoC_conn(conn, CONN_E_Connect_Broken, true)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN someone handle the connection retry task\n");
    }
}

static void sockImpl_connect_target (discoC_conn_t *myconn)
{
    cs_wrapper_t *sk_wrapper;
    conn_action_t action;

    update_conn_state_and_sock(myconn, CONN_E_Connect, NULL, &action);
    if (CONN_A_NoAction == action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN create conn wrong transition CONN_E_Connect %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
    }

    //this function will connect to target server socket
    sk_wrapper = connect_target(myconn->ipaddr, myconn->port);
    if (!IS_ERR_OR_NULL(sk_wrapper)) {
        if (sockImpl_connDONE_fn(myconn, sk_wrapper, false)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN create conn fail exec conn done fn %s:%d\n",
                    myconn->ipv4addr_str, myconn->port);
        }
    } else {
        dms_printk(LOG_LVL_INFO, "DMSC INFO build conn fail %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
        if (_retry_discoC_conn(myconn, CONN_E_Connect_FAIL, false)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong state transit CONN_E_Connect_FAIL %s:%d\n",
                    myconn->ipv4addr_str, myconn->port);
        }
    }
}

static void sockImpl_close_conn (discoC_conn_t *myconn, cs_wrapper_t *old_sock)
{
    cancel_conn_retry_task(myconn->ipaddr, myconn->port);

    if (!IS_ERR_OR_NULL(old_sock)) {
        enq_release_sock(old_sock, connMgr_get_skfreeMgr());
    }

    if (!IS_ERR_OR_NULL(myconn->skrx_thread_ds)) {
        dms_printk(LOG_LVL_INFO, "DMSC conn stop thread with %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
        stop_sk_rx_thread(myconn->skrx_thread_ds);
        myconn->skrx_thread_ds = NULL;
    }
}

static int32_t sockImpl_tx_send (discoC_conn_t *goal, void *buf, size_t len,
               const bool lock_sock, const bool do_simulation)
{
    cs_wrapper_t *sock_wrapper;
    struct socket *mysock;
    int32_t total_done;

    if (IS_ERR_OR_NULL(goal) || IS_ERR_OR_NULL(buf)) {
        dms_printk(LOG_LVL_WARN, "DMSC WANR null para ptr sock tx\n");
        return -EINVAL;
    }

    if (lock_sock) {
        if (connMgr_lockConn(goal)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN acquire conn tx lock fail\n");
            return 0;
        }
    }

    sock_wrapper = get_sock_wrapper(goal);
    if (sock_wrapper == NULL) {
        total_done = -EINVAL;
        goto CONN_TX_DONE;
    }

    mysock = sock_wrapper->sock;
    if (unlikely(do_simulation)) {
        total_done = len;
    } else {
        total_done = sock_tx_send(mysock, buf, len);
    }

    deRef_conn_sock(sock_wrapper);

CONN_TX_DONE:
    if (lock_sock) {
        mutex_unlock(&goal->sock_mux);
    }

    return total_done;
}

/*
 * TODO: change the 1st parameter to connection
 */
static int32_t sockImpl_rx_rcv (cs_wrapper_t *sock_wrapper, void *buf, size_t len)
{
    return sock_rx_rcv(sock_wrapper->sock, buf, len);
}

static void sockImpl_set_connIntf (conn_intf_fn_t *myIntf)
{
    myIntf->rx_rcv = sockImpl_rx_rcv;
    myIntf->tx_send = sockImpl_tx_send;
    myIntf->resetConn = sockImpl_reset_conn;
    myIntf->reconnect = sockImpl_reconnect;
    myIntf->closeConn = sockImpl_close_conn;
    myIntf->connTarget = sockImpl_connect_target;
}

void sockImpl_setConn_errfn (discoC_conn_t *conn)
{
    conn->tx_error_handler = _sock_tx_err_func;
    conn->rx_error_handler = _sock_rx_err_func;

    sockImpl_set_connIntf(&conn->connOPIntf);
}


