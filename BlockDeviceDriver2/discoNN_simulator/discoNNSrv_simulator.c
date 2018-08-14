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
#include "../discoNN_client/discoC_NN_protocol.h"
#include "discoNNSrv_protocol.h"
#include "discoNNSrv_CNRequest.h"
#include "discoNNSrv_simulator_private.h"
#include "discoNNSrv_simulator.h"
#include "../connection_manager/conn_manager_export.h"
#include "conn_intf_NNSimulator_impl.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "../discoNN_client/discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "../discoNN_client/discoC_NNClient.h"

uint64_t nnSmltor_alloc_HBID (NNSmltor_SpaceMGR_t *spaceMgr, uint32_t len)
{
    uint64_t newStart;

    newStart = atomic64_add_return(len, &spaceMgr->HBID_generator);

    return (newStart - len);
}

uint64_t nnSmltor_alloc_ChunkID (NNSimulator_t *nnsmltor, uint32_t len)
{
    uint64_t newStart;

    newStart = atomic64_add_return(len, &nnsmltor->spaceMgr.ChunkID_generator);

    return (newStart - len);
}

static void _nnSmltor_sock_txErrfn (void)
{
    dms_printk(LOG_LVL_DEBUG, "_nnSmltor_sock_txErrfn called");
}

static void _nnSmltor_sock_rxErrfn (struct discoC_connection *conn)
{
    dms_printk(LOG_LVL_DEBUG, "_nnSmltor_sock_rxErrfn called");
}

discoC_conn_t *discoNNSrv_setConn_errfn (discoC_conn_t *conn)
{
    conn->tx_error_handler = _nnSmltor_sock_txErrfn;
    conn->rx_error_handler = _nnSmltor_sock_rxErrfn;

    NNSimulator_set_connIntf(&conn->connOPIntf);
    return conn;
}

static void _add_NNSmltor_tbl (NNSimulator_t *nnsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(nnSmltorMgr.NNSmltor_pool);
    plock = &(nnSmltorMgr.NNSmltor_plock);

    mutex_lock(plock);
    list_add_tail(&nnsmltor->list_NNtbl, pool);
    atomic_inc(&nnSmltorMgr.num_NNSmltor);
    nnsmltor->isInPool = true;
    mutex_unlock(plock);
}

static void _rm_NNSmltor_tbl (NNSimulator_t *nnsmltor)
{
    struct list_head *pool;
    struct mutex *plock;

    pool = &(nnSmltorMgr.NNSmltor_pool);
    plock = &(nnSmltorMgr.NNSmltor_plock);

    mutex_lock(plock);

    if (nnsmltor->isInPool) {
        list_del(&nnsmltor->list_NNtbl);
        atomic_dec(&nnSmltorMgr.num_NNSmltor);
        nnsmltor->isInPool = false;
    }

    mutex_unlock(plock);
}

static void _init_NNSmltor_spaceMgr (NNSmltor_SpaceMGR_t *spaceMgr)
{
    int32_t i;

    for (i = 0; i < MAX_NUM_REPLICA; i++) {
        memset(spaceMgr->discoDN_ipv4addrStr[i], '\0', IPV4ADDR_NM_LEN);
        sprintf(spaceMgr->discoDN_ipv4addrStr[i], "%s%d", DEF_DN_IPPrefix, (i + 1));
        spaceMgr->discoDN_port[i] = DEF_DN_PORT;
    }

    atomic64_set(&spaceMgr->HBID_generator, 0);
    atomic64_set(&spaceMgr->ChunkID_generator, 0);
    spaceMgr->num_replica = DEF_NUM_REPLICA;

    spaceMgr->nnflowCtrl.rate_SlowStart = DEF_NNFC_RATE_SS;
    spaceMgr->nnflowCtrl.rate_SlowStartMaxReqs = DEF_NNFC_RATE_MAXREQ_SS;
    spaceMgr->nnflowCtrl.rate_AgeTime = DEF_NNFC_RATE_AgeTime;
    spaceMgr->nnflowCtrl.rate_Alloc = DEF_NNFC_RATE_ALLOC;
    spaceMgr->nnflowCtrl.rate_AllocMaxReqs = DEF_NNFC_RATE_ALLOCMaxReq;

    spaceMgr->nnflowCtrl.rate_SlowStart = htonl(spaceMgr->nnflowCtrl.rate_SlowStart);
    spaceMgr->nnflowCtrl.rate_SlowStartMaxReqs = ntohl(spaceMgr->nnflowCtrl.rate_SlowStartMaxReqs);
    spaceMgr->nnflowCtrl.rate_AgeTime = ntohl(spaceMgr->nnflowCtrl.rate_AgeTime);
    spaceMgr->nnflowCtrl.rate_Alloc = ntohl(spaceMgr->nnflowCtrl.rate_Alloc);
    spaceMgr->nnflowCtrl.rate_AllocMaxReqs = ntohl(spaceMgr->nnflowCtrl.rate_AllocMaxReqs);

}

