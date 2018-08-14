/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * NN_FSReq_ProcFunc.c
 *
 */
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NN_protocol.h"
#include "NN_FSReq_ProcFunc.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../metadata_manager/space_manager_export_api.h"

int32_t fsReq_send_AllocMData (NNMData_Req_t *nnMDReq, int8_t *pkt)
{
    uint32_t pkt_size;

    pkt_size = fsReq_gen_allocSpace_header(pkt,
            (uint64_t)nnMDReq->nnMDReq_comm.ReqID,
            nnMDReq->nnMDReq_comm.ReqOPCode, nnMDReq->MetaData->usr_ioaddr.volumeID,
            nnMDReq->MetaData->usr_ioaddr.lbid_len);

    return pkt_size;
}

int32_t _gen_ReportMD_MDItem (metaD_item_t *mdItem, int8_t *pkt_buff,
        uint32_t *numItems)
{
    metaD_DNLoc_t *dnLoc_ptr;
    uint32_t i, numReplica, pktsize, rbid, len, offset;

    pktsize = (int32_t)sizeof(CN2NN_fsReportMD_chunk_t);

    numReplica = 0;
    for (i = 0; i < mdItem->num_dnLoc; i++) {
        dnLoc_ptr = &mdItem->udata_dnLoc[i];
        if (dnLoc_ptr->udata_status != UData_S_WRITE_OK) {
            continue;
        }

        decompose_triple(dnLoc_ptr->rbid_len_offset[0], &rbid, &len, &offset);
        pktsize += fsReq_gen_reportMD_DNLoc(pkt_buff + pktsize,
                dnLoc_ptr->ipaddr, dnLoc_ptr->port, rbid, offset);
        numReplica++;
    }

    pktsize = fsReq_gen_reportMD_ChunkInfo(pkt_buff,
            mdItem->space_chunkID, mdItem->num_hbids, mdItem->lbid_s,
            mdItem->arr_HBID[0], numReplica);

    *numItems = 1;

    return pktsize;
}

int32_t _gen_ReportMD_body (NNMData_Req_t *nnMDReq, int8_t *message_buffer,
        uint32_t *numItems_all)
{
    metadata_t *myMData;
    uint32_t pkt_size;
    int32_t i, numItems;

    *numItems_all = 0;
    pkt_size = 0;

    myMData = nnMDReq->MetaData;
    for (i = 0; i < myMData->num_MD_items; i++) {
        pkt_size += _gen_ReportMD_MDItem(myMData->array_metadata[i],
                message_buffer + pkt_size, &numItems);
        *numItems_all += numItems;
    }

    return pkt_size;
}

/*
 * TODO: prevent message buffer overflow
 */
int32_t fsReq_send_ReportMData (NNMData_Req_t *nnMDReq, int8_t *pkt_buff)
{
    uint32_t pkt_size, numItems;

    pkt_size = sizeof(CN2NN_fsReportMD_hder_t);

    pkt_size += _gen_ReportMD_body(nnMDReq, pkt_buff + pkt_size, &numItems);
    pkt_size = fsReq_gen_reportMD_header(pkt_buff,
            nnMDReq->nnMDReq_comm.ReqOPCode, (uint64_t)nnMDReq->nnMDReq_comm.ReqID,
            (uint64_t)nnMDReq->MetaData->usr_ioaddr.volumeID, numItems);

    return pkt_size;
}

int32_t fsReq_send_ReportFreeSpace (NNMData_Req_t *nnMDReq, int8_t *pkt_buff)
{
    uint32_t pkt_size;

    pkt_size = fsReq_gen_reportFS_header(pkt_buff,
            nnMDReq->nnMDReq_comm.ReqOPCode, nnMDReq->nnMDReq_comm.ReqID,
            nnMDReq->MetaData->usr_ioaddr.volumeID,
            nnMDReq->MetaData->array_metadata[0]->space_chunkID,
            nnMDReq->MetaData->array_metadata[0]->arr_HBID[0]);

    return pkt_size;
}

