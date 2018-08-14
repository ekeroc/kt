/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DN_protocol.c
 *
 * This component aim for paring CN - DN protocol
 *
 */
#include <linux/rwsem.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../config/dmsc_config.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_Manager.h"
#include "discoC_DNC_workerfun.h"
#include "discoC_DN_protocol.h"
#include "fingerprint.h"

#if DN_PROTOCOL_VERSION == 1
/*
 * _gen_DNReq_header: packet given information into byte array for CN-DN request
 * @pkt_buffer  : byte array to fill information
 * @opcode      : operation code (discoC_DNC_Manager.h: DNUDReq_opcode_t)
 * @num_hbids   : how many 4K data of DNRequest need took care
 * @num_triplets: how many continuous datanode location to R/W user data
 * @nnreq_id    : DNRequest's caller ID
 * @dnreq_id    : DNRequest ID
 *
 * Return: size in bytes to indicate how many data to sent
 */
static int32_t _gen_DNReq_header (int8_t *pkt_buffer,
        uint8_t opcode, uint16_t num_hbids, uint16_t num_triplets,
        uint32_t nnreq_id, uint32_t dnreq_id)
{
    CN2DN_UDRWReq_hder_t *reqpkt;

    reqpkt = (CN2DN_UDRWReq_hder_t *)pkt_buffer;
    reqpkt->magic_num1 = PKT_MAGIC_NUM_NORDER;
    reqpkt->opcode = opcode;
    reqpkt->num_hbids = htons(num_hbids);
    reqpkt->num_triplets = htons(num_triplets);
    reqpkt->nnreq_id = htonl(nnreq_id);
    reqpkt->dnreq_id = htonl(dnreq_id);
    reqpkt->magic_num2 = PKT_MAGIC_NUM_NORDER;

    return sizeof(CN2DN_UDRWReq_hder_t);
}

/*
 * _fill_u64_array_pkt: put an 8 bytes array into byte array
 * @pkt_buffer  : byte array to fill information
 * @arr_u64     : source of 8 bytes array
 * @arr_size    : how many 8 bytes
 *
 * Return: size in bytes to indicate how many data to sent
 */
static uint32_t _fill_u64_array_pkt (int8_t *pkt_buffer,
        uint64_t *arr_u64, uint16_t arr_size)
{
    uint64_t *ptr_u64;
    uint32_t i;

    ptr_u64 = (uint64_t *)pkt_buffer;

    for (i = 0; i < arr_size; i++) {
        *ptr_u64 = htonll(arr_u64[i]);
        ptr_u64++;
    }

    return (sizeof(uint64_t)*arr_size);
}

/*
 * fill_DNReq_HBID_pkt: put hbid array into byte array
 * @pkt_buffer  : byte array to fill information
 * @arr_hbids   : hbid array
 * @arr_size    : how many hbids
 *
 * Return: size in bytes to indicate how many data to sent
 */
static uint32_t fill_DNReq_HBID_pkt (int8_t *pkt_buffer, uint64_t *arr_hbids,
        uint16_t arr_size)
{
    return _fill_u64_array_pkt(pkt_buffer, arr_hbids, arr_size);
}

/*
 * fill_DNReq_phyLoc_pkt: put physical location array into byte array
 * @pkt_buffer  : byte array to fill information
 * @arr_phyloc  : physical location rlo array
 * @arr_size    : how many hbids
 *
 * Return: size in bytes to indicate how many data to sent
 */
static uint32_t fill_DNReq_phyLoc_pkt (int8_t *pkt_buffer, uint64_t *arr_phyloc,
        uint16_t arr_size)
{
    return _fill_u64_array_pkt(pkt_buffer, arr_phyloc, arr_size);
}

/*
 * fill_DNReq_read_mask_pkt: put read mask array into byte array
 * @num_hbids  : how many 4K data of DNRequest need took care
 * @pkt_buffer  : byte array to fill information
 *
 * Return: size in bytes to indicate how many data to sent
 * TODO: modify DN so that clientnode can avoid mask setting
 */
