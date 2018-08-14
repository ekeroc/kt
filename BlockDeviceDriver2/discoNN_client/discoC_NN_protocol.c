/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DN_protocol.c
 *
 * This component aim for paring CN - NN protocol
 *
 */
#include <linux/rwsem.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../metadata_manager/metadata_manager.h"
//#include "../io_manager/discoC_IO_Manager.h"
#include "discoC_NN_protocol.h"
#include "discoC_NNC_Manager_api.h"

/**
 * NNProto_gen_HBPacket - generate nn connection heartbeat packet
 * @pkt_buffer: memory buffer to hold packet
 * @max_buff_size: size of pkt_buffer
 *
 * Return: size of hb packet
 */
int32_t NNProto_gen_HBPacket (int8_t *pkt_buffer, int32_t max_buff_size)
{
    int8_t *ptr;
    uint64_t tmp_ptr_ll;
    int32_t pkt_len;
    uint16_t tmp_ptr_s;

    if (IS_ERR_OR_NULL(pkt_buffer)) {
        return -EINVAL;
    }

    ptr = pkt_buffer;
    pkt_len = 0;

    tmp_ptr_ll = htonll(MAGIC_NUM);
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

    tmp_ptr_s = (uint16_t)htons(REQ_NN_UPDATE_HEARTBEAT);
    memcpy(ptr, &tmp_ptr_s, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    return pkt_len;
}

/******* NN metadata request packet *******/

/**
 * NNProto_gen_getMDReqheader - generate acquire mentadata request packet
 * @opcode: operation code of request (query metadata for read / query true metadata for read ...)
 *          reference discoC_NNC_Manager_api.h
 * @reqID: request ID of metadata request, namenode need reply this to clientnode
 * @volID: which volume of metadata request want to acquire metadata
 * @lbs: start lba of @volID to acquire metadata
 * @lblen: lba length of @volID to acquire metadata
 * @nnReq_pkt: a request packet for fill and return to caller
 */
void NNProto_gen_getMDReqheader (int16_t opcode, uint32_t reqID, uint32_t volID,
        uint64_t lbs, uint32_t lblen, CN2NN_getMDReq_hder_t *nnReq_pkt)
{
    nnReq_pkt->opcode = htons(opcode);
    nnReq_pkt->lb_len = htonl(lblen);
    nnReq_pkt->nnReqID = htonll((uint64_t)reqID);

    nnReq_pkt->lb_s = htonll(lbs);
    nnReq_pkt->volumeID = htonll((uint64_t)volID);
    nnReq_pkt->magic_number1 = mnum_net_order;
    nnReq_pkt->magic_number2 = mnum_net_order;
}

/******* NN metadata response header packet *******/
/**
 * NNProto_MDRespheader_ntoh - change byte order of metadata request response packet
 *                          from network order to host order
 * @pkt: response packet
 *
 * Return: whether packet is validate: 0 validate
 */
int32_t NNProto_MDRespheader_ntoh (CN2NN_MDResp_hder_t *pkt)
{
    int32_t ret;

    pkt->response_type = ntohs(pkt->response_type);
    pkt->resp_data_size = ntohs(pkt->resp_data_size);
    pkt->request_ID = ntohll(pkt->request_ID);

    ret = -EILSEQ;

    do {
        if (pkt->magic_number1 != mnum_net_order) {
            break;
        }

        if (pkt->resp_data_size == 0) {
            break;
        }

        //NOTE: pkt size including request ID
        pkt->resp_data_size = pkt->resp_data_size - sizeof(uint64_t);

        ret = 0;
    } while (0);

    return ret;
}

/**
 * NNProto_getMDResp_MDheader_ntoh - change byte order of metadata response packet
 *                          from network order to host order
 * @md_hder: metadata response header
 */
void NNProto_getMDResp_MDheader_ntoh (CN2NN_getMDResp_mdhder_t *md_hder)
{
    md_hder->metadata_ID = ntohll(md_hder->metadata_ID);
    md_hder->num_metadata_items = ntohs(md_hder->num_metadata_items);
}

/**
 * NNAck_MDHeader_ntoh_order - change byte order of datanode location in response
 *                          from network order to host order
 * @MDItem: An metadata item data structure for holding datanode location information
 * @mem_buff: memory byte buffer that hold datanode location
 *
 * Return: how many bytes in mem_buff be translated into MDItem
 */
static uint32_t NNAck_DNLoc_ntoh_order (metaD_item_t *MDItem, uint8_t *mem_buff)
{
    int8_t ipv4addr[IPV4ADDR_NM_LEN];
    metaD_DNLoc_t *dnLoc_ptr;
    uint8_t *curptr;
    uint32_t item_size, i, j, port, ipaddr;
    uint16_t strlen_ipv4addr, num_rlo;

    item_size = 0;
    curptr = mem_buff;
    memset(ipv4addr, '\0', IPV4ADDR_NM_LEN);

    for (i = 0; i < MDItem->num_dnLoc; i++) {
        do {
#ifdef DISCO_ERC_SUPPORT
            if (MData_chk_ERCProt(MDItem)) {
                dnLoc_ptr = &(MDItem->erc_dnLoc[i]);
                DMS_MEM_READ_NEXT(curptr, &(MDItem->erc_dnLoc[i].ERC_HBID),
                        sizeof(uint64_t));
                MDItem->erc_dnLoc[i].ERC_HBID = ntohll(MDItem->erc_dnLoc[i].ERC_HBID);
                item_size += sizeof(uint64_t);
                break;
            }
#endif

            dnLoc_ptr = &MDItem->udata_dnLoc[i];
        } while (0);

        DMS_MEM_READ_NEXT(curptr, &strlen_ipv4addr, sizeof(int16_t));
        item_size += sizeof(int16_t);
        strlen_ipv4addr = ntohs(strlen_ipv4addr);

        DMS_MEM_READ_NEXT(curptr, ipv4addr, sizeof(int8_t)*strlen_ipv4addr);
        item_size += strlen_ipv4addr;
        ipaddr = ccma_inet_ntoa(ipv4addr);

        DMS_MEM_READ_NEXT(curptr, &port, sizeof(uint32_t));
        item_size += sizeof(uint32_t);
        port = ntohl(port);

        do {
#ifdef DISCO_ERC_SUPPORT
            if (MData_chk_ERCProt(MDItem)) {
                num_rlo = 1;
                break;
            }
#endif
            DMS_MEM_READ_NEXT(curptr, &num_rlo, sizeof(uint16_t));
            item_size += sizeof(uint16_t);
            num_rlo = ntohs(num_rlo);
        } while (0);

        MData_init_DNLoc(dnLoc_ptr, ipaddr, port, num_rlo);

        DMS_MEM_READ_NEXT(curptr, &dnLoc_ptr->rbid_len_offset,
                sizeof(uint64_t)*dnLoc_ptr->num_phyLocs);
        for (j = 0; j < dnLoc_ptr->num_phyLocs; j++) {
            dnLoc_ptr->rbid_len_offset[j] = ntohll(dnLoc_ptr->rbid_len_offset[j]);
        }
        item_size += sizeof(uint64_t)*dnLoc_ptr->num_phyLocs;
    }

    return item_size;
}

#ifdef DISCO_ERC_SUPPORT
/**
 * NNProto_getMDResp_MDItem_ntoh - change byte order of metadata item in response
 *                           from network order to host order
 * @MDItem  : An metadata item data structure for metadata
 * @startLB : start LB of metadata
 * @mem_buff: memory byte buffer that hold metadata
 *
 * Return: how many bytes in mem_buff be translated into MDItem
 */
uint32_t NNAckERC_MDItem_ntoh_order (metaD_item_t *MDItem, uint64_t startLB,
        uint8_t *mem_buff)
{
    uint8_t *curptr;
    uint64_t targetHB;
    uint32_t item_size, dnLocPkt_size;
    int16_t len_DN;

    item_size = 0;
    curptr = mem_buff;

    DMS_MEM_READ_NEXT(curptr, &targetHB, sizeof(uint64_t));
    targetHB = ntohll(targetHB);
    item_size += sizeof(uint64_t);

    DMS_MEM_READ_NEXT(curptr, (uint8_t *)&len_DN, sizeof(uint16_t));
    len_DN = ntohs(len_DN);
    item_size += sizeof(uint16_t);

    MData_init_MDItem(MDItem, startLB, 1, len_DN, MData_VALID, 1, 1, false);
    MDItem->blk_md_type = MDPROT_ERC;

    MDItem->arr_HBID[0] = targetHB;
    MDItem->erc_dnLoc = discoC_mem_alloc(sizeof(metaD_DNLoc_t)*len_DN, GFP_NOWAIT);
    if (IS_ERR_OR_NULL(MDItem->erc_dnLoc)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem dnLoc parse NN ack\n");
        MDItem->num_dnLoc = 0;
    } else {
        MDItem->num_dnLoc = len_DN;
    }
    MDItem->erc_buffer = (uint8_t *)
            discoC_mem_alloc(sizeof(uint8_t)*PAGE_SIZE, GFP_NOWAIT);
    if (IS_ERR_OR_NULL(MDItem->erc_buffer)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem ERC data buffer\n");
    }

    dnLocPkt_size = NNAck_DNLoc_ntoh_order(MDItem, curptr);
    curptr += dnLocPkt_size;
    item_size += dnLocPkt_size;

    return item_size;
}
#endif

/**
 * NNProto_getMDResp_MDItem_ntoh - change byte order of metadata item in acquire metadata response
 *                           from network order to host order
 * @MDItem  : An metadata item data structure for metadata
 * @startLB : start LB of metadata
 * @mem_buff: memory byte buffer that hold metadata
 *
 * Return: how many bytes in mem_buff be translated into MDItem
 */
uint32_t NNProto_getMDResp_MDItem_ntoh (metaD_item_t *MDItem, uint64_t startLB,
        uint8_t *mem_buff)
{
    uint8_t *curptr;
    uint32_t item_size, dnLocPkt_size;
    int16_t len_hbids, len_DN;
    int8_t i;

    item_size = 0;
    curptr = mem_buff;

    DMS_MEM_READ_NEXT(curptr, &len_hbids, sizeof(int16_t));
    len_hbids = ntohs(len_hbids);
    item_size += sizeof(int16_t);

    DMS_MEM_READ_NEXT(curptr, (uint8_t *)(MDItem->arr_HBID), sizeof(uint64_t)*len_hbids);
    for (i = 0; i < len_hbids; i++) {
        MDItem->arr_HBID[i] = ntohll(MDItem->arr_HBID[i]);
    }
    item_size += sizeof(uint64_t)*len_hbids;

    DMS_MEM_READ_NEXT(curptr, &len_DN, sizeof(int16_t));
    len_DN = ntohs(len_DN);
    item_size += sizeof(int16_t);

    //NOTE NNAck_DNLoc_ntoh_order need num_dnLoc of MDItem
    MData_init_MDItem(MDItem, startLB, len_hbids, len_DN, MData_INVALID, 0, 0, false);

    dnLocPkt_size = NNAck_DNLoc_ntoh_order(MDItem, curptr);
    curptr += dnLocPkt_size;
    item_size += dnLocPkt_size;

    /*
     * lego: receive located request state (state = 0 means ok, state = 1
     * means HBIDs not exist : MData_VALID, MData_INVALID
     */
    DMS_MEM_READ_NEXT(curptr, &MDItem->mdata_stat, sizeof(int16_t));
    MDItem->mdata_stat = ntohs(MDItem->mdata_stat);
    item_size += sizeof(int16_t);

    return item_size;
}

/**
 * NNProto_getMDResp_FlowCtrl_ntoh - change byte order of flow control value in response
 *                          from network order to host order
 * @fcItem: An flow control packet data structure to store flow control value
 * @mem_buff: memory byte buffer that hold metadata
 */
void NNProto_getMDResp_FlowCtrl_ntoh (CN2NN_getMDResp_fcpkt_t *fcItem, uint8_t *mem_buff)
{
    uint8_t *curptr;

    curptr = mem_buff;
    DMS_MEM_READ_NEXT(curptr, &fcItem->rate_SlowStart, sizeof(int32_t));
    fcItem->rate_SlowStart = ntohl(fcItem->rate_SlowStart);
    DMS_MEM_READ_NEXT(curptr, &fcItem->rate_SlowStartMaxReqs, sizeof(int32_t));
    fcItem->rate_SlowStartMaxReqs = ntohl(fcItem->rate_SlowStartMaxReqs);
    DMS_MEM_READ_NEXT(curptr, &fcItem->rate_Alloc, sizeof(int32_t));
    fcItem->rate_Alloc = ntohl(fcItem->rate_Alloc);
    DMS_MEM_READ_NEXT(curptr, &fcItem->rate_AllocMaxReqs, sizeof(int32_t));
    fcItem->rate_AllocMaxReqs = ntohl(fcItem->rate_AllocMaxReqs);
    DMS_MEM_READ_NEXT(curptr, &fcItem->rate_AgeTime, sizeof(int32_t));
    fcItem->rate_AgeTime = ntohl(fcItem->rate_AgeTime);
}

/**
 * pack_failLHO_pkt - put metadata with read DN and DN return disk error to packet buffer
 *                    (LHO: lbid-hbid-physical location - user namespace - namenode namespace - physical storage namespace)
 * @mdItem: metadata item that contains failLHO information
 * @num_failDN: this function will fill number of fail DN in num_failDN for returning to caller
 * @msg_buf: memory buffer to hold failLHO in packet format
 *
 * Return: number of bytes for sending to namenode
 *
 * TODO: break big function into smaller
 */
static int32_t pack_failLHO_pkt (metaD_item_t *mdItem, int32_t *num_failDN,
        int8_t *msg_buf)
{
    metaD_DNLoc_t *dnLoc;
    int32_t pkt_size, port, num_of_triplet, i, j;
    int8_t *local_ptr;
    uint64_t triplet, startLB, hbid;
    uint32_t LB_cnt, num_of_dn;

    local_ptr = msg_buf;
    //Lego: skip the msg_buf start location reserving for header
    //LB start + LB cnt + HBIDs + num of dn
    pkt_size = sizeof(uint64_t) + sizeof(uint32_t) +
            sizeof(uint64_t) * ((uint32_t)mdItem->num_hbids) + sizeof(uint32_t);
    local_ptr = local_ptr + pkt_size;

    for (i = 0; i < mdItem->num_dnLoc; i++) {
        do {
#ifdef DISCO_ERC_SUPPORT
            if (MData_chk_ERCProt(mdItem)) {
                dnLoc = &(mdItem->erc_dnLoc[i]);
				break;
            }
#endif
            dnLoc = &(mdItem->udata_dnLoc[i]);
        } while (0);

        if (MData_chk_UDStatus_readfail(dnLoc) == false) {
            continue;
        }

        ccma_inet_aton(dnLoc->ipaddr, local_ptr, DN_IPADDR_NAME_LEN);
        local_ptr = local_ptr + DN_IPADDR_NAME_LEN;
        pkt_size = pkt_size + DN_IPADDR_NAME_LEN;

        port = htonl(dnLoc->port);
        memcpy(local_ptr, &port, sizeof(uint32_t));
        local_ptr = local_ptr + sizeof(uint32_t);
        pkt_size = pkt_size + sizeof(uint32_t);

        num_of_triplet = htonl((uint32_t)dnLoc->num_phyLocs);
        memcpy(local_ptr, &num_of_triplet, sizeof(uint32_t));
        local_ptr = local_ptr + sizeof(uint32_t);
        pkt_size = pkt_size + sizeof(uint32_t);

        for (j = 0; j < dnLoc->num_phyLocs; j++) {
            triplet = htonll(dnLoc->rbid_len_offset[j]);
            memcpy(local_ptr, &triplet, sizeof(uint64_t));
            local_ptr = local_ptr + sizeof(uint64_t);
            pkt_size = pkt_size + sizeof(uint64_t);
        }

        *num_failDN = *num_failDN + 1;
    }

    if (*num_failDN > 0) {
        local_ptr = msg_buf;

        startLB = htonll(mdItem->lbid_s);
        memcpy(local_ptr, &startLB, sizeof(uint64_t));
        local_ptr = local_ptr + sizeof(uint64_t);

        LB_cnt = htonl((uint32_t)mdItem->num_hbids);
        memcpy(local_ptr, &LB_cnt, sizeof(uint32_t));
        local_ptr = local_ptr + sizeof(uint32_t);

        for (i = 0; i < (uint32_t)mdItem->num_hbids; i++) {
            hbid = htonll(mdItem->arr_HBID[i]);
            memcpy(local_ptr, &hbid, sizeof(uint64_t));
            local_ptr = local_ptr + sizeof(uint64_t);
        }

        num_of_dn = htonl(*num_failDN);
        memcpy(local_ptr, &num_of_dn, sizeof(uint32_t));
        local_ptr = local_ptr + sizeof(uint32_t);
    } else {
        pkt_size = 0;
    }

    return pkt_size;
}

/**
 * pack_MData_failLHO - traversal all metadata item in metadata and
 *                         pack fail one to memory buffer
 * @my_MData: a metadata structure that conatins multiples metadata_items need be check
 *            and pack failure DN information inside metadata_items
 * @num_mdItem: how many metadata item needed be report for caller
 * @msg_buf: memory buffer
 *
 * Return: number of bytes for sending to namenode
 */
static int32_t pack_MData_failLHO (metadata_t *my_MData, int32_t *num_mdItem,
        int8_t *msg_buf)
{
    int32_t num_lrs, i, numFailDN, pkt_size, ret_size;
    int8_t *local_ptr;

    if (unlikely(IS_ERR_OR_NULL(my_MData->array_metadata))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null metadata info when gen fail loc pkt\n");
        DMS_WARN_ON(true);
        return 0;
    }

    pkt_size = 0;
    local_ptr = msg_buf;
    *num_mdItem = 0;

    num_lrs = my_MData->num_MD_items;

    for (i = 0; i < num_lrs; i++) {
        numFailDN = 0;
        ret_size = pack_failLHO_pkt(my_MData->array_metadata[i], &numFailDN, local_ptr);
        if (numFailDN > 0) {
            pkt_size = pkt_size + ret_size;
            local_ptr = local_ptr + ret_size;
            *num_mdItem = *num_mdItem + 1;
        }
    }

    return pkt_size;
}

/**
 * NNProto_gen_ciMDReq_ReadIO_pkt - generate commit metadata request for read IO
 * @volumeID: which volume this metadata report request belong to
 * @nnReqID: generated by clientnode, namenode has to reply this for clientnode find corresponding data structure
 * @myMData: metadata that contains DN information that needed report to namenode
 * @msg_buf: memory buffer to hold packet
 *
 * Return: number of bytes for sending to namenode
 */
int32_t NNProto_gen_ciMDReq_ReadIO_pkt (uint32_t volumeID, uint32_t nnReqID,
        metadata_t *myMData, int8_t *msg_buf)
{
    CN2NN_ciMDReq_hder_t nn_ci_hder;
    uint64_t vol64, reqID64;
    int32_t ret_size, num_mdItem, pkt_size;

    num_mdItem = 0;
    pkt_size = sizeof(CN2NN_ciMDReq_hder_t) - sizeof(uint64_t);
    ret_size = pack_MData_failLHO(myMData, &num_mdItem, msg_buf + pkt_size);

    if (num_mdItem > 0) {
        vol64 = (uint64_t)volumeID;
        reqID64 = (uint64_t)nnReqID;

        nn_ci_hder.magic_number1 = mnum_net_order;
        nn_ci_hder.opcode = htons((uint16_t)REQ_NN_REPORT_READIO_RESULT);
        nn_ci_hder.volumeID = htonll(vol64);
        nn_ci_hder.request_id = htonll(reqID64);
        nn_ci_hder.numOfReportItems = htonl(num_mdItem);
        nn_ci_hder.magic_number2 = nn_ci_hder.magic_number1;

        memcpy(msg_buf, (int8_t *)(&nn_ci_hder), pkt_size);
        memcpy((int8_t *)(msg_buf + pkt_size + ret_size),
               (int8_t *) & (nn_ci_hder.magic_number2), sizeof(uint64_t));
        pkt_size = pkt_size + ret_size + sizeof(uint64_t);
    } else {
        pkt_size = 0;
    }

    return pkt_size;
}

/**
 * pack_SuccessLHO_pkt - put write success Logical location-HBID-Physical location to memory buffer
 *                       for sending commit information to namenode
 * @my_MData: A metadata item array that contains metadata information for function to check and
 *            generate packet from it
 * @num_MDItem: how many metadata item needed be report for caller
 * @msg_buf: memory buffer
 *
 * Return: number of bytes for sending to namenode
 */
static int32_t pack_SuccessLHO_pkt (metadata_t *my_MData, int32_t *num_MDItem,
        int8_t *msg_buf)
{
    metaD_item_t *MD_item;
    metaD_DNLoc_t *dnloc;
    int32_t num_lrs, i, j;
    int32_t k, pkt_size, port, numOfDNs, num_of_triplet;
    uint64_t LB_start, hbid, triplet;
    uint32_t LB_cnt;
    int8_t *ptr_for_num_of_dns, *local_ptr;

    local_ptr = msg_buf;
    *num_MDItem = 0;
    pkt_size = 0;
    num_of_triplet = 0;

    num_lrs = my_MData->num_MD_items;

    for (i = 0; i < num_lrs; i++) {
        MD_item = my_MData->array_metadata[i];
        LB_start = htonll(MD_item->lbid_s);
        memcpy(local_ptr, &LB_start, sizeof(uint64_t));
        local_ptr += sizeof(uint64_t);
        pkt_size += sizeof(uint64_t);

        LB_cnt = htonl((uint32_t)MD_item->num_hbids);
        memcpy(local_ptr, &LB_cnt, sizeof(uint32_t));
        local_ptr += sizeof(uint32_t);
        pkt_size += sizeof(uint32_t);

        for (j = 0; j < (uint32_t)MD_item->num_hbids; j++) {
            hbid = htonll(MD_item->arr_HBID[j]);
            memcpy(local_ptr, &hbid, sizeof(uint64_t));
            local_ptr += sizeof(uint64_t);
            pkt_size += sizeof(uint64_t);
        }

        ptr_for_num_of_dns = local_ptr;
        local_ptr += sizeof(uint32_t);
        pkt_size += sizeof(uint32_t);
        numOfDNs = 0;

        for (j = 0; j < MD_item->num_dnLoc; j++) {
            /*
             * TODO: Some lr's replica maybe equals to replica of volume and
             * all success, should we report??
             */
            dnloc = &(MD_item->udata_dnLoc[j]);
            if (MData_chk_UDStatus_writefail(dnloc)) {
                continue;
            }

            numOfDNs++;
            ccma_inet_aton(dnloc->ipaddr, local_ptr, DN_IPADDR_NAME_LEN);
            local_ptr += DN_IPADDR_NAME_LEN;
            pkt_size += DN_IPADDR_NAME_LEN;

            port = htonl(dnloc->port);
            memcpy(local_ptr, &port, sizeof(uint32_t));
            local_ptr += sizeof(uint32_t);
            pkt_size += sizeof(uint32_t);

            num_of_triplet = htonl(dnloc->num_phyLocs);
            memcpy(local_ptr, &num_of_triplet, sizeof(uint32_t));
            local_ptr += sizeof(uint32_t);
            pkt_size += sizeof(uint32_t);

            for (k = 0; k < dnloc->num_phyLocs; k++) {
                triplet = htonll(dnloc->rbid_len_offset[k]);
                memcpy(local_ptr, &triplet, sizeof(uint64_t));
                local_ptr += sizeof(uint64_t);
                pkt_size += sizeof(uint64_t);
            }
        }

        numOfDNs = htonl(numOfDNs);
        memcpy(ptr_for_num_of_dns, &numOfDNs, sizeof(uint32_t));
        *num_MDItem = *num_MDItem + 1;
    }

    return pkt_size;
}

/**
 * NNProto_gen_ciMDReq_WriteIO_pkt - generate commit metadata request for write IO
 * @volumeID: which volume this metadata report request belong to
 * @nnReqID: generated by clientnode, namenode has to reply this for clientnode find corresponding data structure
 * @myMData: metadata that contains DN information that needed report to namenode
 * @msg_buf: memory buffer to hold packet
 *
 * Return: number of bytes for sending to namenode
 */
int32_t NNProto_gen_ciMDReq_WriteIO_pkt (uint32_t volumeID, uint32_t nnReqID,
        metadata_t *myMData, int8_t *msg_buf)
{
    CN2NN_ciMDReq_hder_t nn_cheader;
    uint64_t volID64, nnReqID64;
    int32_t pkt_size, numOfSuccessLHOs, ret_size;

    numOfSuccessLHOs = 0;
    pkt_size = sizeof(CN2NN_ciMDReq_hder_t) - sizeof(uint64_t);
    ret_size = pack_SuccessLHO_pkt(myMData, &numOfSuccessLHOs,
            msg_buf + pkt_size);

    nn_cheader.magic_number1 = htonll(MAGIC_NUM);

    nn_cheader.opcode = htons((uint16_t)REQ_NN_REPORT_WRITEIO_RESULT);

    volID64 = (uint64_t)volumeID;
    nnReqID64 = (uint64_t)nnReqID;
    nn_cheader.volumeID = htonll(volID64);
    nn_cheader.request_id = htonll(nnReqID64);
    nn_cheader.numOfReportItems = htonl(numOfSuccessLHOs);
    nn_cheader.magic_number2 = nn_cheader.magic_number1;

    memcpy(msg_buf, (int8_t *)(&nn_cheader), pkt_size);
    memcpy((int8_t *)(msg_buf + pkt_size + ret_size),
           (int8_t *)(&nn_cheader.magic_number2), sizeof(uint64_t));

    pkt_size = pkt_size + ret_size + sizeof(uint64_t);

    return pkt_size;
}

/*
 * print phys_loc
 */
static void print_phys_loc (int32_t log_level, phys_loc_t *loc)
{
    if (log_level <  discoC_log_lvl) {
        return;
    }

    dms_printk(log_level, "COMMIT_TO_NN: hostname = %s \n", loc->hostname);

    dms_printk(log_level, "COMMIT_TO_NN: port = %d \n", loc->port);

    dms_printk(log_level, "COMMIT_TO_NN: RBID = %d \n", loc->RBID);

    dms_printk(log_level, "COMMIT_TO_NN: offset_in_rb = %d \n", loc->off_in_rb);

    dms_printk(log_level, "COMMIT_TO_NN: len = %d \n", loc->len);
}

/*
 * change_DNLoc_HostByteOrder: change the byte order in net_DNLocation from network
 *                             to host order and copy to new location
 * @net_DNLocation: struct FailedDatanodeLocation *failedDN
 *         data received from network
 * @dnLoc: struct FailedDatanodeLocation *tmp
 *         container for translated data
 */
static int32_t change_DNLoc_HostByteOrder (CN2NN_ciMDReq_dnLoc_pkt_t *net_DNLocation,
        CN2NN_ciMDReq_dnLoc_pkt_t *dnLoc)
{
    int32_t size = 0;

    memcpy(&dnLoc->ipv4addr, &net_DNLocation->ipv4addr, IPV4ADDR_NM_LEN);
    size += sizeof(dnLoc->ipv4addr);

    dnLoc->port = ntohl(net_DNLocation->port);
    size += sizeof(dnLoc->port);

    dnLoc->numOfPhyLocs = ntohl(net_DNLocation->numOfPhyLocs);
    size += sizeof(dnLoc->numOfPhyLocs);

    return size;
}

/*
 * print the FailedDatanodeLocation from a given network buffer
 */
static int32_t print_CommitDNLoc_in_pkt (int32_t log_level, int8_t *buf)
{
    CN2NN_ciMDReq_dnLoc_pkt_t dnLoc;
    int32_t numOfLocation, i, size;
    uint64_t triple;
    phys_loc_t phyloc;

    if (log_level < discoC_log_lvl) {
        return 0;
    }

    if (IS_ERR_OR_NULL(buf)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN print commit info from null buffer\n");
        DMS_WARN_ON(true);
        return 0;
    }

    size = 0;
    memset(&dnLoc, 0, sizeof(CN2NN_ciMDReq_dnLoc_pkt_t));
    size += change_DNLoc_HostByteOrder((CN2NN_ciMDReq_dnLoc_pkt_t *)buf, &dnLoc);
    numOfLocation = dnLoc.numOfPhyLocs;

    for (i = 0; i < numOfLocation; i++) {
        triple = ntohll(*((uint64_t *)(buf + size)));
        dms_printk(log_level, "PARTIAL_COMMIT: rbid_len_offset = %llu\n", triple);
        memset(&phyloc, 0, sizeof(phys_loc_t));
        decompose_triple(triple, &phyloc.RBID, &phyloc.len, &phyloc.off_in_rb);

        memcpy(&phyloc.hostname, &dnLoc.ipv4addr, IPV4ADDR_NM_LEN);
        phyloc.port = dnLoc.port;

        print_phys_loc(log_level, &phyloc);

        size += sizeof(uint64_t);
    }

    return size;
}

/*
 * print out all of the msg in the buffer, of course we have to translate
 * the data from Big endian to host
 */
void NNProto_show_ciMDReq_WriteIO_pkt (int32_t log_level, int8_t *msg_buf)
{
    uint64_t magicNum1, magicNum2, volid, lbid_start, hbid, req_id;
    int32_t index, numOfLHOs, numOfDNs, i, lbid_cnt, j;
    uint16_t rw;

    if (log_level < discoC_log_lvl) {
        return;
    }

    if (IS_ERR_OR_NULL(msg_buf)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN print write commit info from null buffer\n");
        DMS_WARN_ON(true);
        return;
    }

    index = 0;
    magicNum1 = ntohll(*((uint64_t *)(msg_buf + index)));
    dms_printk(log_level, "magic_number1 = %llu \n", magicNum1);
    index += sizeof(uint64_t);

    rw = ntohs(*((uint16_t *)(msg_buf + index)));
    dms_printk(log_level, "rw = %u \n", rw);
    index += sizeof(uint16_t);

    volid = ntohll(*((uint64_t *)(msg_buf + index)));
    dms_printk(log_level, "volume id = %llu \n", volid);
    index += sizeof(uint64_t);

    req_id = ntohll(*((uint64_t *)(msg_buf + index)));
    dms_printk(log_level, "request id = %llu \n", req_id);
    index += sizeof(uint64_t);

    numOfLHOs = ntohl(*((int32_t *)(msg_buf + index)));
    dms_printk(log_level, "numOfsuccessLHOs = %d \n", numOfLHOs);
    index += sizeof(int32_t); //sizeof(numOfLHOs);

    for (i = 0; i < numOfLHOs; i++) {
        lbid_start = ntohll(*((uint64_t *)(msg_buf + index)));
        dms_printk(log_level, "LHO[%d]: start LB=%llu\n", i, lbid_start);
        index += sizeof(uint64_t);

        lbid_cnt = ntohl(*((int32_t *)(msg_buf + index)));
        dms_printk(log_level, "LHO[%d]: LB cnt=%d\n", i, lbid_cnt);
        index += sizeof(lbid_cnt);

        dms_printk(log_level, "LHO[%d]: HBIDs =", i);

        for (j = 0; j < lbid_cnt; j++) {
            hbid = ntohll(*((uint64_t *)(msg_buf + index)));
            dms_printk(log_level, " %llu", hbid);
            index += sizeof(uint64_t);
        }

        dms_printk(log_level, "\n");

        numOfDNs = ntohl(*((int32_t *)(msg_buf + index)));
        dms_printk(log_level, "LHO[%d]: numOfsuccessDNs = %d \n",
                   i, numOfDNs);
        index += sizeof(int32_t); //sizeof(numOfDNs);

        for (j = 0; j < numOfDNs; j++) {
            dms_printk(log_level, "LHO[%d].DNs[%d]:\n", i, j);
            index += print_CommitDNLoc_in_pkt(log_level, msg_buf + index);
        }
    }

    magicNum2 = ntohll(*((uint64_t *)(msg_buf + index)));
    dms_printk(log_level, "magic_number2 = %llu \n", magicNum2);
    index += sizeof(uint64_t);

    dms_printk(log_level, "end of print, the total size = %d \n", index);
}

#ifdef DISCO_PREALLOC_SUPPORT
int32_t fsReq_gen_allocSpace_header (int8_t *pkt,
        uint64_t reqID, uint16_t opcode, uint64_t volID, uint32_t minSize)
{
    CN2NN_fsAllocMD_hder_t *pkt_hder;

    pkt_hder = (CN2NN_fsAllocMD_hder_t *)pkt;

    pkt_hder->magic_number1 = htonll(MAGIC_NUM);
    pkt_hder->opcode = htons(opcode);
    pkt_hder->request_id = htonll(reqID);
    pkt_hder->volumeID = htonll(volID);
    pkt_hder->min_space_size = htonl(minSize);
    pkt_hder->magic_number2 = htonll(MAGIC_NUM);

    return (int32_t)sizeof(CN2NN_fsAllocMD_hder_t);
}

/*
 * NOTE: only handle 1 rbid len offset
 */
int32_t fsReq_gen_reportMD_DNLoc (int8_t *pkt, uint32_t ipaddr, uint32_t port,
        uint32_t rbid, uint32_t offset)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN], *ptr;
    uint32_t pkt_size;
    uint16_t dnIPLen, dnIPLen_norder;

    ptr = pkt;
    pkt_size = 0;
    ccma_inet_aton(ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);

    dnIPLen = strlen(ipv4addr_str);
    dnIPLen_norder = htons(dnIPLen);
    memcpy(ptr, &dnIPLen_norder, sizeof(uint16_t));
    pkt_size += sizeof(uint16_t);
    ptr += sizeof(uint16_t);

    memcpy(ptr, ipv4addr_str, dnIPLen);
    pkt_size += dnIPLen;
    ptr += dnIPLen;

    port = htonl(port);
    memcpy(ptr, &port, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    rbid = htonl(rbid);
    memcpy(ptr, &rbid, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    offset = htonl(offset);
    memcpy(ptr, &offset, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    ptr += sizeof(uint32_t);

    return pkt_size;
}

int32_t fsReq_gen_reportMD_ChunkInfo (int8_t *pkt, uint64_t chunkID,
        uint32_t chunkLen, uint64_t lbid_s, uint64_t hbid_s, uint16_t numReplica)
{
    CN2NN_fsReportMD_chunk_t *pkt_chunk;

    pkt_chunk = (CN2NN_fsReportMD_chunk_t *)pkt;
    pkt_chunk->chunkID = htonll(chunkID);
    pkt_chunk->length = htonl(chunkLen);
    pkt_chunk->lbid_s = htonll(lbid_s);
    pkt_chunk->hbid_s = htonll(hbid_s);
    pkt_chunk->numReplica = htons(numReplica);

    return (int32_t)sizeof(CN2NN_fsReportMD_chunk_t);
}

int32_t fsReq_gen_reportMD_header (int8_t *pkt_buff, uint16_t opcode,
        uint64_t reqID, uint64_t volID, uint32_t numItems)
{
    CN2NN_fsReportMD_hder_t *pkt_hder;

    pkt_hder = (CN2NN_fsReportMD_hder_t *)pkt_buff;

    pkt_hder->magic_number1 = htonll(MAGIC_NUM);
    pkt_hder->opcode = htons(opcode);
    pkt_hder->request_id = htonll(reqID);
    pkt_hder->volumeID = htonll(volID);
    pkt_hder->numOfReportItems = htonl(numItems);
    pkt_hder->magic_number2 = htonll(MAGIC_NUM);

    return (int32_t)sizeof(CN2NN_fsReportMD_hder_t);
}

int32_t fsReq_gen_reportFS_header (int8_t *pkt_buff, uint16_t opcode,
        uint64_t reqID, uint64_t volID, uint32_t chunkID, uint64_t hbid_s)
{
    CN2NN_fsReportFS_hder_t *pkt_hder;

    pkt_hder = (CN2NN_fsReportFS_hder_t *)pkt_buff;

    pkt_hder->magic_number1 = htonll(MAGIC_NUM);
    pkt_hder->opcode = htons(opcode);
    pkt_hder->request_id = htonll(reqID);
    pkt_hder->volumeID = htonll(volID);
    pkt_hder->chunkID = htonll(chunkID);
    pkt_hder->hbid_s = htonll(hbid_s);
    pkt_hder->magic_number2 = htonll(MAGIC_NUM);

    return sizeof(CN2NN_fsReportFS_hder_t);
}

int32_t fsResp_parse_prealloc_ack_chkHder (int8_t *pkt_buff, uint64_t *reqID,
        uint32_t *num_chunk)
{
    int32_t pkt_size;

    pkt_size = 0;
    memcpy(reqID, pkt_buff + pkt_size, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    *reqID = ntohll(*reqID);

    memcpy(num_chunk, pkt_buff + pkt_size, sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);
    *num_chunk = ntohl(*num_chunk);

    return pkt_size;
}

int32_t fsResp_parse_prealloc_ack_chunk (int8_t *pkt_buff,
        NN2CN_fsAllocMDResp_chunk_t *chunkInfo)
{
    memcpy((int8_t *)chunkInfo, pkt_buff, sizeof(NN2CN_fsAllocMDResp_chunk_t));

    chunkInfo->chunkID = ntohll(chunkInfo->chunkID);
    chunkInfo->chunkLen = ntohl(chunkInfo->chunkLen);
    chunkInfo->hbid_s = ntohll(chunkInfo->hbid_s);
    chunkInfo->numReplica = ntohs(chunkInfo->numReplica);

    return sizeof(NN2CN_fsAllocMDResp_chunk_t);
}

int32_t fsResp_parse_prealloc_ack_DNLoc (int8_t *pkt_buff,
        NN2CN_fsReq_DNLoc_t *dnInfo)
{
    int32_t pktSize;

    pktSize = 0;
    memcpy(&dnInfo->ipv4addr_strlen, pkt_buff + pktSize, sizeof(uint16_t));
    dnInfo->ipv4addr_strlen = ntohs(dnInfo->ipv4addr_strlen);
    pktSize += sizeof(uint16_t);

    memset(&dnInfo->ipv4addr_str, '\0', IPV4ADDR_NM_LEN);
    memcpy(&dnInfo->ipv4addr_str, pkt_buff + pktSize, dnInfo->ipv4addr_strlen);
    pktSize += dnInfo->ipv4addr_strlen;

    dnInfo->ipv4addr = ccma_inet_ntoa(dnInfo->ipv4addr_str);
    memcpy(&dnInfo->port, pkt_buff + pktSize, sizeof(uint32_t));
    dnInfo->port = ntohl(dnInfo->port);
    pktSize += sizeof(uint32_t);

    memcpy(&dnInfo->rbid, pkt_buff + pktSize, sizeof(uint32_t));
    dnInfo->rbid = ntohl(dnInfo->rbid);
    pktSize += sizeof(uint32_t);

    memcpy(&dnInfo->offset, pkt_buff + pktSize, sizeof(uint32_t));
    dnInfo->offset = ntohl(dnInfo->offset);
    pktSize += sizeof(uint32_t);

    return pktSize;
}


int32_t fsResp_parse_reportFS_ack (int8_t *pkt_buff, uint64_t *reqID,
        uint16_t *result)
{
    int32_t pkt_size;

    pkt_size = 0;
    memcpy(reqID, pkt_buff + pkt_size, sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);
    *reqID = ntohll(*reqID);

    memcpy(result, pkt_buff + pkt_size, sizeof(uint16_t));
    pkt_size += sizeof(uint16_t);
    *result = ntohs(*result);

    return pkt_size;
}
#endif
