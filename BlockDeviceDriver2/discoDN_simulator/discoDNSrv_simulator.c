/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_protocol.c
 *
 * This component try to simulate DN
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
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../discoDN_client/discoC_DNUData_Request.h"
#include "../discoDN_client/discoC_DNC_worker.h"
#include "../discoDN_client/discoC_DNC_workerfun.h"
#include "../discoDN_client/discoC_DN_protocol.h"
#include "../discoDN_client/discoC_DNUDReqPool.h"
#include "../discoDN_client/discoC_DNC_Manager.h"
#include "discoDNSrv_protocol.h"
#include "discoDNSrv_CNRequest.h"
#include "discoDNSrv_simulator_private.h"
#include "discoDNSrv_simulator.h"
#include "../connection_manager/conn_manager_export.h"
#include "conn_intf_DNSimulator_impl.h"

static void _dnSmltor_sock_txErrfn (void)
{
    dms_printk(LOG_LVL_DEBUG, "_dnSmltor_sock_txErrfn called");
}

static void _dnSmltor_sock_rxErrfn (struct discoC_connection *conn)
{
    dms_printk(LOG_LVL_DEBUG, "_dnSmltor_sock_rxErrfn called");
}

discoC_conn_t *discoDNSrv_setConn_errfn (discoC_conn_t *conn)
{
    conn->tx_error_handler = _dnSmltor_sock_txErrfn;
    conn->rx_error_handler = _dnSmltor_sock_rxErrfn;

    DNSimulator_set_connIntf(&conn->connOPIntf);
    return conn;
}

static void _add_DNSmltor_tbl (DNSimulator_t *dnsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(dnSmltorMgr.DNSmltor_pool);
    plock = &(dnSmltorMgr.DNSmltor_plock);

    mutex_lock(plock);
    list_add_tail(&dnsmltor->list_DNtbl, pool);
    atomic_inc(&dnSmltorMgr.num_DNSmltor);
    dnsmltor->isInPool = true;
    mutex_unlock(plock);
}

static void _rm_DNSmltor_tbl (DNSimulator_t *dnsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(dnSmltorMgr.DNSmltor_pool);
    plock = &(dnSmltorMgr.DNSmltor_plock);

    mutex_lock(plock);

    if (dnsmltor->isInPool) {
        list_del(&dnsmltor->list_DNtbl);
        atomic_dec(&dnSmltorMgr.num_DNSmltor);
        dnsmltor->isInPool = false;
    }

    mutex_unlock(plock);
}

DNSimulator_t *create_DNSmltor (uint32_t ipaddr, uint32_t port)
{
    DNSimulator_t *dnSmltor;

    dnSmltor = discoC_mem_alloc(sizeof(DNSimulator_t), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(dnSmltor)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc DN simulator\n");
        return NULL;
    }

    dnSmltor->ipaddr = ipaddr;
    dnSmltor->port = port;

    INIT_LIST_HEAD(&dnSmltor->list_DNtbl);
    INIT_LIST_HEAD(&dnSmltor->CNReq_pool);
    atomic_set(&dnSmltor->CNReq_qsize, 0);
    spin_lock_init(&dnSmltor->CNReq_plock);
    init_waitqueue_head(&dnSmltor->CNReq_waitq);

    dnSmltor->rx_pendCNReq = NULL;
    dnSmltor->rx_pendDataSize = 0;

    dnSmltor->tx_pendCNReq = NULL;
    dnSmltor->isInPool = false;
    atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_INIT);
    _add_DNSmltor_tbl(dnSmltor);

    dnSmltor->isRunning = true;
    return dnSmltor;
}

static void _free_all_CNReq (DNSimulator_t *dnSmltor)
{
    CNReq_t *cur, *next, *cnReq;
    int32_t numReq;

    numReq = 0;

PEEK_NEXT_CNREQ:
    cnReq = NULL;
    spin_lock(&dnSmltor->CNReq_plock);

    list_for_each_entry_safe (cur, next, &dnSmltor->CNReq_pool, list_CNReq_pool) {
        if (IS_ERR_OR_NULL(cur)) {
            break;
        }

        list_del(&cur->list_CNReq_pool);
        numReq++;
        cnReq = cur;
    }
    spin_unlock(&dnSmltor->CNReq_plock);

    if (!IS_ERR_OR_NULL(cnReq)) {
        CNReq_free_request(dnSmltorMgr.DNSmltor_mpoolID, cnReq);
        goto PEEK_NEXT_CNREQ;
    }

    if (numReq) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger CNReq %d when free DN Simulator\n", numReq);
    }

}