static uint32_t fill_DNReq_read_mask_pkt (uint16_t num_hbids, int8_t *pkt_buffer)
{
    //int8_t *ptr;
    //uint16_t i;
    /*
     * Lego: When DMS_LB_SIZE is the same as KERNEL_LOGICAL_SECTOR_SIZE,
     * we don't need masks. When opcode ==
     * CN_INTERNAL_USE_FOR_DN_REQUEST_READ, we should set all bit as 1 in
     * mask array dms_printk(LOG_LEVEL_DEBUG, "%s opcode is %d and set
     * mask value as 0xff\n", __func__, internal_op_code);
     */
    //ptr = pkt_buffer;
    //memset(ptr, 0, sizeof(uint8_t) * 8 * num_hbids);
    //for (i = 0; i < num_hbids; i++) {
    //    *ptr = 0x1;
    //    ptr += sizeof(uint8_t) * 8;
    //}

    memcpy(pkt_buffer, DNProto_ReadMask_buffer, sizeof(uint8_t) * 8 * num_hbids);

    return (8 * num_hbids);
}

/*
 * DNProto_gen_ReqPacket: generate packet base on information in DNRequest
 * @dnreq      : source DNRequest that has information for sent
 * @packet_len : how many bytes in packet for returning to user
 * @pkt_buf    : packet buffer for adding data
 */
void DNProto_gen_ReqPacket (DNUData_Req_t *dnreq, int32_t *packet_len, int8_t *pkt_buf)
{
    ReqCommon_t *dnR_comm;
    int8_t *ptr_pkt;
    uint32_t len_pkt;

    ptr_pkt = pkt_buf;
    len_pkt = 0;

    dnR_comm = &dnreq->dnReq_comm;
    len_pkt += _gen_DNReq_header(ptr_pkt + len_pkt, dnR_comm->ReqOPCode,
            dnreq->num_hbids, dnreq->num_phyloc,
            dnR_comm->CallerReqID, dnR_comm->ReqID);

    len_pkt += fill_DNReq_HBID_pkt(ptr_pkt + len_pkt, dnreq->arr_hbids,
            dnreq->num_hbids);

    /*
     * Lego: although we using uint64_t to represent mask arrays, but we should
     * transfer into byte array for communication with dn
     * Masks array : 8 bytes * Num of HBIDs
     */
    switch (dnR_comm->ReqOPCode) {
    case DNUDREQ_WRITE:
    case DNUDREQ_WRITE_NO_PAYLOAD:
    case DNUDREQ_WRITE_NO_IO:
    case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        /*
         * NOTE: We don't calculate fp when copy payload from bio to buffer.
         *       The reason is if we get partial write payload (not align with
         *       DMS LB size), the finger print would be wrong.
         */
        len_pkt += get_fingerprint(dms_client_config->fp_method,
                ptr_pkt + len_pkt, dnreq);
        break;
    case DNUDREQ_READ:
    case DNUDREQ_READ_NO_PAYLOAD:
    case DNUDREQ_READ_NO_IO:
    case DNUDREQ_READ_NO_PAYLOAD_IO:
        /*
         * Lego: When DMS_LB_SIZE is the same as KERNEL_LOGICAL_SECTOR_SIZE,
         * we don't need masks. When opcode ==
         * CN_INTERNAL_USE_FOR_DN_REQUEST_READ, we should set all bit as 1 in
         * mask array dms_printk(LOG_LVL_DEBUG, "%s opcode is %d and set
         * mask value as 0xff\n", __func__, internal_op_code);
         */

        len_pkt += fill_DNReq_read_mask_pkt(dnreq->num_hbids,
                ptr_pkt + len_pkt);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong dn OPCode when gen pkt %d\n",
                dnR_comm->ReqOPCode);
        DMS_WARN_ON(true);
        break;
    }

    len_pkt += fill_DNReq_phyLoc_pkt(ptr_pkt + len_pkt,
            dnreq->phyLoc_rlo, dnreq->num_phyloc);

    *packet_len = len_pkt;
}