void fsReq_process_preAllocAck (int8_t *mem)
{
    struct meta_cache_dn_loc dnLoc[MAX_NUM_REPLICA];
    NN2CN_fsAllocMDResp_chunk_t chunkInfo;
    NN2CN_fsReq_DNLoc_t dnInfo;
    NNMData_Req_t *nnfsReq;
    uint64_t reqID;
    int8_t *local_ptr;
    int32_t retCode, i, j, reqResult, pktSize, numChunk;

    local_ptr = mem;
    pktSize = fsResp_parse_prealloc_ack_chkHder(local_ptr, &reqID, &numChunk);
    local_ptr += pktSize;

    nnfsReq = NNMDReq_find_ReqPool((uint32_t)reqID);
    if (IS_ERR_OR_NULL(nnfsReq)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO nn req has been GONE nn id %llu\n", reqID);
        return;
    }

    NNMDReq_cancel_timer(nnfsReq);
    nnfsReq->nnMDReq_comm.t_rcvReqAck = jiffies_64;

    NNClient_dec_numReq_NoResp(nnfsReq->nnSrv_client);
    NNClient_set_last_rcvReqID(nnfsReq->nnSrv_client, reqID);

    for (i = 0; i < numChunk; i++) {
        pktSize = fsResp_parse_prealloc_ack_chunk(local_ptr, &chunkInfo);
        local_ptr += pktSize;

        for (j = 0; j < chunkInfo.numReplica; j++) {
            memset(&dnInfo, 0, sizeof(NN2CN_fsReq_DNLoc_t));

            pktSize = fsResp_parse_prealloc_ack_DNLoc(local_ptr, &dnInfo);

            dnLoc[j].ipaddr = dnInfo.ipv4addr;
            dnLoc[j].port = dnInfo.port;
            dnLoc[j].rbid = dnInfo.rbid;
            dnLoc[j].offset = dnInfo.offset;

            local_ptr += pktSize;
        }

        if (sm_add_volume_fschunk(nnfsReq->MetaData->usr_ioaddr.volumeID,
                chunkInfo.chunkID, chunkInfo.chunkLen, chunkInfo.hbid_s,
                chunkInfo.numReplica, dnLoc)) {
        }
    }

    reqResult = 0;

    if ((retCode = nnfsReq->nnReq_endfn(nnfsReq->MetaData, nnfsReq->usr_pdata,
            &nnfsReq->nnMDReq_comm, reqResult))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail exec MD req end function ret %d\n", retCode);
    }

    NNMDReq_deref_Req(nnfsReq);
    NNMDReq_free_Req(nnfsReq);
}

void fsReq_process_ReportMDAck (int8_t *mem)
{
    NNMData_Req_t *nnfsReq;
    uint64_t reqID;
    int32_t retCode;
    uint16_t reqResult;

    fsResp_parse_reportFS_ack(mem, &reqID, &reqResult);

    nnfsReq = NNMDReq_find_ReqPool((uint32_t)reqID);
    if (IS_ERR_OR_NULL(nnfsReq)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO nn req has been GONE nn id %llu\n", reqID);
        return;
    }

    NNMDReq_cancel_timer(nnfsReq);
    nnfsReq->nnMDReq_comm.t_rcvReqAck = jiffies_64;

    NNClient_dec_numReq_NoResp(nnfsReq->nnSrv_client);
    NNClient_set_last_rcvReqID(nnfsReq->nnSrv_client, reqID);

    if ((retCode = nnfsReq->nnReq_endfn(nnfsReq->MetaData, nnfsReq->usr_pdata,
            &nnfsReq->nnMDReq_comm, reqResult))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail exec MD req end function ret %d\n", retCode);
    }

    NNMDReq_deref_Req(nnfsReq);
    NNMDReq_free_Req(nnfsReq);
}

void fsReq_process_ReportFreeAck (int8_t *mem)
{
    NNMData_Req_t *nnfsReq;
    uint64_t reqID;
    int32_t retCode;
    uint16_t reqResult;

    fsResp_parse_reportFS_ack(mem, &reqID, &reqResult);

    nnfsReq = NNMDReq_find_ReqPool((uint32_t)reqID);
    if (IS_ERR_OR_NULL(nnfsReq)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO nn req has been GONE nn id %llu\n", reqID);
        return;
    }

    NNMDReq_cancel_timer(nnfsReq);
    nnfsReq->nnMDReq_comm.t_rcvReqAck = jiffies_64;

    NNClient_dec_numReq_NoResp(nnfsReq->nnSrv_client);
    NNClient_set_last_rcvReqID(nnfsReq->nnSrv_client, reqID);

    if ((retCode = nnfsReq->nnReq_endfn(nnfsReq->MetaData, nnfsReq->usr_pdata,
            &nnfsReq->nnMDReq_comm, reqResult))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail exec MD req end function ret %d\n", retCode);
    }

    NNMDReq_deref_Req(nnfsReq);
    NNMDReq_free_Req(nnfsReq);
}
#endif
