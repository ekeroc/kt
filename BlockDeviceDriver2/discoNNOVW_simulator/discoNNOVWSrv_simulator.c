/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_simulator.c
 *
 * This component try to simulate NN
 *
 */
#include <linux/delay.h>
#include <linux/net.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../config/dmsc_config.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoNNOVWSrv_CNRequest.h"
#include "discoNNOVWSrv_simulator_private.h"
#include "discoNNOVWSrv_simulator.h"
#include "../connection_manager/conn_manager_export.h"
#include "conn_intf_NNOVWSimulator_impl.h"
#include "../io_manager/discoC_IOSegment_Request.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "../discoNN_client/discoC_NNC_ovw.h"
#include "discoNNOVWSrv_protocol.h"

static void _nnovwSmltor_sock_txErrfn (void)
{
    dms_printk(LOG_LVL_DEBUG, "_nnovwSmltor_sock_txErrfn called");
}

static void _nnovwSmltor_sock_rxErrfn (struct discoC_connection *conn)
{
    dms_printk(LOG_LVL_DEBUG, "_nnovwSmltor_sock_rxErrfn called");
}

discoC_conn_t *discoNNOVWSrv_setConn_errfn (discoC_conn_t *conn)
{
    conn->tx_error_handler = _nnovwSmltor_sock_txErrfn;
    conn->rx_error_handler = _nnovwSmltor_sock_rxErrfn;

    NNOVWSimulator_set_connIntf(&conn->connOPIntf);
    return conn;
}

static void _add_NNOVWSmltor_tbl (NNOVWSimulator_t *ovwsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(ovwSmltorMgr.NNOVWSmltor_pool);
    plock = &(ovwSmltorMgr.NNOVWSmltor_plock);

    mutex_lock(plock);
    list_add_tail(&ovwsmltor->list_NNOVWtbl, pool);
    atomic_inc(&ovwSmltorMgr.num_NNOVWSmltor);
    ovwsmltor->isInPool = true;
    mutex_unlock(plock);
}

static void _rm_NNOVWSmltor_tbl (NNOVWSimulator_t *ovwsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(ovwSmltorMgr.NNOVWSmltor_pool);
    plock = &(ovwSmltorMgr.NNOVWSmltor_plock);

    mutex_lock(plock);

    if (ovwsmltor->isInPool) {
        list_del(&ovwsmltor->list_NNOVWtbl);
        atomic_dec(&ovwSmltorMgr.num_NNOVWSmltor);
        ovwsmltor->isInPool = false;
    }

    mutex_unlock(plock);
}

NNOVWSimulator_t *create_NNOVWSmltor (uint32_t ipaddr, uint32_t port)
{
    NNOVWSimulator_t *ovwSmltor;

    ovwSmltor = discoC_mem_alloc(sizeof(NNOVWSimulator_t), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(ovwSmltor)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc NN OVW simulator\n");
        return NULL;
    }

    ovwSmltor->ipaddr = ipaddr;
    ovwSmltor->port = port;

    INIT_LIST_HEAD(&ovwSmltor->list_NNOVWtbl);
    INIT_LIST_HEAD(&ovwSmltor->CNOVWReq_pool);
    atomic_set(&ovwSmltor->CNOVWReq_qsize, 0);
    spin_lock_init(&ovwSmltor->CNOVWReq_plock);
    init_waitqueue_head(&ovwSmltor->CNOVWRXThd_waitq);

    ovwSmltor->tx_pendCNOVWReq = NULL;
    ovwSmltor->isInPool = false;
    atomic_set(&ovwSmltor->tx_pendState, TXNNOVWResp_Send_INIT);
    _add_NNOVWSmltor_tbl(ovwSmltor);

    ovwSmltor->isRunning = true;
    return ovwSmltor;
}

static void _free_all_CNOVWReq (NNOVWSimulator_t *ovwSmltor)
{
    CNOVWReq_t *cur, *next, *cnReq;
    int32_t numReq;

    numReq = 0;

PEEK_NEXT_CNREQ:
    cnReq = NULL;
    spin_lock(&ovwSmltor->CNOVWReq_plock);

    list_for_each_entry_safe (cur, next, &ovwSmltor->CNOVWReq_pool, list_CNOVWReq_pool) {
        if (IS_ERR_OR_NULL(cur)) {
            break;
        }

        list_del(&cur->list_CNOVWReq_pool);
        numReq++;
        cnReq = cur;
    }
    spin_unlock(&ovwSmltor->CNOVWReq_plock);

    if (!IS_ERR_OR_NULL(cnReq)) {
        CNOVWReq_free_request(ovwSmltorMgr.NNOVWSmltor_mpoolID, cnReq);
        goto PEEK_NEXT_CNREQ;
    }

    if (numReq) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger CNMDReq %d when free NN OVW Simulator\n", numReq);
    }

}