void free_DNSmltor (DNSimulator_t *dnSmltor)
{
    _rm_DNSmltor_tbl(dnSmltor);
    _free_all_CNReq(dnSmltor);

    if (!IS_ERR_OR_NULL(dnSmltor->rx_pendCNReq)) {
        CNReq_free_request(dnSmltorMgr.DNSmltor_mpoolID, dnSmltor->rx_pendCNReq);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger rx_CNReq when free DN Simulator\n");
    }

    if (!IS_ERR_OR_NULL(dnSmltor->tx_pendCNReq)) {
        CNReq_free_request(dnSmltorMgr.DNSmltor_mpoolID, dnSmltor->tx_pendCNReq);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger tx_CNReq when free DN Simulator\n");
    }

    discoC_mem_free(dnSmltor);
}

void stop_DNSmltor (DNSimulator_t *dnSmltor)
{
    dnSmltor->isRunning = false;
    wmb();
    wake_up_interruptible_all(&dnSmltor->CNReq_waitq);
}

static void _submit_CNReq_dnSmltor (DNSimulator_t *dnSmltor, CNReq_t *cnReq)
{
    spin_lock(&dnSmltor->CNReq_plock);

    list_add_tail(&cnReq->list_CNReq_pool, &dnSmltor->CNReq_pool);
    atomic_inc(&dnSmltor->CNReq_qsize);

    spin_unlock(&dnSmltor->CNReq_plock);

    if (waitqueue_active(&dnSmltor->CNReq_waitq)) {
        wake_up_interruptible_all(&dnSmltor->CNReq_waitq);
    }
}

static int32_t _DNSrv_rcv_hder (int32_t pkt_len, int8_t *packet, CNReq_t *cnReq)
{
    CN2DN_UDRWReq_hder_t *pkt_hder;
    int8_t *pkt_ptr;

    pkt_hder = &cnReq->pkt_hder;
    pkt_ptr = packet;
    memcpy(pkt_hder, pkt_ptr, sizeof(CN2DN_UDRWReq_hder_t));
    DNClient_ReqHder_ntoh(pkt_hder);
    pkt_ptr += sizeof(CN2DN_UDRWReq_hder_t);

    if (DNUDREQ_HEART_BEAT == pkt_hder->opcode ||
            pkt_len == sizeof(CN2DN_UDRWReq_hder_t)) {
        return pkt_len;
    }

    memcpy(cnReq->arr_hbids, pkt_ptr, sizeof(uint64_t)*pkt_hder->num_hbids);
    pkt_ptr += sizeof(uint64_t)*pkt_hder->num_hbids;

    switch (pkt_hder->opcode) {
    case DNUDREQ_WRITE:
    case DNUDREQ_WRITE_NO_PAYLOAD:
    case DNUDREQ_WRITE_NO_IO:
    case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        pkt_ptr += FP_LENGTH*pkt_hder->num_hbids;
        break;
    case DNUDREQ_READ:
    case DNUDREQ_READ_NO_PAYLOAD:
    case DNUDREQ_READ_NO_IO:
    case DNUDREQ_READ_NO_PAYLOAD_IO:
        pkt_ptr += 8*pkt_hder->num_hbids;
        break;
    case DNUDREQ_HEART_BEAT:
        dms_printk(LOG_LVL_WARN, "DMSC WARN DNC req opcode %d HB err\n",
                pkt_hder->opcode);
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow DNC req opcode %d\n",
                pkt_hder->opcode);
        break;
    }

    memcpy(cnReq->phyLoc_rlo, pkt_ptr, sizeof(uint64_t)*pkt_hder->num_triplets);
    pkt_ptr += sizeof(uint64_t)*pkt_hder->num_triplets;

    CNReq_init_request(cnReq, pkt_hder->opcode, pkt_hder->num_hbids,
            pkt_hder->nnreq_id, pkt_hder->dnreq_id);
    return pkt_len;
}

