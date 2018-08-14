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
#include "../common/discoC_mem_manager.h"
#include "../common/common_util.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../discoDN_client/discoC_DNUData_Request.h"
#include "../discoDN_client/discoC_DNC_worker.h"
#include "../discoDN_client/discoC_DNC_workerfun.h"
#include "../discoDN_client/discoC_DN_protocol.h"

void DNClient_ReqHder_ntoh (CN2DN_UDRWReq_hder_t *pkt_hder)
{
    pkt_hder->magic_num1 = ntohll(pkt_hder->magic_num1);
    pkt_hder->num_hbids = ntohs(pkt_hder->num_hbids);
    pkt_hder->num_triplets = ntohs(pkt_hder->num_triplets);
    pkt_hder->nnreq_id = ntohl(pkt_hder->nnreq_id);
    pkt_hder->dnreq_id = ntohl(pkt_hder->dnreq_id);
    pkt_hder->magic_num2 = ntohll(pkt_hder->magic_num2);
}

void DNSrv_gen_ackpkt (CN2DN_UDRWResp_hder_t *pkt, uint8_t acktype, uint16_t num_of_lb,
        uint32_t pReqID, uint32_t dnReqID)
{
    int8_t *pkt_ptr;
    uint64_t tmp_ll;
    uint32_t tmp_l;
    uint16_t tmp_s;
    uint8_t redundant_byte;

    pkt_ptr = (int8_t *)pkt;
    tmp_ll = htonll(MAGIC_NUM);
    memcpy(pkt_ptr, &tmp_ll, sizeof(uint64_t));
    pkt_ptr += sizeof(uint64_t);

    memcpy(pkt_ptr, &acktype, sizeof(uint8_t));
    pkt_ptr += sizeof(uint8_t);

    redundant_byte = 0;
    memcpy(pkt_ptr, &redundant_byte, sizeof(uint8_t));
    pkt_ptr += sizeof(uint8_t);

    tmp_s = htons(num_of_lb);
    memcpy(pkt_ptr, &tmp_s, sizeof(uint16_t));
    pkt_ptr += sizeof(uint16_t);

    memset(pkt_ptr, 0, sizeof(uint8_t)*4);
    pkt_ptr += sizeof(uint8_t)*4;

    tmp_l = htonl(pReqID);
    memcpy(pkt_ptr, &tmp_ll, sizeof(uint32_t));
    pkt_ptr += sizeof(uint32_t);

    tmp_l = htonl(dnReqID);
    memcpy(pkt_ptr, &tmp_ll, sizeof(uint32_t));
    pkt_ptr += sizeof(uint32_t);

    tmp_ll = htonll(MAGIC_NUM);
    memcpy(pkt_ptr, &tmp_ll, sizeof(uint64_t));
    pkt_ptr += sizeof(uint64_t);

    pkt->pReqID = htonl(pReqID);
    pkt->dnid = htonl(dnReqID);
}