NNSimulator_t *create_NNSmltor (uint32_t ipaddr, uint32_t port)
{
    NNSimulator_t *nnSmltor;

    nnSmltor = discoC_mem_alloc(sizeof(NNSimulator_t), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(nnSmltor)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc NN simulator\n");
        return NULL;
    }

    nnSmltor->ack_dataSize = 0;
    nnSmltor->ack_pktBuffer =
            discoC_mem_alloc(sizeof(int8_t)*ACKPKT_BUFFER_SIZE, GFP_NOWAIT);
    if (IS_ERR_OR_NULL(nnSmltor->ack_pktBuffer)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc NN simulator ack buffer\n");
        discoC_mem_free(nnSmltor);
        return NULL;
    }

    nnSmltor->ipaddr = ipaddr;
    nnSmltor->port = port;

    INIT_LIST_HEAD(&nnSmltor->list_NNtbl);
    INIT_LIST_HEAD(&nnSmltor->CNMDReq_pool);
    atomic_set(&nnSmltor->CNMDReq_qsize, 0);
    spin_lock_init(&nnSmltor->CNMDReq_plock);
    init_waitqueue_head(&nnSmltor->CNRXThd_waitq);

    nnSmltor->tx_pendCNMDReq = NULL;
    nnSmltor->isInPool = false;

    _init_NNSmltor_spaceMgr(&nnSmltor->spaceMgr);

    atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_INIT);
    _add_NNSmltor_tbl(nnSmltor);

    nnSmltor->isRunning = true;
    return nnSmltor;
}

static void _free_all_CNReq (NNSimulator_t *nnSmltor)
{
    CNMDReq_t *cur, *next, *cnReq;
    int32_t numReq;

    numReq = 0;

PEEK_NEXT_CNREQ:
    cnReq = NULL;
    spin_lock(&nnSmltor->CNMDReq_plock);

    list_for_each_entry_safe (cur, next, &nnSmltor->CNMDReq_pool, list_CNMDReq_pool) {
        if (IS_ERR_OR_NULL(cur)) {
            break;
        }

        list_del(&cur->list_CNMDReq_pool);
        numReq++;
        cnReq = cur;
    }
    spin_unlock(&nnSmltor->CNMDReq_plock);

    if (!IS_ERR_OR_NULL(cnReq)) {
        CNMDReq_free_request(nnSmltorMgr.NNSmltor_mpoolID, cnReq);
        goto PEEK_NEXT_CNREQ;
    }

    if (numReq) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger CNMDReq %d when free NN Simulator\n", numReq);
    }

}

void free_NNSmltor (NNSimulator_t *nnSmltor)
{
    _rm_NNSmltor_tbl(nnSmltor);
    _free_all_CNReq(nnSmltor);

    if (!IS_ERR_OR_NULL(nnSmltor->tx_pendCNMDReq)) {
        CNMDReq_free_request(nnSmltorMgr.NNSmltor_mpoolID, nnSmltor->tx_pendCNMDReq);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger tx_CNMDReq when free NN Simulator\n");
    }

    discoC_mem_free(nnSmltor->ack_pktBuffer);
    discoC_mem_free(nnSmltor);
}

