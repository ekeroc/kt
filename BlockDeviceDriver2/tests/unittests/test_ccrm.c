
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/netlink.h>

#include "../test_lib.h"
#include "../../datanode_fsm.h"
#include "../../dms_kernel_version.h"
#include "../../drv_init.h"
#include "../../ccrm.h"
#include "../../common_util.h"
#include "../../dms_client_mm.h"
#include "../../sock_client.h"
#include "../../common.h"

discoC_conn_t *mock_dn_conn;

NN_req_t *_build_test_nnreq(void)
{
    NN_req_t *nn_req;
    dn_req_t *dn_req;

    nn_req = (NN_req_t *)ccma_malloc(sizeof(NN_req_t), GFP_NOWAIT | GFP_ATOMIC);
    INIT_LIST_HEAD(&nn_req->dnreqs);

    dn_req = (dn_req_t *)ccma_malloc(sizeof(dn_req_t), GFP_NOWAIT | GFP_ATOMIC);
    INIT_LIST_HEAD(&dn_req->dn_group_list);

    dms_list_add_tail(&dn_req->dn_group_list, &nn_req->dnreqs);
    return nn_req;
}

discoC_conn_t *_build_dn_conn(void)
{
    discoC_conn_t *dn_conn;
    dn_conn = (discoC_conn_t *)ccma_malloc(
        sizeof(discoC_conn_t), GFP_NOWAIT | GFP_ATOMIC);
    dn_conn->conn_owner = CONN_OWN_DN;
    return dn_conn;
}

void _free_nn_req(NN_req_t *nn_req)
{
    dn_req_t *dn_req;

    dn_req = list_entry(nn_req->dnreqs.next, dn_req_t, dn_group_list);
    ccma_free(dn_req);
    ccma_free(nn_req);
}

void _update_nn_req(
    NN_req_t *nn_req, uint32_t ip, uint16_t port, uint64_t vol_id, uint64_t ts)
{
    dn_req_t *dn_req;
    dn_req = list_entry(nn_req->dnreqs.next, typeof(*dn_req), dn_group_list);
    nn_req->vol_id = vol_id;
    nn_req->ts_dn_logging = ts;
    dn_req->ipaddr = ip;
    dn_req->port = port;
}

void _update_cleanlog_ts(NN_req_t *nn_req, uint64_t start_ts, uint16_t interval)
{
    int16_t i, j;
    uint32_t ip;
    uint64_t vol_id, ts = start_ts;

    ip = ccma_inet_ntoa("10.10.0.1");
    vol_id = 0;

    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            _update_nn_req(nn_req, ip, 2121, vol_id, ts);
            update_cleanLog_ts(nn_req);

            if (interval == 0) {
                return;
            } else {
                ts = ts + interval;
            }
        }

        ip++;
        vol_id++;
    }
}

void _record_cleanlog(
    uint64_t start_ts, uint16_t interval, uint16_t num_vol, uint16_t num_ts)
{
    int16_t i, j;
    uint32_t ip;
    uint64_t vol_id, ts = start_ts;

    ip = ccma_inet_ntoa("10.10.0.1");
    vol_id = 0;

    for (i = 0; i < num_vol; i++) {
        for (j = 0; j < num_ts; j++) {
            record_cleanlog(ip, (uint16_t)2121, vol_id, ts);

            if (j % interval == 0) {
              ts++;
            }
        }

        ip++;
        vol_id++;
    }
}

void test_cleanlog_base_op(void)
{
    init_ccrm();
    _record_cleanlog(jiffies_64, 3, 10, 10);
    dump_pending_cleanlog();
    // TODO: add assertion
    destroy_pending_cleanlog();
}

void test_update_cleanlog_ts(void)
{
    NN_req_t *nn_req;
    uint64_t ts;

    init_ccrm();
    ts = jiffies_64;
    _record_cleanlog(ts, 3, 10, 10);
    dump_pending_cleanlog();
    nn_req = _build_test_nnreq();
    _update_cleanlog_ts(nn_req, ts, 1);
    _free_nn_req(nn_req);

    // Examine if `ready_to_clean` is 1 (true)
    dump_pending_cleanlog();

    // TODO: add assertion
    destroy_pending_cleanlog();
}