void free_NNOVWSmltor (NNOVWSimulator_t *ovwSmltor)
{
    _rm_NNOVWSmltor_tbl(ovwSmltor);
    _free_all_CNOVWReq(ovwSmltor);

    if (!IS_ERR_OR_NULL(ovwSmltor->tx_pendCNOVWReq)) {
        CNOVWReq_free_request(ovwSmltorMgr.NNOVWSmltor_mpoolID, ovwSmltor->tx_pendCNOVWReq);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger tx_CNOVWReq when free NN OVW Simulator\n");
    }

    discoC_mem_free(ovwSmltor);
}

void stop_NNOVWSmltor (NNOVWSimulator_t *ovwSmltor)
{
    ovwSmltor->isRunning = false;
    wmb();
    wake_up_interruptible_all(&ovwSmltor->CNOVWRXThd_waitq);
}

static void _submit_CNOVWReq_ovwSmltor (NNOVWSimulator_t *ovwSmltor, CNOVWReq_t *cnReq)
{
    spin_lock(&ovwSmltor->CNOVWReq_plock);

    list_add_tail(&cnReq->list_CNOVWReq_pool, &ovwSmltor->CNOVWReq_pool);
    atomic_inc(&ovwSmltor->CNOVWReq_qsize);

    spin_unlock(&ovwSmltor->CNOVWReq_plock);

    if (waitqueue_active(&ovwSmltor->CNOVWRXThd_waitq)) {
        wake_up_interruptible_all(&ovwSmltor->CNOVWRXThd_waitq);
    }
}

static uint32_t _NNOVWSrv_rcv_header (int8_t *packet, uint16_t *opcode,
        uint16_t *subopcode, uint64_t *volID)
{
    int8_t *pkt_ptr;
    uint32_t len;

    pkt_ptr = packet;
    len = 0;

    //memcpy(&mnum, pkt_ptr, sizeof(uint64_t)); //mnum start
    pkt_ptr += sizeof(uint64_t);
    len += sizeof(uint64_t);

    memcpy((int8_t *)opcode, pkt_ptr, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);
    len += sizeof(uint16_t);

    memcpy((int8_t *)subopcode, pkt_ptr, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);
    len += sizeof(uint16_t);

    memcpy((int8_t *)volID, pkt_ptr, sizeof(uint64_t));
    pkt_ptr += sizeof(uint64_t);
    len += sizeof(uint64_t);

    *opcode = ntohs(*opcode);
    *subopcode = ntohs(*subopcode);
    *volID = ntohll(*volID);

    return len;
}