void stop_NNSmltor (NNSimulator_t *nnSmltor)
{
    nnSmltor->isRunning = false;
    wmb();
    wake_up_interruptible_all(&nnSmltor->CNRXThd_waitq);
}

static void _submit_CNMDReq_nnSmltor (NNSimulator_t *nnSmltor, CNMDReq_t *cnReq)
{
    spin_lock(&nnSmltor->CNMDReq_plock);

    list_add_tail(&cnReq->list_CNMDReq_pool, &nnSmltor->CNMDReq_pool);
    atomic_inc(&nnSmltor->CNMDReq_qsize);

    spin_unlock(&nnSmltor->CNMDReq_plock);

    if (waitqueue_active(&nnSmltor->CNRXThd_waitq)) {
        wake_up_interruptible_all(&nnSmltor->CNRXThd_waitq);
    }
}

static uint32_t _NNSrv_rcv_opcode (int8_t *packet, uint16_t *opcode)
{
    int8_t *pkt_ptr;
    uint64_t mnum;
    uint32_t len;

    pkt_ptr = packet;
    len = 0;

    memcpy(&mnum, pkt_ptr, sizeof(uint64_t));
    pkt_ptr += sizeof(uint64_t);
    len += sizeof(uint64_t);

    memcpy((int8_t *)opcode, pkt_ptr, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);
    len += sizeof(uint16_t);

    *opcode = ntohs(*opcode);
    return len;
}