#else
static void add_pkt_length (int32_t *pkt_len, uint32_t length)
{
    *pkt_len += length;
}

static void gen_pkt_content (int8_t **pkt, uint64_t content, uint32_t length)
{

    uint64_t hton_content;

    hton_content = 0;
    switch (length) {
    case 1:
        hton_content = (uint8_t)content;
        break;
    case 2:
        hton_content = htons((uint16_t)content);
        break;
    case 4:
        hton_content = htonl((uint32_t)content);
        break;
    case 8:
        hton_content = htonll(content);
        break;
    default:
        return;
    }
    memcpy((*pkt), &hton_content, length);
    (*pkt) = (*pkt) + length;
}


/*
 * pkt     : packet pointer
 * pkt_len : length of packet
 * length  : length of content (byte)
 */
static void
gen_local_pkt_content (int8_t **pkt, uint32_t *pkt_len, uint64_t content,
                       uint32_t len)
{
    gen_pkt_content(pkt, content, len);
    add_pkt_length(pkt_len, len);
}

static void
DNProto_gen_ReqPacket (DNUData_Req_t *dnreq, int32_t *packet_len, int8_t *result)
{
    int8_t *ptr, *ptr_data_size, *ptr_startLB;
    uint16_t NumHBIDs, triple_len, reserved;
    uint32_t i, cont_size, fp_total_len, cnt, rbid, len, offset;
    int16_t opcode;
    uint64_t len_off;
    write_proto_t *new_wp;

    opcode = dnreq->wp.opCode;
    *packet_len = 0;
    ptr = result;
    new_wp = &(dnreq->wp);
    NumHBIDs = new_wp->NumHBIDs;
    triple_len = new_wp->triple_len;

    //Pack magice number 1 : 4 bytes
    gen_local_pkt_content(&ptr, packet_len, DN_MAGIC_S, 4);

    //Pack data size : 4 bytes
    cont_size = 0;
    ptr_data_size = ptr;
    gen_local_pkt_content(&ptr, packet_len, cont_size, 4);

    //Pack opcode : 2 bytes
    gen_local_pkt_content(&ptr, packet_len, new_wp->opCode, 2);

    //Number of HBIDs : 2 bytes
    gen_local_pkt_content(&ptr, packet_len, NumHBIDs, 2);

    //Number of triplets : 2 bytes
    gen_local_pkt_content(&ptr, packet_len, triple_len, 2);

    //reserved: 2 bytes
    reserved = 0;
    gen_local_pkt_content(&ptr, packet_len, reserved, 2);

    //NN Request id : 4 bytes
    gen_local_pkt_content(&ptr, packet_len, dnreq->nn_req->nnreq_id, 4);

    //DN Request id : 4 bytes
    gen_local_pkt_content(&ptr, packet_len, dnreq->dnreq_id, 4);

    switch (opcode) {
    case DNUDREQ_WRITE:
    case DNUDREQ_WRITE_NO_PAYLOAD:
    case DNUDREQ_WRITE_NO_IO:
    case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        // volume id : 8 bytes
        gen_local_pkt_content(&ptr, packet_len, dnreq->nn_req->vol_id, 8);
        // timestamp : 8 bytes
#if (CN_CRASH_RECOVERY == 1)
        gen_local_pkt_content(&ptr, packet_len, dnreq->nn_req->ts_dn_logging, 8);
#else
        gen_local_pkt_content(&ptr, packet_len, 0, 8);
#endif
        cont_size += 16;

        ptr_startLB = ptr;
        cont_size += (triple_len << 3);
        ptr += (triple_len << 3);
        break;
    default:
        break;
    }

    //HBIDs array   : 8 bytes * Num of HBIDs
    for (i = 0; i < NumHBIDs; i++) {
        gen_pkt_content(&ptr, new_wp->HBIDS[i], 8);
    }
    add_pkt_length(packet_len, 8 * NumHBIDs);
    cont_size += 8*NumHBIDs;

    /*
     * Lego: although we using uint64_t to represent mask arrays, but we should
     * transfer into byte array for communication with dn
     * Masks array : 8 bytes * Num of HBIDs
     */
    switch (opcode) {

    case DNUDREQ_WRITE:
    case DNUDREQ_WRITE_NO_PAYLOAD:
    case DNUDREQ_WRITE_NO_IO:
    case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        /*
         * NOTE: We don't calculate fp when copy payload from bio to buffer.
         *       The reason is if we get partial write payload (not align with
         *       DMS LB size), the finger print would be wrong.
         */
        fp_total_len = get_fingerprint(dms_client_config->fp_method, ptr, dnreq);
        add_pkt_length(packet_len, fp_total_len);
        ptr += sizeof(uint8_t) * fp_total_len;
        cont_size += fp_total_len;
        break;
    case REQ_DN_REQUEST_READ:
    case DNUDREQ_READNO_PAYLOAD:
    case DNUDREQ_READNO_IO:
    case DNUDREQ_READNO_PAYLOAD_IO:
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN worng dn op code when gen pkt %d\n", opcode);
        DMS_WARN_ON(true);
        break;
    }

    //Triplets array: 8 bytes * Num of triplet
    cnt = 0;
    for (i = 0; i < triple_len; i++) {
        decompose_triple(new_wp->RBID_len_offset[i], &rbid, &len, &offset);
        gen_pkt_content(&ptr, rbid, 8);
        len_off = compose_len_off(len, offset);
        gen_pkt_content(&ptr, len_off, 8);

        switch (opcode) {
        case DNUDREQ_WRITE:
        case DNUDREQ_WRITE_NO_PAYLOAD:
        case DNUDREQ_WRITE_NO_IO:
        case DNUDREQ_WRITE_NO_PAYLOAD_IO:
            // volume id : 8 bytes
            gen_local_pkt_content(&ptr_startLB, packet_len,  dnreq->lbids_lbmd[cnt]->LBID, 8);
            cnt += len;
            break;
        default:
            break;
        }
    }
    add_pkt_length(packet_len, 16*triple_len);
    cont_size += 16*triple_len;

    switch (opcode) {
    case DNUDREQ_WRITE:
    case DNUDREQ_WRITE_NO_PAYLOAD:
    case DNUDREQ_WRITE_NO_IO:
    case DNUDREQ_WRITE_NO_PAYLOAD_IO:
        cont_size += (NumHBIDs << BIT_LEN_DMS_LB_SIZE); //payload size
        cont_size += 4; //magic end
        gen_pkt_content(&ptr_data_size, cont_size, 4);
        break;
    case REQ_DN_REQUEST_READ:
        case DNUDREQ_READNO_PAYLOAD:
        case DNUDREQ_READNO_IO:
        case DNUDREQ_READNO_PAYLOAD_IO:
        cont_size += 4;
        gen_pkt_content(&ptr_data_size, cont_size, 4);
        //Pack magice number 2 : 4 bytes
        gen_local_pkt_content(&ptr, packet_len, DN_MAGIC_E, 4);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN worng dn op code when gen pkt %d\n", opcode);
        DMS_WARN_ON(true);
        break;
    }
}
#endif


