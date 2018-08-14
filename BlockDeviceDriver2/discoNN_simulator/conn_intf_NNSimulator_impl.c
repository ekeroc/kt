/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_intf_DNSimulator_impl.c
 *
 * This component try to simulate NN
 *
 */

#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/common.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../connection_manager/conn_state_fsm.h"
#include "../connection_manager/conn_manager.h"
#include "../connection_manager/conn_sock_api.h"
#include "discoNNSrv_CNRequest.h"
#include "../discoNN_client/discoC_NN_protocol.h"
#include "discoNNSrv_simulator.h"

static cs_wrapper_t *_conn_target (discoC_conn_t *myconn, void *sk_private)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    cs_wrapper_t *sk_wrapper;

    sk_wrapper = alloc_sock_wrapper(myconn->ipaddr, myconn->port);
    ccma_inet_aton(myconn->ipaddr, hname, IPV4ADDR_NM_LEN);
    if (unlikely(IS_ERR_OR_NULL(sk_wrapper))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC INFO DNSmltor fail alloc socket wrapper %s:%d\n",
                hname, myconn->port);
        return NULL;
    }

    sk_wrapper->private_data = sk_private;
    myconn->current_sock = sk_wrapper;

    return sk_wrapper;
}

static void nnSmltorImpl_connect_target (discoC_conn_t *myconn)
{
    cs_wrapper_t *sk_wrapper;
    NNSimulator_t *nnSmltor;
    conn_action_t action;

    nnSmltor = create_NNSmltor(myconn->ipaddr, myconn->port);
    if (IS_ERR_OR_NULL(nnSmltor)) {
        return;
    }

    update_conn_state_and_sock(myconn, CONN_E_Connect, NULL, &action);
    if (CONN_A_NoAction == action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DNSmltor fail create conn transit CONN_E_Connect %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
    }

    //this function will connect to target server socket
    sk_wrapper = _conn_target(myconn, nnSmltor);
    if(IS_ERR_OR_NULL(sk_wrapper)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DNSmltor fail create sock wrapper %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
        return;
    }

    update_conn_state_and_sock(myconn, CONN_E_Connect_OK, sk_wrapper, &action);
    if (action != CONN_A_DO_Service) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DNSmltor fail change conn state: CONN_E_Connect_OK\n");
        return;
    }

    myconn->conn_lost_time = 0; //Lego: reset to 0 indiciate connect now

    if (myconn->conn_done_fn(myconn, false)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DNSmltor conn_don_fn fail after re-connect to %s:%d success\n",
                myconn->ipv4addr_str, myconn->port);
        //TODO: error handling whe socket is ok but fail to create rx
    }
}

static int32_t nnSmltorImpl_tx_send (discoC_conn_t *conn, void *buf, size_t len,
               const bool lock_sock, const bool do_simulation)
{
    cs_wrapper_t *sock_wrapper;
    int32_t ret;

    if (IS_ERR_OR_NULL(conn) || IS_ERR_OR_NULL(buf)) {
        dms_printk(LOG_LVL_WARN, "DMSC WANR DNSmltor null para ptr sock tx\n");
        return -EINVAL;
    }

    if (lock_sock) {
        if (connMgr_lockConn(conn)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN DNSmltor acquire conn tx lock fail\n");
            return 0;
        }
    }

    sock_wrapper = get_sock_wrapper(conn);
    if (sock_wrapper == NULL) {
        ret = -EINVAL;
        goto CONN_TX_DONE;
    }

    ret = discoNNSrv_rx_rcv((NNSimulator_t *)sock_wrapper->private_data, len, buf);

    deRef_conn_sock(sock_wrapper);

CONN_TX_DONE:
    if (lock_sock) {
        mutex_unlock(&conn->sock_mux);
    }

    return ret;
}

static int32_t nnSmltorImpl_rx_rcv (cs_wrapper_t *sock_wrapper, void *buf, size_t len)
{
    return discoNNSrv_tx_send(sock_wrapper->private_data, len, buf);
}

static void _nnSmltor_free_socket (cs_wrapper_t *sock)
{
    int32_t retry_free;

    if (IS_ERR_OR_NULL(sock)) {
        return;
    }

    retry_free = 0;
    while (atomic_read(&sock->sock_ref_cnt) > 0) {
        retry_free++;
        msleep(100);
        if (retry_free % 100 == 0) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO someone ref the socket retry_cnt %d ref_cnt %d\n",
                    retry_free, atomic_read(&sock->sock_ref_cnt));
        }
    }

    free_sock_wrapper(sock);
}

static void nnSmltorImpl_close_conn (discoC_conn_t *myconn, cs_wrapper_t *old_sock)
{
    if (!IS_ERR_OR_NULL(old_sock)) {
        stop_NNSmltor((NNSimulator_t *)old_sock->private_data);
    }

    if (!IS_ERR_OR_NULL(myconn->skrx_thread_ds)) {
        dms_printk(LOG_LVL_INFO, "DMSC conn stop thread with %s:%d\n",
                myconn->ipv4addr_str, myconn->port);
        stop_sk_rx_thread(myconn->skrx_thread_ds);
        myconn->skrx_thread_ds = NULL;
    }

    _nnSmltor_free_socket(old_sock);

    dms_printk(LOG_LVL_INFO, "DMSC INFO NN Simulator not support close conn\n");
}

/*
 * Description: when fail to send data, caller can use reset_discoC_conn to
 *              close current connection and create a new one.
 */
static int32_t nnSmltorImpl_reset_conn (discoC_conn_t *myconn)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO NN Simulator not support reset conn\n");

    return ret;
}

static void nnSmltorImpl_reconnect (discoC_conn_t *myconn)
{
    dms_printk(LOG_LVL_INFO, "DMSC INFO NN Simulator not support reconnect\n");
}

void NNSimulator_set_connIntf (conn_intf_fn_t *myIntf)
{
    myIntf->rx_rcv = nnSmltorImpl_rx_rcv;
    myIntf->tx_send = nnSmltorImpl_tx_send;
    myIntf->resetConn = nnSmltorImpl_reset_conn;
    myIntf->reconnect = nnSmltorImpl_reconnect;
    myIntf->closeConn = nnSmltorImpl_close_conn;
    myIntf->connTarget = nnSmltorImpl_connect_target;
}