static void _NNSrv_rcv_MDReq (NNSimulator_t *nnSmltor, uint16_t opcode,
        int32_t pkt_len, int8_t *packet)
{
    CN2NN_getMDReq_hder_t pkt_hder;
    CNMDReq_t *cnReq;
    int8_t *ptr;
    uint32_t remain_len;

    remain_len = pkt_len;
    ptr = packet;
    memcpy(&pkt_hder, ptr, sizeof(CN2NN_getMDReq_hder_t));
    remain_len -= sizeof(CN2NN_getMDReq_hder_t);
    ptr += sizeof(CN2NN_getMDReq_hder_t);

    NNClient_MDReqHder_ntoh(&pkt_hder);

    cnReq = CNMDReq_alloc_request(nnSmltorMgr.NNSmltor_mpoolID);
    if (IS_ERR_OR_NULL(cnReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN nnSimulator fail alloc CNMDReq\n");
        return;
    }

    CNMDReq_init_request(cnReq, opcode, pkt_hder.nnReqID, pkt_hder.volumeID);
    cnReq->lbid_s = pkt_hder.lb_s;
    cnReq->lbid_len = pkt_hder.lb_len;

    _submit_CNMDReq_nnSmltor(nnSmltor, cnReq);
}

static void _NNSrv_rcv_MDReportReq (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    printk("LEOG: not support now\n");
}

#ifdef DISCO_PREALLOC_SUPPORT
static void _NNSrv_rcv_PreallocReq (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    CN2NN_fsAllocMD_hder_t *pkt_hder;
    CNMDReq_t *cnReq;
    int8_t *ptr;
    uint32_t remain_len;

    pkt_hder = (CN2NN_fsAllocMD_hder_t *)packet;
    remain_len = pkt_len - sizeof(CN2NN_fsAllocMD_hder_t);
    ptr = packet + sizeof(CN2NN_fsAllocMD_hder_t);

    NNClient_FSAllocSpaceHder_ntoh(pkt_hder);
    cnReq = CNMDReq_alloc_request(nnSmltorMgr.NNSmltor_mpoolID);
    if (IS_ERR_OR_NULL(cnReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN nnSimulator fail alloc CNMDReq\n");
        return;
    }

    CNMDReq_init_request(cnReq, pkt_hder->opcode, pkt_hder->request_id,
            pkt_hder->volumeID);
    _submit_CNMDReq_nnSmltor(nnSmltor, cnReq);
}

static void _NNSrv_rcv_ReportAlloc (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    CN2NN_fsReportMD_hder_t *pkt_hder;
    CNMDReq_t *cnReq;
    int8_t *ptr;
    uint32_t remain_len, i, itemSize;

    pkt_hder = (CN2NN_fsReportMD_hder_t *)packet;
    remain_len = pkt_len - sizeof(CN2NN_fsReportMD_hder_t);
    ptr = packet + sizeof(CN2NN_fsReportMD_hder_t);

    NNClient_FSReportReqHder_ntoh(pkt_hder);

    for (i = 0; i < pkt_hder->numOfReportItems; i++) {
        itemSize = NNClient_FSReportReq_ntoh(ptr);
        ptr += itemSize;
        remain_len -= itemSize;
    }

    cnReq = CNMDReq_alloc_request(nnSmltorMgr.NNSmltor_mpoolID);
    if (IS_ERR_OR_NULL(cnReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN nnSimulator fail alloc CNMDReq\n");
        return;
    }

    CNMDReq_init_request(cnReq, pkt_hder->opcode, pkt_hder->request_id,
            pkt_hder->volumeID);
    _submit_CNMDReq_nnSmltor(nnSmltor, cnReq);
}

static void _NNSrv_rcv_ReturnSpace (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    CN2NN_fsReportFS_hder_t *pkt_hder;
    CNMDReq_t *cnReq;
    int8_t *ptr;
    uint32_t remain_len;

    pkt_hder = (CN2NN_fsReportFS_hder_t *)packet;
    remain_len = pkt_len - sizeof(CN2NN_fsReportFS_hder_t);
    ptr = packet + sizeof(CN2NN_fsReportFS_hder_t);

    NNClient_FSReportUSHder_ntoh(pkt_hder);

    cnReq = CNMDReq_alloc_request(nnSmltorMgr.NNSmltor_mpoolID);
    if (IS_ERR_OR_NULL(cnReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN nnSimulator fail alloc CNMDReq\n");
        return;
    }

    CNMDReq_init_request(cnReq, pkt_hder->opcode, pkt_hder->request_id,
            pkt_hder->volumeID);
    _submit_CNMDReq_nnSmltor(nnSmltor, cnReq);
}
#endif

int32_t discoNNSrv_rx_rcv (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    //int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint8_t *pkt_ptr;
    uint16_t opcode;

    //ccma_inet_aton(nnSmltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    pkt_ptr = packet;

    _NNSrv_rcv_opcode(pkt_ptr, &opcode);

    if (opcode == REQ_NN_UPDATE_HEARTBEAT) {
        return pkt_len;
    }

    switch (opcode) {
    case REQ_NN_QUERY_MDATA_READIO:
    case REQ_NN_QUERY_TRUE_MDATA_READIO:
    case REQ_NN_ALLOC_MDATA_AND_GET_OLD:
    case REQ_NN_ALLOC_MDATA_NO_OLD:
    case REQ_NN_QUERY_MDATA_WRITEIO:
        _NNSrv_rcv_MDReq(nnSmltor, opcode, pkt_len, packet);
        break;
    case REQ_NN_REPORT_READIO_RESULT:
    case REQ_NN_REPORT_WRITEIO_RESULT:
        _NNSrv_rcv_MDReportReq(nnSmltor, pkt_len, packet);
        break;
#ifdef DISCO_PREALLOC_SUPPORT
    case REQ_NN_PREALLOC_SPACE:
        _NNSrv_rcv_PreallocReq(nnSmltor, pkt_len, packet);
        break;
    case REQ_NN_REPORT_SPACE_MDATA:
        _NNSrv_rcv_ReportAlloc(nnSmltor, pkt_len, packet);
        break;
    case REQ_NN_RETURN_SPACE:
        _NNSrv_rcv_ReturnSpace(nnSmltor, pkt_len, packet);
        break;
#endif
    case REQ_NN_REPORT_WRITEIO_RESULT_DEPRECATED:
    case REQ_NN_UPDATE_HEARTBEAT:
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN NNSmlator got inval opcode %d\n",
                opcode);
        break;
    }

    return pkt_len;
}

static int32_t _gen_MDReq_Metadata (NNSmltor_SpaceMGR_t *spaceMgr, int8_t *pkt_buffer,
        CNMDReq_t *myReq)
{
    int8_t *pkt_ptr;
    uint64_t tmpll, hbid_s, phyrlo;
    int32_t pkt_len, i, dnLocSize;
    uint16_t numReplica, tmp_s;

    //TODO: get volume replica first

    pkt_ptr = pkt_buffer;
    pkt_len = 0;

    pkt_len = gen_MDReq_MDBody1(pkt_ptr, 1, myReq->lbid_len);
    pkt_ptr += pkt_len;

    hbid_s = nnSmltor_alloc_HBID(spaceMgr, myReq->lbid_len);
    for (i = 0; i < myReq->lbid_len; i++) {
        tmpll = htonll(hbid_s);
        memcpy(pkt_ptr, &tmpll, sizeof(uint64_t));
        pkt_ptr += sizeof(uint64_t);
        pkt_len += sizeof(uint64_t);
        hbid_s++;
    }

    numReplica = spaceMgr->num_replica;
    tmp_s = htons(numReplica);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    phyrlo = compose_triple(0, myReq->lbid_len, 0);
    for (i = 0; i < numReplica; i++) {
        dnLocSize = gen_MDReq_DNLoc(pkt_ptr,
                strlen(spaceMgr->discoDN_ipv4addrStr[i]),
                spaceMgr->discoDN_ipv4addrStr[i], spaceMgr->discoDN_port[i],
                1, &phyrlo);
        pkt_ptr += dnLocSize;
        pkt_len += dnLocSize;
    }

    tmp_s = 0; //md state
    tmp_s = htons(tmp_s);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    memcpy(pkt_ptr, &(spaceMgr->nnflowCtrl), sizeof(CN2NN_getMDResp_fcpkt_t));
    pkt_ptr += sizeof(CN2NN_getMDResp_fcpkt_t);
    pkt_len += sizeof(CN2NN_getMDResp_fcpkt_t);

    return pkt_len;
}

static int32_t _gen_MDReq_txAck_packet (NNSimulator_t *nnSmltor, int32_t pkt_len,
        int8_t *packet)
{
    CNMDReq_t *myReq;
    TXNNResp_Send_State_t myStat;

    myReq = nnSmltor->tx_pendCNMDReq;
    myStat = atomic_read(&nnSmltor->tx_pendState);

    if (TXNNResp_Send_INIT == myStat) {
        nnSmltor->ack_dataSize = _gen_MDReq_Metadata(&nnSmltor->spaceMgr,
                nnSmltor->ack_pktBuffer, myReq);
        gen_MDReq_ackHeader(packet, NN_RESP_METADATA_REQUEST,
                nnSmltor->ack_dataSize + 8, myReq->CNMDReqID); //NOTE ack size include request ID
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Hder_DONE);
    } else if (TXNNResp_Send_Hder_DONE == myStat) {
        memcpy(packet, nnSmltor->ack_pktBuffer, nnSmltor->ack_dataSize);
        nnSmltor->ack_dataSize = 0;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Body_DONE);
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow state %d NNSmltor gen MDAck\n",
                myStat);
    }

    return pkt_len;
}