// MOCK: (ccrm.c) sock_xmit(goal, buf, len, lock_sock) => mock_sock_xmit(goal, buf, len, lock_sock)
int32_t mock_sock_xmit(discoC_conn_t *goal, void *buf, size_t len, bool lock_sock)
{
    return CL_PACKET_SIZE_DN;
}

// MOCK: (ccrm.c) get_connection(ipaddr, port) => mock_get_connection(ipaddr, port)
discoC_conn_t *mock_get_connection (uint32_t ipaddr, int32_t port)
{
    mock_dn_conn = (discoC_conn_t *)ccma_malloc(
        sizeof(discoC_conn_t), GFP_NOWAIT | GFP_ATOMIC);
    mock_dn_conn->conn_owner = CONN_OWN_DN;
    return mock_dn_conn;
}

void test_send_cleanlog(void)
{
    NN_req_t *nn_req;
    uint64_t ts;

    ts = jiffies_64;
    init_ccrm();
    init_cleanLog_data();
    nn_req = _build_test_nnreq();
    mock_dn_conn = _build_dn_conn();

    _record_cleanlog(ts, 1, 1, 5);
    dump_pending_cleanlog();
    _update_cleanlog_ts(nn_req, ts + 3, 0);
    dump_pending_cleanlog();
    send_clog_to_dn();
    dump_pending_cleanlog();
    dump_sent_cleanlog();

    _update_cleanlog_ts(nn_req, ts + 4, 0);
    dump_pending_cleanlog();
    send_clog_to_dn();
    dump_pending_cleanlog();
    dump_sent_cleanlog();

    _update_cleanlog_ts(nn_req, ts, 0);
    dump_pending_cleanlog();
    send_clog_to_dn();
    dump_pending_cleanlog();
    dump_sent_cleanlog();

    destroy_pending_cleanlog();
    destroy_sent_cleanlog();
    // TODO: add assertion

    _free_nn_req(nn_req);
    ccma_free(mock_dn_conn);
}

void test_update_nonexist_cleanlog(void)
{
    NN_req_t *nn_req;
    uint64_t ts;

    ts = jiffies_64;
    init_ccrm();
    init_cleanLog_data();
    nn_req = _build_test_nnreq();
    mock_dn_conn = _build_dn_conn();

    _record_cleanlog(ts, 1, 1, 5);
    dump_pending_cleanlog();
    _update_cleanlog_ts(nn_req, ts, 1);
    destroy_pending_cleanlog();
    destroy_sent_cleanlog();
    // TODO: add assertion

    _free_nn_req(nn_req);
    ccma_free(mock_dn_conn);
}

void test_cleanlog_response(void)
{
    NN_req_t *nn_req;
    uint64_t ts;

    ts = jiffies_64;
    init_ccrm();
    init_cleanLog_data();
    nn_req = _build_test_nnreq();
    mock_dn_conn = _build_dn_conn();

    _record_cleanlog(ts, 1, 1, 5);
    _update_cleanlog_ts(nn_req, ts + 3, 0);
    send_clog_to_dn();
    _update_cleanlog_ts(nn_req, ts + 4, 0);
    send_clog_to_dn();
    dump_sent_cleanlog();
    cleanlog_response(clog_uid);
    dump_sent_cleanlog();
    cleanlog_response(clog_uid - 1);
    dump_sent_cleanlog();
}

void test_flush_cleanlog(void)
{
    uint64_t ts;
    NN_req_t *nn_req;

    init_ccrm();
    init_cleanLog_data();
    nn_req = _build_test_nnreq();
    ts = jiffies_64;
    _record_cleanlog(ts, 2, 5, 5);

    _update_nn_req(nn_req, ccma_inet_ntoa("10.10.0.1"), 2121, 0, ts);
    update_cleanLog_ts(nn_req);
    send_clog_to_dn();
    dump_pending_cleanlog();
    dump_sent_cleanlog();

    flush_cleanlog(0);
    dump_pending_cleanlog();
    dump_sent_cleanlog();

    flush_cleanlog(1);
    dump_pending_cleanlog();

    flush_cleanlog(2);
    dump_pending_cleanlog();

    flush_cleanlog(3);
    dump_pending_cleanlog();

    /* Non-exist vol */
    flush_cleanlog(10);
    dump_pending_cleanlog();

    destroy_pending_cleanlog();
    destroy_sent_cleanlog();
    _free_nn_req(nn_req);
    ccma_free(mock_dn_conn);
}