#if DN_PROTOCOL_VERSION == 1
/*
 * DNProto_ReqAck_ntoh: translate data in CN2DN_UDRWResp_hder_t from network order to host order
 * @pkt      : ack received from socket
 */
void DNProto_ReqAck_ntoh (CN2DN_UDRWResp_hder_t *pkt)
{
    uint8_t *ptr = (uint8_t *)pkt;
    uint64_t magic_num1 = 0, magic_num2 = 0;
    uint8_t res_type = 0;
    uint8_t num_of_bytes_byte1 = 0;
    uint16_t num_of_bytes_byte2 = 0;
    uint32_t num_of_sector = 0;
    uint32_t cid = 0;
    uint32_t did = 0;

    memcpy(&magic_num1, ptr, sizeof(uint8_t)*8);
    magic_num1 = ntohll(magic_num1);
    ptr = ptr + 8;

    memcpy(&res_type, ptr, sizeof(uint8_t));
    ptr = ptr + 1;

    memcpy(&num_of_bytes_byte1, ptr, sizeof(uint8_t));
    ptr = ptr + 1;

    memcpy(&num_of_bytes_byte2, ptr, sizeof(uint8_t)*2);
    ptr = ptr + 2;

    num_of_sector = ntohs(num_of_bytes_byte2) | (num_of_bytes_byte1 << 16);
    ptr = ptr + 4; //for reserved

    memcpy(&cid, ptr, sizeof(uint8_t)*4);
    ptr = ptr + 4;
    cid = ntohl(cid);

    memcpy(&did, ptr, sizeof(uint8_t)*4);
    ptr = ptr + 4;
    did = ntohl(did);

    memcpy(&magic_num2, ptr, sizeof(uint8_t)*8);
    magic_num2 = ntohll(magic_num2);

    memset(pkt, 0, sizeof(CN2DN_UDRWResp_hder_t));
    pkt->magic_num1 = magic_num1;
    pkt->magic_num2 = magic_num2;
    pkt->responseType = res_type;
    pkt->pReqID = cid;
    pkt->dnid = did;
    pkt->num_of_lb = num_of_sector;
}
#else
static void DNProto_ReqAck_ntoh (CN2DN_UDRWResp_hder_t *pkt)
{
    uint8_t *ptr;
    uint32_t magic_num1, magic_num2;
    uint32_t cid, did;
    uint16_t res_type, num_HBs;

    ptr = (uint8_t *)pkt;

    memcpy(&magic_num1, ptr, sizeof(uint8_t)*4);
    magic_num1 = ntohl(magic_num1);
    ptr = ptr + 4;

    memcpy(&res_type, ptr, sizeof(uint8_t)*2);
    ptr = ptr + 2;
    res_type = ntohs(res_type);

    memcpy(&num_HBs, ptr, sizeof(uint8_t)*2);
    ptr = ptr + 2;
    num_HBs = ntohs(num_HBs);

    ptr += 4; //reserved

    memcpy(&cid, ptr, sizeof(uint8_t)*4);
    ptr = ptr + 4;
    cid = ntohl(cid);

    memcpy(&did, ptr, sizeof(uint8_t)*4);
    ptr = ptr + 4;
    did = ntohl(did);

    magic_num2 = 0;

    memset(pkt, 0, sizeof(CN2DN_UDRWResp_hder_t));
    pkt->magic_num1 = magic_num1;
    pkt->magic_num2 = magic_num2;
    pkt->responseType = res_type;
    pkt->num_of_lb = num_HBs;
    pkt->pReqID = cid;
    pkt->dnid = did;
}
#endif