#ifdef DISCO_PREALLOC_SUPPORT
static int32_t _gen_fsReportAlloc_body (int8_t *pkt_buffer, CNMDReq_t *myReq)
{
    int8_t *ptr;
    uint64_t reqID;
    int32_t pkt_size;
    uint16_t respResult;

    pkt_size = 0;
    ptr = pkt_buffer;

    reqID = htonll(myReq->CNMDReqID);
    memcpy(ptr, &reqID, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    ptr += sizeof(uint64_t);

    respResult = 0;
    respResult = htons(respResult);
    memcpy(ptr, &respResult, sizeof(uint16_t));
    pkt_size += sizeof(uint16_t);
    ptr += sizeof(uint16_t);

    return pkt_size;
}

static int32_t _gen_fsReportAlloc_txAck_packet (NNSimulator_t *nnSmltor,
        int32_t pkt_len, int8_t *packet)
{
    CNMDReq_t *myReq;
    TXNNResp_Send_State_t myStat;

    myReq = nnSmltor->tx_pendCNMDReq;
    myStat = atomic_read(&nnSmltor->tx_pendState);

    if (TXNNResp_Send_INIT == myStat) {
        nnSmltor->ack_dataSize = _gen_fsReportAlloc_body(nnSmltor->ack_pktBuffer, myReq);
        gen_fsReportAlloc_ackHeader(packet, NN_RESP_FS_REPORT_MDATA,
                nnSmltor->ack_dataSize); //NOTE ack size include request ID
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Hder_DONE);
    } else if (TXNNResp_Send_Hder_DONE == myStat) {
        memcpy(packet, nnSmltor->ack_pktBuffer, nnSmltor->ack_dataSize);
        nnSmltor->ack_dataSize = 0;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Body_DONE);
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknow state %d NNSmltor gen fsReportAllocAck\n",
                myStat);
    }

    return pkt_len;
}

