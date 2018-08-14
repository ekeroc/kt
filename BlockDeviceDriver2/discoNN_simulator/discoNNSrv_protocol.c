/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_protocol.c
 *
 * This component provide the socket protocol parsing and packing of DN simulator
 *
 */

#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/common_util.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../discoNN_client/discoC_NN_protocol.h"
#include "discoNNSrv_protocol.h"

void NNClient_MDReqHder_ntoh (CN2NN_getMDReq_hder_t *pkt_hder)
{
    pkt_hder->opcode = ntohs(pkt_hder->opcode);
    pkt_hder->lb_s = ntohll(pkt_hder->lb_s);
    pkt_hder->nnReqID = ntohll(pkt_hder->nnReqID);
    pkt_hder->volumeID = ntohll(pkt_hder->volumeID);
    pkt_hder->lb_len = ntohl(pkt_hder->lb_len);
}

int32_t gen_MDReq_ackHeader (int8_t *pkt_buffer, uint16_t opcode, uint16_t dataSize, uint64_t cnReqID)
{
    CN2NN_MDResp_hder_t *ackhder;

    ackhder = (CN2NN_MDResp_hder_t *)pkt_buffer;
    ackhder->magic_number1 = htonll(MAGIC_NUM);
    ackhder->response_type = htons(opcode);
    ackhder->resp_data_size = htons(dataSize);
    ackhder->request_ID = htonll(cnReqID);

    return (int32_t)(sizeof(CN2NN_MDResp_hder_t));
}

int32_t gen_MDReq_MDBody1 (int8_t *pkt_buffer, uint16_t numLoc, uint16_t len_hbids)
{
    CN2NN_getMDResp_mdhder_t *hder;
    int8_t *pkt_ptr;
    int32_t pkt_len;
    int16_t tmp_s;

    hder = (CN2NN_getMDResp_mdhder_t *)pkt_buffer;
    hder->metadata_ID = 0;
    hder->metadata_ID = htonll(hder->metadata_ID);
    hder->num_metadata_items = htons(numLoc);

    pkt_ptr = pkt_buffer + sizeof(CN2NN_getMDResp_mdhder_t);
    pkt_len = sizeof(CN2NN_getMDResp_mdhder_t);

    tmp_s = htons(len_hbids);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t)); //num HBID
    pkt_ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    return pkt_len;
}

int32_t gen_MDReq_DNLoc (int8_t *pkt_buffer, int16_t len_ipaddr, int8_t *ipaddr,
        int32_t port, uint16_t num_rlo, uint64_t *phy_rlo)
{
    int8_t *pkt_ptr;
    uint64_t tmp_ll;
    int32_t pkt_len, tmp_l, i;
    int16_t tmp_s;

    pkt_ptr = pkt_buffer;
    pkt_len = 0;

    tmp_s = htons(len_ipaddr);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t)); //len of ipaddr
    pkt_ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    memcpy(pkt_ptr, ipaddr, len_ipaddr);
    pkt_ptr += len_ipaddr;
    pkt_len += len_ipaddr;

    tmp_l = htonl(port);
    memcpy(pkt_ptr, &tmp_l, sizeof(uint32_t)); //port
    pkt_ptr += sizeof(uint32_t);
    pkt_len += sizeof(uint32_t);

    tmp_s = htons(num_rlo);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t)); //num rlo
    pkt_ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    for (i = 0; i < num_rlo; i++) {
        tmp_ll = htonll(phy_rlo[i]);
        memcpy(pkt_ptr, &tmp_s, sizeof(uint64_t)); //rlo
        pkt_ptr += sizeof(uint64_t);
        pkt_len += sizeof(uint64_t);
    }

    return pkt_len;
}

#ifdef DISCO_PREALLOC_SUPPORT
int32_t gen_fsReportAlloc_ackHeader (int8_t *pkt_buffer, uint16_t opcode,
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

void NNClient_FSReportReqHder_ntoh (CN2NN_fsReportMD_hder_t *pkt_hder)
{
    pkt_hder->opcode = ntohs(pkt_hder->opcode);
    pkt_hder->request_id = ntohll(pkt_hder->request_id);
    pkt_hder->volumeID = ntohll(pkt_hder->volumeID);
    pkt_hder->numOfReportItems = ntohl(pkt_hder->numOfReportItems);
}

void NNClient_FSReportUSHder_ntoh (CN2NN_fsReportFS_hder_t *pkt_hder)
{
    pkt_hder->opcode = ntohs(pkt_hder->opcode);
    pkt_hder->request_id = ntohll(pkt_hder->request_id);
    pkt_hder->volumeID = ntohll(pkt_hder->volumeID);
    pkt_hder->chunkID = ntohll(pkt_hder->chunkID);
    pkt_hder->hbid_s = ntohll(pkt_hder->hbid_s);
}

void NNClient_FSAllocSpaceHder_ntoh (CN2NN_fsAllocMD_hder_t *pkt_hder)
{
    pkt_hder->opcode = ntohs(pkt_hder->opcode);
    pkt_hder->request_id = ntohll(pkt_hder->request_id);
    pkt_hder->volumeID = ntohll(pkt_hder->volumeID);
    pkt_hder->min_space_size = ntohl(pkt_hder->min_space_size);
}

static int32_t _NNClient_FSDNLoc_ntoh (int8_t *pkt_buffer)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    int32_t pkt_size, port, rbid, offset;
    int16_t IPLen;
    int8_t *ptr;

    ptr = pkt_buffer;
    pkt_size = 0;

    memset(ipv4addr_str, '\0', IPV4ADDR_NM_LEN);
    memcpy(&IPLen, ptr, sizeof(uint16_t));
    IPLen = ntohs(IPLen);
    pkt_size += sizeof(uint16_t);
    ptr += sizeof(uint16_t);

    memcpy(ipv4addr_str, ptr, IPLen);
    pkt_size += IPLen;
    ptr += IPLen;

    memcpy(&port, ptr, sizeof(uint32_t));
    port = ntohl(port);
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    memcpy(&rbid, ptr, sizeof(uint32_t));
    rbid = ntohl(rbid);
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    memcpy(&offset, ptr, sizeof(uint32_t));
    offset = ntohl(offset);
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    return pkt_size;
}

int32_t NNClient_FSReportReq_ntoh (int8_t *pkt_buffer)
{
    CN2NN_fsReportMD_chunk_t *chunk_pkt;
    int32_t pkt_size, i;
    int8_t *ptr;

    chunk_pkt = (CN2NN_fsReportMD_chunk_t *)pkt_buffer;
    chunk_pkt->chunkID = ntohll(chunk_pkt->chunkID);
    chunk_pkt->length = ntohl(chunk_pkt->length);
    chunk_pkt->lbid_s = ntohll(chunk_pkt->lbid_s);
    chunk_pkt->hbid_s = ntohll(chunk_pkt->hbid_s);
    chunk_pkt->numReplica = ntohs(chunk_pkt->numReplica);

    pkt_size = sizeof(CN2NN_fsReportMD_chunk_t);
    ptr = pkt_buffer + pkt_size;
    for (i = 0; i < chunk_pkt->numReplica; i++) {
        pkt_size = _NNClient_FSDNLoc_ntoh(ptr);
        ptr += pkt_size;
    }

    return pkt_size;
}
#endif