#if (DN_PROTOCOL_VERSION == 2)
#define HB_PACKET_SIZE_DN   28
#if (CN_CRASH_RECOVERY == 1)
#define CL_PACKET_SIZE_DN   36
static int8_t clog_buffer[CL_PACKET_SIZE_DN];
static int8_t *ptr_clog_ts;
#endif

static void init_hb_data (int8_t *hb_buffer_dn)
{
    uint8_t tmp_ptr_char;
    uint32_t tmp_ptr_l;
    int8_t *ptr;

    //Generate packet for dn socket
    ptr = hb_buffer_dn;

    tmp_ptr_l = htonl(DN_MAGIC_S);
    memcpy(ptr, &tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;

    tmp_ptr_l = htonl(4);
    memcpy(ptr, &tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;

    tmp_ptr_s = htons((uint16_t)DNUDREQ_HEART_BEAT);
    memcpy(ptr, &tmp_ptr_s, SIZEOF_U16);
    ptr += SIZEOF_U16;
    ptr += 14; //Reserved bytes

    tmp_ptr_l = htonl(DN_MAGIC_E);
    memcpy(ptr, &tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;
}
#endif

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
static int8_t *init_cleanLog_data (int8_t *cl_buffer)
{
    int8_t *ptr, *ptr_ts;
    uint32_t tmp_ptr_l;
    uint16_t tmp_ptr_s;

    memset(cl_buffer, 0, CL_PACKET_SIZE_DN);
    ptr = cl_buffer;

    tmp_ptr_l = htonl(DN_MAGIC_S);
    memcpy(ptr, &tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;

    tmp_ptr_l = htonl(0); //volume id when clean log is by volume
    memcpy(ptr, &tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;

    tmp_ptr_s = htons((uint16_t)DNUDREQ_CLEAN_LOG);
    memcpy(ptr, &tmp_ptr_s, SIZEOF_U16);
    ptr += SIZEOF_U16;

    tmp_ptr_s = htons(0); //clean mode
    memcpy(ptr, &tmp_ptr_s, SIZEOF_U16);
    ptr += SIZEOF_U16;

    ptr += 12; //Reserved bytes

    ptr_ts = ptr;
    ptr += SIZEOF_U64;

    tmp_ptr_l = htonl(DN_MAGIC_E);
    memcpy(ptr,&tmp_ptr_l, SIZEOF_U32);
    ptr+= SIZEOF_U32;

    return ptr_ts;
}

int8_t *ptr_clog_ts = init_cleanLog_data(clog_buffer);
#endif

/*
 * DNProto_gen_HBPacket: generate packet for heartbeat
 * @pkt_buffer      : packet buffer to be sent
 * @max_buff_size   : max size of pkt_buffer
 *
 * Return: size of packet
 */
int32_t DNProto_gen_HBPacket (int8_t *pkt_buffer, int32_t max_buff_size)
{
    int8_t *ptr;
    uint64_t tmp_ptr_ll;
#if DN_PROTOCOL_VERSION == 1
    int8_t tmp_ptr_char;
#else
    int32_t tmp_ptr_l;
#endif
    int32_t pkt_len;

    if (IS_ERR_OR_NULL(pkt_buffer)) {
        return -EINVAL;
    }

    //Generate hb packet for dn socket
    ptr = pkt_buffer;
    pkt_len = 0;

#if DN_PROTOCOL_VERSION == 1
    tmp_ptr_ll = htonll(MAGIC_NUM);
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

    tmp_ptr_char = (uint8_t)DNUDREQ_HEART_BEAT;
    memcpy(ptr, &tmp_ptr_char, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    pkt_len += sizeof(uint8_t);

    ptr += 15; //Reserved bytes
    pkt_len += 15;

    tmp_ptr_ll = htonll(MAGIC_NUM);
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

#else
    tmp_ptr_l = htonl(DN_MAGIC_S);
    memcpy(ptr, &tmp_ptr_l, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    pkt_len += sizeof(uint32_t);

    tmp_ptr_l = htonl(4);
    memcpy(ptr, &tmp_ptr_l, sizeof(uint32_t));
    ptr+= sizeof(uint32_t);
    pkt_len += sizeof(uint32_t);

    tmp_ptr_s = htons((uint16_t)DNUDREQ_HEART_BEAT);
    memcpy(ptr, &tmp_ptr_s, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint32_t);

    ptr += 14; //Reserved bytes
    pkt_len += 14;

    tmp_ptr_l = htonl(DN_MAGIC_E);
    memcpy(ptr, &tmp_ptr_l, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    pkt_len += sizeof(uint32_t);
#endif

//TODO
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    ptr_clog_ts = init_cleanLog_data(clog_buffer);
#endif

    return pkt_len;
}