static int32_t _gen_fsAllocSpace_body (int8_t *pkt_buffer, CNMDReq_t *myReq)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    int8_t *ptr;
    uint64_t reqID, chunkID, hbid_s;
    int32_t pkt_size, num_chunk, i, len, port, rbid, offset;
    uint16_t numReplica, replica_norder, iplen_norder, iplen;

    pkt_size = 0;
    ptr = pkt_buffer;

    reqID = htonll(myReq->CNMDReqID);
    memcpy(ptr, &reqID, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    ptr += sizeof(uint64_t);

    num_chunk = 1;
    num_chunk = htonl(num_chunk);
    memcpy(ptr, &num_chunk, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    chunkID = 10011;
    chunkID = htonll(chunkID);
    memcpy(ptr, &chunkID, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    ptr += sizeof(uint64_t);

    len = 65536;
    len = htonl(len);
    memcpy(ptr, &len, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    hbid_s = 99887766;
    hbid_s = htonll(hbid_s);
    memcpy(ptr, &hbid_s, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    ptr += sizeof(uint64_t);

    numReplica = 2;
    replica_norder = htons(numReplica);
    memcpy(ptr, &replica_norder, sizeof(uint16_t));
    pkt_size += sizeof(uint16_t);
    ptr += sizeof(uint16_t);

    for (i = 0; i < numReplica; i++) {
        memset(ipv4addr_str, '\0', IPV4ADDR_NM_LEN);
        iplen = sprintf((char *)ipv4addr_str, "192.168.168.%d", i);

        iplen_norder = htons(iplen);
        memcpy(ptr, &iplen_norder, sizeof(uint16_t));
        pkt_size += sizeof(uint16_t);
        ptr += sizeof(uint16_t);

        memcpy(ptr, ipv4addr_str, iplen);
        pkt_size += iplen;
        ptr += iplen;

        port = 20100;
        port = htonl(port);
        memcpy(ptr, &port, sizeof(uint32_t));
        pkt_size += sizeof(uint32_t);
        ptr += sizeof(uint32_t);

        rbid = 1003 + i;
        rbid = htonl(rbid);
        memcpy(ptr, &rbid, sizeof(uint32_t));
        pkt_size += sizeof(uint32_t);
        ptr += sizeof(uint32_t);

        offset = 192 + i;
        offset = htonl(offset);
        memcpy(ptr, &offset, sizeof(uint32_t));
        pkt_size += sizeof(uint32_t);
        ptr += sizeof(uint32_t);
    }

    return pkt_size;
}

int32_t gen_fsAllocSpace_ackHeader (int8_t *pkt_buffer, uint16_t opcode,
        uint16_t dataSize)
{
    CN2NN_MDResp_hder_t *ackhder;

    ackhder = (CN2NN_MDResp_hder_t *)pkt_buffer;
    ackhder->magic_number1 = htonll(MAGIC_NUM);
    ackhder->response_type = htons(opcode);
    ackhder->resp_data_size = htons(dataSize);
    ackhder->request_ID = htonll(MAGIC_NUM);

    return (int32_t)(sizeof(CN2NN_MDResp_hder_t));
}

static int32_t _gen_fsAllocS_txAck_packet (NNSimulator_t *nnSmltor,
        int32_t pkt_len, int8_t *packet)
{
    CNMDReq_t *myReq;
    TXNNResp_Send_State_t myStat;

    myReq = nnSmltor->tx_pendCNMDReq;
    myStat = atomic_read(&nnSmltor->tx_pendState);

    if (TXNNResp_Send_INIT == myStat) {
        nnSmltor->ack_dataSize = _gen_fsAllocSpace_body(nnSmltor->ack_pktBuffer, myReq);
        gen_fsAllocSpace_ackHeader(packet, NN_RESP_FS_PREALLOCATION,
                nnSmltor->ack_dataSize); //NOTE ack size include request ID
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Hder_DONE);
    } else if (TXNNResp_Send_Hder_DONE == myStat) {
        memcpy(packet, nnSmltor->ack_pktBuffer, nnSmltor->ack_dataSize);
        nnSmltor->ack_dataSize = 0;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Body_DONE);
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknow state %d NNSmltor gen fsReportAllocAck\n",
                myStat);
    }

    return pkt_len;
}

static int32_t _gen_fsReportUS_txAck_packet (NNSimulator_t *nnSmltor,
        int32_t pkt_len, int8_t *packet)
{
    CNMDReq_t *myReq;
    TXNNResp_Send_State_t myStat;

    myReq = nnSmltor->tx_pendCNMDReq;
    myStat = atomic_read(&nnSmltor->tx_pendState);

    if (TXNNResp_Send_INIT == myStat) {
        nnSmltor->ack_dataSize = _gen_fsReportAlloc_body(nnSmltor->ack_pktBuffer, myReq);
        gen_fsReportAlloc_ackHeader(packet, NN_RESP_FS_REPORT_FREE,
                nnSmltor->ack_dataSize); //NOTE ack size include request ID
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Hder_DONE);
    } else if (TXNNResp_Send_Hder_DONE == myStat) {
        memcpy(packet, nnSmltor->ack_pktBuffer, nnSmltor->ack_dataSize);
        nnSmltor->ack_dataSize = 0;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_Body_DONE);
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknow state %d NNSmltor gen fsReportAllocAck\n",
                myStat);
    }

    return pkt_len;
}
#endif