int32_t discoNNOVWSrv_rx_rcv (NNOVWSimulator_t *ovwSmltor, int32_t pkt_len, int8_t *packet)
{
    CNOVWReq_t *cnReq;
    uint64_t volID;
    uint16_t opcode, subopcode;

    //ccma_inet_aton(nnSmltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    _NNOVWSrv_rcv_header(packet, &opcode, &subopcode, &volID);

    if (opcode == DMS_NN_REQUEST_CMD_TYPE_OVW_UPDATE_HB) {
        return pkt_len;
    }

    cnReq = CNOVWReq_alloc_request(ovwSmltorMgr.NNOVWSmltor_mpoolID);
    if (IS_ERR_OR_NULL(cnReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc CNOVW Request\n");
        return pkt_len;
    }

    cnReq->opcode = opcode;
    cnReq->subopcode = subopcode;
    cnReq->volumeID = volID;

    _submit_CNOVWReq_ovwSmltor(ovwSmltor, cnReq);
    return pkt_len;
}

static int32_t _gen_txAck_packet (NNOVWSimulator_t *ovwSmltor, int32_t pkt_len,
        int8_t *packet)
{
    CNOVWReq_t *myReq;
    TXNNOVWResp_Send_State_t myStat;

    myReq = ovwSmltor->tx_pendCNOVWReq;
    myStat = atomic_read(&ovwSmltor->tx_pendState);

    if (TXNNOVWResp_Send_INIT == myStat) {
        switch (myReq->opcode) {
        case DMS_NN_REQUEST_CMD_TYPE_OVERWRITTEN:
            break;
        case DMS_NN_REQUEST_CMD_TYPE_OVW_UPDATE_HB:
        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN err opcode OVWSmltor %d gen ack\n", myReq->opcode);
            break;
        }

        NNOVWSrv_gen_ackpkt((ovw_res *)packet, myReq->opcode,
                DMS_NN_CMD_OVW_SUB_TYPE_RESPONSE, myReq->volumeID, 0);
        atomic_set(&ovwSmltor->tx_pendState, TXNNOVWResp_Send_Body_DONE);
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN err state OVWSmltor %d gen ack\n", myStat);
    }

    return pkt_len;
}

static CNOVWReq_t *_peek_CNOVWReq_ReqPool (NNOVWSimulator_t *ovwSmltor)
{
    CNOVWReq_t *myReq;

    spin_lock(&ovwSmltor->CNOVWReq_plock);

    myReq = list_first_entry_or_null(&ovwSmltor->CNOVWReq_pool, CNOVWReq_t,
            list_CNOVWReq_pool);

    if (!IS_ERR_OR_NULL(myReq)) {
        list_del(&myReq->list_CNOVWReq_pool);
        atomic_dec(&ovwSmltor->CNOVWReq_qsize);
    }

    spin_unlock(&ovwSmltor->CNOVWReq_plock);

    return myReq;
}

int32_t discoNNOVWSrv_tx_send (NNOVWSimulator_t *ovwSmltor, int32_t pkt_len, int8_t *packet)
{
    CNOVWReq_t *myReq;
    TXNNOVWResp_Send_State_t myStat;

PEEK_AGAIN:
    myReq = NULL;
    myStat = atomic_read(&ovwSmltor->tx_pendState);
    if (TXNNOVWResp_Send_INIT == myStat) {
        wait_event_interruptible(ovwSmltor->CNOVWRXThd_waitq,
            !(ovwSmltor->isRunning) || atomic_read(&ovwSmltor->CNOVWReq_qsize) > 0);

        if (!ovwSmltor->isRunning) {
            return 0;
        }

        myReq = _peek_CNOVWReq_ReqPool(ovwSmltor);
        if (IS_ERR_OR_NULL(myReq)) {
            goto PEEK_AGAIN;
        }

        ovwSmltor->tx_pendCNOVWReq = myReq;

        _gen_txAck_packet(ovwSmltor, pkt_len, packet);
        atomic_set(&ovwSmltor->tx_pendState, TXNNOVWResp_Send_Hder_DONE);

        atomic_set(&ovwSmltor->tx_pendState, TXNNOVWResp_Send_Body_DONE);
    }

    myStat = atomic_read(&ovwSmltor->tx_pendState);
    if (myStat == TXNNOVWResp_Send_Body_DONE) {
        atomic_set(&ovwSmltor->tx_pendState, TXNNOVWResp_Send_INIT);
        CNOVWReq_free_request(ovwSmltorMgr.NNOVWSmltor_mpoolID,
                ovwSmltor->tx_pendCNOVWReq);
        ovwSmltor->tx_pendCNOVWReq = NULL;

    }

    return pkt_len;
}

void init_NNOVWSimulator_manager (void)
{
    int32_t num_req_size;

    INIT_LIST_HEAD(&ovwSmltorMgr.NNOVWSmltor_pool);
    mutex_init(&ovwSmltorMgr.NNOVWSmltor_plock);

    atomic_set(&ovwSmltorMgr.num_NNOVWSmltor, 0);
    num_req_size = 1024;
    ovwSmltorMgr.NNOVWSmltor_mpoolID = register_mem_pool("NNOVWSmltorCNOVWReq",
            num_req_size, sizeof(CNOVWReq_t), true);
}

static void _rel_NNOVWSmltor_gut (void)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct mutex *plock;
    struct list_head *phead;
    NNOVWSimulator_t *cur, *next, *smltor;

    plock = &(ovwSmltorMgr.NNOVWSmltor_plock);
    phead = &(ovwSmltorMgr.NNOVWSmltor_pool);

PEEK_FREE_NEXT:
    smltor = NULL;
    mutex_lock(plock);
    list_for_each_entry_safe (cur, next, phead, list_NNOVWtbl) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN rel NN simulator list broken\n");
            break;
        }

        list_del(&cur->list_NNOVWtbl);
        cur->isInPool = false;
        atomic_dec(&ovwSmltorMgr.num_NNOVWSmltor);

        smltor = cur;
        break;
    }
    mutex_unlock(plock);

    if (!IS_ERR_OR_NULL(smltor)) {
        ccma_inet_aton(smltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);

        free_NNOVWSmltor(smltor);

        goto PEEK_FREE_NEXT;
    }
}

void rel_NNOVWSimulator_manager (void)
{
    int32_t num_worker, ret;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel NN OVW Simulator manager\n");
    num_worker = atomic_read(&ovwSmltorMgr.num_NNOVWSmltor);

    _rel_NNOVWSmltor_gut();

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel NN OVW Simulator manager DONE\n");

    ret = deregister_mem_pool(ovwSmltorMgr.NNOVWSmltor_mpoolID);

    if (ret < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail free CNMDReq mem pool %d\n",
                ovwSmltorMgr.NNOVWSmltor_mpoolID);
    }
}