int32_t discoDNSrv_rx_rcv (DNSimulator_t *dnSmltor, int32_t pkt_len, int8_t *packet)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    CNReq_t *cnReq;

    ccma_inet_aton(dnSmltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);

    if (pkt_len < PAGE_SIZE) {
        cnReq = CNReq_alloc_request(dnSmltorMgr.DNSmltor_mpoolID);
        if (IS_ERR_OR_NULL(cnReq)) {
            return pkt_len;
        }

        _DNSrv_rcv_hder(pkt_len, packet, cnReq);

        switch (cnReq->opcode) {
        case DNUDREQ_WRITE:
        case DNUDREQ_WRITE_NO_IO:
            dnSmltor->rx_pendCNReq = cnReq;
            dnSmltor->rx_pendDataSize = (cnReq->num_hbids << BIT_LEN_DMS_LB_SIZE);
            break;
        case DNUDREQ_READ:
        case DNUDREQ_WRITE_NO_PAYLOAD:
        case DNUDREQ_READ_NO_PAYLOAD:
        case DNUDREQ_READ_NO_IO:
        case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        case DNUDREQ_READ_NO_PAYLOAD_IO:
            _submit_CNReq_dnSmltor(dnSmltor, cnReq);
            break;
        case DNUDREQ_HEART_BEAT:
        default:
            CNReq_free_request(dnSmltorMgr.DNSmltor_mpoolID, cnReq);
            break;
        }
    } else {
        dnSmltor->rx_pendDataSize -= pkt_len;
        if (dnSmltor->rx_pendDataSize == 0) {
            cnReq = dnSmltor->rx_pendCNReq;
            dnSmltor->rx_pendCNReq = NULL;
            dnSmltor->rx_pendDataSize = 0;

            _submit_CNReq_dnSmltor(dnSmltor, cnReq);
        }
    }

    return pkt_len;
}

static int32_t _gen_txAck_packet (DNSimulator_t *dnSmltor, int32_t pkt_len,
        int8_t *packet)
{
    CNReq_t *myReq;
    CN2DN_UDRWResp_hder_t ackpkt;
    TXCNR_Send_State_t myStat;

    myReq = dnSmltor->tx_pendCNReq;
    myStat = atomic_read(&dnSmltor->tx_pendState);

    if (TXCNR_Send_INIT == myStat) {
        switch (myReq->opcode) {
        case DNUDREQ_WRITE:
        case DNUDREQ_WRITE_NO_PAYLOAD:
        case DNUDREQ_WRITE_NO_IO:
        case DNUDREQ_WRITE_NO_PAYLOAD_IO:
            DNSrv_gen_ackpkt(&ackpkt, DNUDRESP_MEM_ACK, myReq->num_hbids,
                    myReq->pReqID, myReq->DNReqID);
            atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_WMemAck_DONE);
            break;
        case DNUDREQ_READ:
        case DNUDREQ_READ_NO_IO:
            DNSrv_gen_ackpkt(&ackpkt, DNUDRESP_READ_PAYLOAD, myReq->num_hbids,
                    myReq->pReqID, myReq->DNReqID);
            atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_RHder_DONE);
            break;
        case DNUDREQ_READ_NO_PAYLOAD:
        case DNUDREQ_READ_NO_PAYLOAD_IO:
            DNSrv_gen_ackpkt(&ackpkt, DNUDRESP_READ_NO_PAYLOAD, myReq->num_hbids,
                    myReq->pReqID, myReq->DNReqID);
            atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_RData_DONE);
            break;
        case DNUDREQ_HEART_BEAT:
        default:
            break;
        }

        memcpy(packet, &ackpkt, sizeof(CN2DN_UDRWResp_hder_t));
    } else if (TXCNR_Send_RHder_DONE == myStat) {
        atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_RData_DONE);
    } else if (TXCNR_Send_WMemAck_DONE == myStat) {
        DNSrv_gen_ackpkt(&ackpkt, DNUDRESP_DISK_ACK, myReq->num_hbids,
                myReq->pReqID, myReq->DNReqID);
        memcpy(packet, &ackpkt, sizeof(CN2DN_UDRWResp_hder_t));
        atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_WDiskAck_DONE);
    }

    return pkt_len;
}