static int32_t _gen_txAck_packet (NNSimulator_t *nnSmltor, int32_t pkt_len,
        int8_t *packet)
{
    CNMDReq_t *myReq;

    myReq = nnSmltor->tx_pendCNMDReq;
    switch (myReq->opcode) {
    case REQ_NN_QUERY_MDATA_READIO:
    case REQ_NN_QUERY_TRUE_MDATA_READIO:
    case REQ_NN_ALLOC_MDATA_AND_GET_OLD:
    case REQ_NN_ALLOC_MDATA_NO_OLD:
    case REQ_NN_QUERY_MDATA_WRITEIO:
        _gen_MDReq_txAck_packet(nnSmltor, pkt_len, packet);
        break;
    case REQ_NN_REPORT_READIO_RESULT:
    case REQ_NN_REPORT_WRITEIO_RESULT:
        printk("LEGO: not support not\n");
        break;
#ifdef DISCO_PREALLOC_SUPPORT
    case REQ_NN_PREALLOC_SPACE:
        _gen_fsAllocS_txAck_packet(nnSmltor, pkt_len, packet);
        break;
    case REQ_NN_REPORT_SPACE_MDATA:
        _gen_fsReportAlloc_txAck_packet(nnSmltor, pkt_len, packet);
        break;
    case REQ_NN_RETURN_SPACE:
        _gen_fsReportUS_txAck_packet(nnSmltor, pkt_len, packet);
        break;
#endif
    case REQ_NN_REPORT_WRITEIO_RESULT_DEPRECATED: //useless now, when send ci report with opcode = 6, nn won't ack
    case REQ_NN_UPDATE_HEARTBEAT:
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow opcode %d nnSmltor gen ack\n",
                myReq->opcode);
        break;
    }

    return pkt_len;
}

static CNMDReq_t *_peek_CNMDReq_ReqPool (NNSimulator_t *nnSmltor)
{
    CNMDReq_t *myReq;

    spin_lock(&nnSmltor->CNMDReq_plock);

    myReq = list_first_entry_or_null(&nnSmltor->CNMDReq_pool, CNMDReq_t,
            list_CNMDReq_pool);

    if (!IS_ERR_OR_NULL(myReq)) {
        list_del(&myReq->list_CNMDReq_pool);
        atomic_dec(&nnSmltor->CNMDReq_qsize);
    }

    spin_unlock(&nnSmltor->CNMDReq_plock);

    return myReq;
}

int32_t discoNNSrv_tx_send (NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet)
{
    CNMDReq_t *myReq;
    TXNNResp_Send_State_t myStat;

PEEK_AGAIN:
    myReq = NULL;
    myStat = atomic_read(&nnSmltor->tx_pendState);
    if (TXNNResp_Send_INIT == myStat) {
        wait_event_interruptible(nnSmltor->CNRXThd_waitq,
            !(nnSmltor->isRunning) || atomic_read(&nnSmltor->CNMDReq_qsize) > 0);

        if (!nnSmltor->isRunning) {
            return 0;
        }

        myReq = _peek_CNMDReq_ReqPool(nnSmltor);
        if (IS_ERR_OR_NULL(myReq)) {
            goto PEEK_AGAIN;
        }

        nnSmltor->tx_pendCNMDReq = myReq;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_INIT);
    }

    _gen_txAck_packet(nnSmltor, pkt_len, packet);
    myStat = atomic_read(&nnSmltor->tx_pendState);
    if (myStat == TXNNResp_Send_Body_DONE) {
        myReq = nnSmltor->tx_pendCNMDReq;
        atomic_set(&nnSmltor->tx_pendState, TXNNResp_Send_INIT);
        CNMDReq_free_request(nnSmltorMgr.NNSmltor_mpoolID, nnSmltor->tx_pendCNMDReq);
        nnSmltor->tx_pendCNMDReq = NULL;
    }

    return pkt_len;
}

void init_NNSimulator_manager (void)
{
    int32_t num_req_size;

    INIT_LIST_HEAD(&nnSmltorMgr.NNSmltor_pool);
    mutex_init(&nnSmltorMgr.NNSmltor_plock);

    atomic_set(&nnSmltorMgr.num_NNSmltor, 0);
    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    nnSmltorMgr.NNSmltor_mpoolID = register_mem_pool("NNSmltorCNMDReq",
            num_req_size, sizeof(CNMDReq_t), true);

}

static void _rel_NNSmltor_gut (void)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct mutex *plock;
    struct list_head *phead;
    NNSimulator_t *cur, *next, *smltor;

    plock = &(nnSmltorMgr.NNSmltor_plock);
    phead = &(nnSmltorMgr.NNSmltor_pool);

PEEK_FREE_NEXT:
    smltor = NULL;
    mutex_lock(plock);
    list_for_each_entry_safe (cur, next, phead, list_NNtbl) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN rel NN simulator list broken\n");
            break;
        }

        list_del(&cur->list_NNtbl);
        cur->isInPool = false;
        atomic_dec(&nnSmltorMgr.num_NNSmltor);

        smltor = cur;
        break;
    }
    mutex_unlock(plock);

    if (!IS_ERR_OR_NULL(smltor)) {
        ccma_inet_aton(smltor->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);

        free_NNSmltor(smltor);

        goto PEEK_FREE_NEXT;
    }
}

void rel_NNSimulator_manager (void)
{
    int32_t num_worker, ret;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel NN Simulator manager\n");
    num_worker = atomic_read(&nnSmltorMgr.num_NNSmltor);

    _rel_NNSmltor_gut();

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
    dms_printk(LOG_LVL_INFO, "DMSC INFO rel NN Simulator manager DONE\n");

    ret = deregister_mem_pool(nnSmltorMgr.NNSmltor_mpoolID);

    if (ret < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail free CNMDReq mem pool %d\n",
                nnSmltorMgr.NNSmltor_mpoolID);
    }
}