static CNReq_t *_peek_CNReq_ReqPool (DNSimulator_t *dnSmltor)
{
    CNReq_t *myReq;

    spin_lock(&dnSmltor->CNReq_plock);

    myReq = list_first_entry_or_null(&dnSmltor->CNReq_pool, CNReq_t,
            list_CNReq_pool);

    if (!IS_ERR_OR_NULL(myReq)) {
        list_del(&myReq->list_CNReq_pool);
        atomic_dec(&dnSmltor->CNReq_qsize);
    }

    spin_unlock(&dnSmltor->CNReq_plock);

    return myReq;
}

int32_t discoDNSrv_tx_send (DNSimulator_t *dnSmltor, int32_t pkt_len, int8_t *packet)
{
    CNReq_t *myReq;
    TXCNR_Send_State_t myStat;

PEEK_AGAIN:
    myReq = NULL;
    myStat = atomic_read(&dnSmltor->tx_pendState);
    if (TXCNR_Send_INIT == myStat) {
        wait_event_interruptible(dnSmltor->CNReq_waitq,
            !(dnSmltor->isRunning) || atomic_read(&dnSmltor->CNReq_qsize) > 0);

        if (!dnSmltor->isRunning) {
            return 0;
        }

        myReq = _peek_CNReq_ReqPool(dnSmltor);
        if (IS_ERR_OR_NULL(myReq)) {
            goto PEEK_AGAIN;
        }

        dnSmltor->tx_pendCNReq = myReq;
        atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_INIT);
    }

    _gen_txAck_packet(dnSmltor, pkt_len, packet);

    myStat = atomic_read(&dnSmltor->tx_pendState);

    if (myStat == TXCNR_Send_WDiskAck_DONE ||
            myStat == TXCNR_Send_RData_DONE) {
        atomic_set(&dnSmltor->tx_pendState, TXCNR_Send_INIT);
        CNReq_free_request(dnSmltorMgr.DNSmltor_mpoolID, dnSmltor->tx_pendCNReq);
        dnSmltor->tx_pendCNReq = NULL;
    }

    return pkt_len;
}

void init_DNSimulator_manager (void)
{
    int32_t num_req_size;

    INIT_LIST_HEAD(&dnSmltorMgr.DNSmltor_pool);
    mutex_init(&dnSmltorMgr.DNSmltor_plock);

    atomic_set(&dnSmltorMgr.num_DNSmltor, 0);
    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ*MAX_NUM_REPLICA;
    dnSmltorMgr.DNSmltor_mpoolID = register_mem_pool("DNSmltorCNReq",
            num_req_size, sizeof(CNReq_t), true);

}

static void _rel_DNSmltor_gut (void)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct mutex *plock;
    struct list_head *phead;
    DNSimulator_t *cur, *next, *smltor;

    plock = &(dnSmltorMgr.DNSmltor_plock);
    phead = &(dnSmltorMgr.DNSmltor_pool);

PEEK_FREE_NEXT:
    smltor = NULL;
    mutex_lock(plock);
    list_for_each_entry_safe (cur, next, phead, list_DNtbl) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN rel DN simulator list broken\n");
            break;
        }

        list_del(&cur->list_DNtbl);
        cur->isInPool = false;
        atomic_dec(&dnSmltorMgr.num_DNSmltor);

        smltor = cur;
        break;
    }
    mutex_unlock(plock);

    if (!IS_ERR_OR_NULL(smltor)) {
        ccma_inet_aton(smltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);

        free_DNSmltor(smltor);

        goto PEEK_FREE_NEXT;
    }
}

void rel_DNSimulator_manager (void)
{
    int32_t num_worker, ret;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DN Simulator manager\n");
    num_worker = atomic_read(&dnSmltorMgr.num_DNSmltor);

    _rel_DNSmltor_gut();

#if 0
    count = 0;
    while (atomic_read(&dnSmltorMgr.num_DNSmltor) != 0) {
        msleep(100);
        count++;

        if (count == 0 || count % 10 != 0) {
            continue;
        }

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail stop DN Ack worker %d thread running "
                "(wait count %d)\n",
                atomic_read(&dnSmltorMgr.num_DNSmltor), count);
    }
#endif
    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DN Simulator manager DONE\n");

    ret = deregister_mem_pool(dnSmltorMgr.DNSmltor_mpoolID);

    if (ret < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail free CNReq mem pool %d\n",
                dnSmltorMgr.DNSmltor_mpoolID);
    }
}
