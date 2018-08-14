/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReq_copyData_func.c
 *
 * This c file contains functions that support following:
 * 1. copy data from bio to a memory buffer for write operation
 * 2. copy data from a memory buffer to bio for read operation
 * 3. copy data from a memory buffer to io request memory buffer when read phase
 *    of partial write IO done
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"

/*
 * Caller guarantee ioreq and ioreq->payload not NULL
 */
static int8_t *get_chunk_ptr (data_mem_chunks_t *payload,
        int32_t *cnt, uint32_t *off, int32_t len, int32_t *r_len)
{
    int8_t *ptr;

    ptr = NULL;
    (*cnt) += len >> BIT_LEN_MEM_CHUNK; //do /
    len = len & LEN_MEM_CHUNK_MOD_NUM; //do %

    if (*cnt >= payload->num_chunks) {
        goto GET_PTR_ERR;
    }

    (*off) += len;
    if (*off >= MAX_SIZE_KERNEL_MEMCHUNK) {
        (*cnt)++;
        if (*cnt >= payload->num_chunks) {
            goto GET_PTR_ERR;
        }
        (*off) &= LEN_MEM_CHUNK_MOD_NUM; // do - MAX_SIZE_KERNEL_MEMCHUNK
    }

    ptr = payload->chunks[*cnt]->data_ptr;
    ptr += (*off);
    *r_len = payload->chunk_item_size[*cnt] - *off;
    return ptr;

GET_PTR_ERR:
    ptr = NULL;
    *cnt = payload->num_chunks;
    *off = 0;
    *r_len = 0;

    return NULL;
}

static int32_t _cp_bio2ioReq (struct request *rq, uint64_t ior_s,
        int32_t total_bytes_cp, data_mem_chunks_t *payload,
        int32_t *cnt, int32_t *off)
{
    dmsc_bio_vec_t bvec;
    struct req_iterator iter;
    int8_t *dst_ptr, *bio_kaddr;
    uint64_t bvec_s, bvec_e, ior_e;
    int32_t chunk_rsize, bvlen, num_cps, bvoff, pieces_cps;
    bool do_bv_cp_again;
    triple_cmp_type_t cmp_res;

    if (*cnt >= payload->num_chunks) {
        //TODO: leave debug message
        return 0;
    }

    chunk_rsize = payload->chunk_item_size[*cnt];
    if (*cnt == (payload->num_chunks - 1) && *off >= chunk_rsize) {
        //TODO: leave debug message
        return 0;
    }

    num_cps = 0;
    dst_ptr = get_chunk_ptr(payload, cnt, off, 0, &chunk_rsize);
    bvec_s = (blk_rq_pos(rq) << BIT_LEN_SECT_SIZE);
    ior_e = ior_s + total_bytes_cp - 1;

    rq_for_each_segment (bvec, rq, iter) {
        bvlen = DMSC_BVLEN(bvec);
        bvec_e = bvec_s + bvlen - 1;

        if (bvec_e < ior_s) {
            bvec_s = bvec_e + 1;
            continue;
        }

        if (bvec_s > ior_e) {
            goto DO_RQ_CP_EXIT;
        }

DO_BV_CP_AGAIN:
        do_bv_cp_again = false;
        bvoff = ior_s - bvec_s;
        bvlen = bvlen - bvoff;

        cmp_res = cmp_triple_int(total_bytes_cp, chunk_rsize, bvlen);

        pieces_cps = 0;
        switch (cmp_res) {
        case cmp_1st_Min: //total_bytes_cp is the smallest
            pieces_cps = total_bytes_cp;
            break;
        case cmp_2nd_Min: //chunk_rsize is the smallest
            pieces_cps = chunk_rsize;

            if (bvlen > chunk_rsize) {
                do_bv_cp_again = true;
            }
            break;
        case cmp_3rd_Min: //bvlen is the smallest
            pieces_cps = bvlen;
            break;
        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong triple cmp result %d\n", cmp_res);
            DMS_WARN_ON(true);
            goto DO_RQ_CP_EXIT;
        }

        bio_kaddr = kmap(DMSC_BVPAGE(bvec));
        memcpy(dst_ptr, bio_kaddr + DMSC_BVOFFSET(bvec) + bvoff, pieces_cps);
        kunmap(DMSC_BVPAGE(bvec));

        total_bytes_cp -= pieces_cps;
        num_cps += pieces_cps;

        if (total_bytes_cp <= 0) {
            goto DO_RQ_CP_EXIT;
        }

        dst_ptr = get_chunk_ptr(payload, cnt, off, pieces_cps, &chunk_rsize);
        ior_s += pieces_cps;

        if (do_bv_cp_again) {
            goto DO_BV_CP_AGAIN;
        }
        bvec_s = bvec_e + 1;
    }

DO_RQ_CP_EXIT:
    return num_cps;
}

void ioReq_WCopyData_bio2mem (io_request_t *ioreq)
{
    struct request *rq;
    data_mem_chunks_t *payload;
    int8_t *curptr;
    uint64_t bytes_skip, ior_s, rq_s, rq_e;
    uint32_t num_sect_skip, total_bytes_cp, i;
    int32_t cnt, offset, num_bytes;

    cnt = 0;
    offset = 0;
    num_bytes = 0;
    payload = &(ioreq->payload);
    curptr = payload->chunks[cnt]->data_ptr;

    if (false == ioreq->is_dms_lb_align) {
        num_sect_skip = head_sect_skip(ioreq->mask_lb_align[0]);
        bytes_skip = num_sect_skip << BIT_LEN_SECT_SIZE;
        memset(curptr, 0, sizeof(int8_t)*bytes_skip);
        curptr += bytes_skip;
        offset = bytes_skip;
    }

    ior_s = (ioreq->kern_sect - ioreq->drive->sects_shift) <<
            BIT_LEN_SECT_SIZE;
    total_bytes_cp = (ioreq->nr_ksects << BIT_LEN_SECT_SIZE);

    for (i = 0; i < ioreq->num_rqs; i++) {
        rq = ioreq->rqs[i]->rq;

        if (IS_ERR_OR_NULL(rq)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO rq already commit\n");
            break;
        }

        rq_s = (blk_rq_pos(rq) << BIT_LEN_SECT_SIZE);
        rq_e = rq_s + (blk_rq_sectors(rq) << BIT_LEN_SECT_SIZE) - 1;
        if (rq_e < ior_s) {
            continue;
        }

        num_bytes = _cp_bio2ioReq(rq, ior_s, total_bytes_cp,
                payload, &cnt, &offset);

        if (num_bytes <= 0) {
            break;
        }

        total_bytes_cp -= num_bytes;

        if (total_bytes_cp <= 0) {
            break;
        }

        ior_s += num_bytes;
    }
}


/*
 * ret value:
 *     ret : number of bytes be copy
 *     *cnt : current mem chunk in payload
 *     *off : offset with the start of chunk
 *
 *     when retrun, *cnt will record new chunk
 *                  *off  will record new off
 */
static uint32_t _cp_payload_mem2bio (struct request *rq,
        uint64_t dn_start, int32_t total_bytes_cp, data_mem_chunks_t *payload,
        uint32_t *cnt, int32_t *off)
{
    dmsc_bio_vec_t bvec;
    struct req_iterator iter;
    int32_t num_cps, bvlen, offset_in_bv, chunk_rsize;
    int32_t bvlen_cp, bytes_shift, pieces_cps;
    uint64_t bvec_start, bvec_end;
    int8_t *bio_kaddr, *src_ptr;
    bool do_bv_cp_again;
    triple_cmp_type_t cmp_res;

    if (*cnt >= payload->num_chunks) {
        //TODO: leave error message
        return 0;
    }

    chunk_rsize = payload->chunk_item_size[*cnt];
    if (*cnt == (payload->num_chunks - 1) && *off >= chunk_rsize) {
        //TODO: leave error message
        return 0;
    }

    src_ptr = get_chunk_ptr(payload, cnt, off, 0, &chunk_rsize);

    bvec_start = blk_rq_pos(rq) << BIT_LEN_SECT_SIZE;
    num_cps = 0;

    if (dn_start < bvec_start) {
        bytes_shift = bvec_start - dn_start;

        num_cps += bytes_shift;

        if (total_bytes_cp <= num_cps) {
            num_cps = total_bytes_cp;
            (void)get_chunk_ptr(payload, cnt, off, num_cps, &chunk_rsize);
            return total_bytes_cp;
        }

        src_ptr = get_chunk_ptr(payload, off, cnt, bytes_shift, &chunk_rsize);
        total_bytes_cp -= bytes_shift;
        dn_start = bvec_start;
    }

    rq_for_each_segment (bvec, rq, iter) {
        bvlen = DMSC_BVLEN(bvec);
        bvec_end = bvec_start + bvlen - 1;

        if (bvec_end < dn_start) {
            bvec_start = bvec_end + 1;
            continue;
        }

DO_BV_CP_AGAIN:
        do_bv_cp_again = false;
        offset_in_bv = dn_start - bvec_start;
        bvlen_cp = bvlen - offset_in_bv;

        cmp_res = cmp_triple_int(total_bytes_cp, chunk_rsize, bvlen_cp);
        pieces_cps = 0;
        switch (cmp_res) {
        case cmp_1st_Min: //total_bytes_cp is the smallest
            do_bv_cp_again = false;
            pieces_cps = total_bytes_cp;
            break;
        case cmp_2nd_Min: //chunk_rsize is the smallest
            pieces_cps = chunk_rsize;

            if (bvlen_cp > chunk_rsize) {
                do_bv_cp_again = true;
            }
            break;
        case cmp_3rd_Min: //bvlen is the smallest
            pieces_cps = bvlen_cp;
            bvec_start = bvec_end + 1;
            break;
        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong triple cmp result %d\n", cmp_res);
            DMS_WARN_ON(true);
            goto _DO_CP_EXIT;
        }

        bio_kaddr = kmap(DMSC_BVPAGE(bvec));
        memcpy(bio_kaddr + DMSC_BVOFFSET(bvec) + offset_in_bv, src_ptr, pieces_cps);
        kunmap(DMSC_BVPAGE(bvec));

        total_bytes_cp -= pieces_cps;
        num_cps += pieces_cps;
        dn_start += pieces_cps;
        src_ptr = get_chunk_ptr(payload, cnt, off, pieces_cps, &chunk_rsize);

        if (total_bytes_cp <= 0) {
            goto _DO_CP_EXIT;
        }

        if (do_bv_cp_again) {
            goto DO_BV_CP_AGAIN;
        }
    }

_DO_CP_EXIT:
    return num_cps;
}

void ioReq_RCopyData_mem2bio (io_request_t *io_req,
        uint64_t startlb, uint32_t lb_len, uint64_t payload_off,
        data_mem_chunks_t *payload)
{
    struct request *rq;
    struct rw_semaphore *rq_lock;
    uint64_t rq_sect_s, dn_sect_s, dn_sect_e, ior_sect_s;
    int32_t dnr_lb_len, dnr_sect_hd_skip, dnr_sect_tail_skip, i;
    int32_t cnt, len_sect_to_cp, total_bytes_cp;
    int32_t num_bytes_cps, offset;
    uint32_t idx_s, idx_e;
    uint8_t lb_mskVal_s, lb_mskVal_e;

    dnr_lb_len = lb_len;
    idx_s = (uint32_t)(startlb - io_req->usrIO_addr.lbid_start);
    idx_e = idx_s + dnr_lb_len - 1;
    lb_mskVal_s = io_req->mask_lb_align[idx_s];
    lb_mskVal_e = io_req->mask_lb_align[idx_e];

    dnr_sect_hd_skip = head_sect_skip(lb_mskVal_s);
    dnr_sect_tail_skip = tail_sect_skip(lb_mskVal_e);

    ior_sect_s = (io_req->usrIO_addr.lbid_start << BIT_LEN_DMS_LB_SECTS) -
                io_req->drive->sects_shift;
    dn_sect_s = ior_sect_s + (payload_off >> BIT_LEN_SECT_SIZE);
    dn_sect_s += dnr_sect_hd_skip;

    cnt = 0;
    offset = (dnr_sect_hd_skip << BIT_LEN_SECT_SIZE);

    len_sect_to_cp = (dnr_lb_len << BIT_LEN_DMS_LB_SECTS) -
            dnr_sect_hd_skip - dnr_sect_tail_skip;
    total_bytes_cp = (len_sect_to_cp << BIT_LEN_SECT_SIZE);
    dn_sect_e = dn_sect_s + len_sect_to_cp - 1;

    rq_lock = &io_req->rq_lock;
    down_read(rq_lock);

    for (i = 0; i < io_req->num_rqs; i++) {
        rq = io_req->rqs[i]->rq;

        if (IS_ERR_OR_NULL(rq)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO rq already commit\n");
            break;
        }

        rq_sect_s = blk_rq_pos(rq);
        if (dn_sect_e < rq_sect_s) {
            break;
        }

        if (dn_sect_s >= (rq_sect_s + blk_rq_sectors(rq))) {
            continue;
        }

        num_bytes_cps = _cp_payload_mem2bio(rq, dn_sect_s << BIT_LEN_SECT_SIZE,
                total_bytes_cp, payload, &cnt, &offset);
        if (num_bytes_cps <= 0) {
            break;
        }

        total_bytes_cp -= num_bytes_cps;

        if (total_bytes_cp <= 0) {
            break;
        }

        dn_sect_s += (num_bytes_cps >> BIT_LEN_SECT_SIZE);
        //WARN_ON(dn_sect_s > dn_sect_e);
    }

    up_read(rq_lock);
}

static int32_t copy_payload_from_tail (uint8_t *lb_msk, uint16_t num_hbid)
{
    int32_t i;
    int32_t mask_bit_start;
    int32_t num_of_sector_to_copy = 0;

    for (i = num_hbid - 1; i >= 0; i--) {
        for (mask_bit_start = BIT_LEN_PER_1_MASK - 1; mask_bit_start >= 0;
                mask_bit_start--) {
            if (!test_bit(mask_bit_start,
                    (const volatile void *)(&lb_msk[i]))) {
                num_of_sector_to_copy++;
            } else {
                return num_of_sector_to_copy;
            }
        }
    }
    return 0;
}

static int32_t copy_payload_from_head (uint8_t *lb_msk, uint16_t num_hbid)
{
    int32_t mask_bit_start;
    int32_t i, num_of_sector_to_copy = 0;

    for (i = 0; i < num_hbid; i++) {
        for (mask_bit_start = 0; mask_bit_start < (int32_t)BIT_LEN_PER_1_MASK;
                mask_bit_start++) {
            if (!test_bit(mask_bit_start,
                    (const volatile void *)(&lb_msk[i]))) {
                num_of_sector_to_copy++;
            } else {
                return num_of_sector_to_copy;
            }
        }
    }

    return 0;
}

static int8_t *get_payload_ref (io_request_t *ioreq,
        uint64_t payload_offset, uint32_t len, uint32_t *ref_len)
{
    int8_t *ref_ptr;
    uint64_t chunk_start, off, next_chunk_start;
    uint32_t chunk_index;
    data_mem_chunks_t *data;

    ref_ptr = NULL;
    *ref_len = 0;

    data = &ioreq->payload;

    chunk_index = payload_offset >> BIT_LEN_MEM_CHUNK;
    chunk_start = chunk_index << BIT_LEN_MEM_CHUNK;

    off = payload_offset - chunk_start;
    ref_ptr = data->chunks[chunk_index]->data_ptr + off;

    next_chunk_start = chunk_start + data->chunk_item_size[chunk_index];

    *ref_len = next_chunk_start >= (payload_offset + len) ?
            len : (next_chunk_start - payload_offset);

    return ref_ptr;
}

void ioReq_RCopyData_mem2wbuff (io_request_t *ioReq, uint64_t startlb, uint32_t lb_len,
        uint64_t payload_off, data_mem_chunks_t *payload)
{
    uint8_t *msk_ptr;
    int8_t *src_ptr, *dst_ptr;
    uint32_t num_of_sector_to_copy, len, msk_idx; //payload_len
    uint16_t num_hbid;

    msk_idx = (uint32_t)(startlb - ioReq->usrIO_addr.lbid_start);
    msk_ptr = &(ioReq->mask_lb_align[msk_idx]);

    src_ptr = payload->chunks[0]->data_ptr;
    num_hbid = lb_len;
    dst_ptr = get_payload_ref(ioReq, payload_off, num_hbid << BIT_LEN_DMS_LB_SIZE, &len);

    num_of_sector_to_copy = copy_payload_from_head(msk_ptr, num_hbid);
    if (num_of_sector_to_copy > 0) {
        memcpy(dst_ptr, src_ptr, (num_of_sector_to_copy << BIT_LEN_SECT_SIZE));
    }

    num_of_sector_to_copy = copy_payload_from_tail(msk_ptr, num_hbid);
    if (num_of_sector_to_copy > 0) {
        dst_ptr = dst_ptr + (num_hbid << BIT_LEN_DMS_LB_SIZE) -
                (num_of_sector_to_copy << BIT_LEN_SECT_SIZE);
        src_ptr = src_ptr + (num_hbid << BIT_LEN_DMS_LB_SIZE) -
                (num_of_sector_to_copy << BIT_LEN_SECT_SIZE);
        memcpy(dst_ptr, src_ptr, (num_of_sector_to_copy << BIT_LEN_SECT_SIZE));
    }
}

#ifdef  ENABLE_PAYLOAD_CACHE_FOR_DEBUG
void print_payload_content (int32_t log_level, int8_t *ptr)
{
    int i = 0;

    if (log_level < dms_client_config->log_level) {
        return;
    }

    dms_printk(log_level, "\n");
    for (i = 0; i < 64; i++) {
        if (i % 16 == 0 && i != 0) {
            dms_printk(log_level, "\n");
        }

        dms_printk(log_level, "0x%02x ", (uint8_t)*ptr);
        ptr++;
    }

    dms_printk(log_level, "\n");
}

void compare_payload (io_request_t *req)
{
    int8_t *ptr_payload, *tmp, *target;
    int32_t ret;
    uint64_t i, j;
    uint64_t sect;
    //char buffer[512];

    target = req->payload;
    //ptr_payload = &(buffer[0]);
    ptr_payload = (int8_t *)ccma_malloc_pool(MEM_Buf_CPayload);
    tmp = ptr_payload;

    memset(ptr_payload, 0,
            sizeof(int8_t)*(MAX_SECTS_PER_RQ << BIT_LEN_SECT_SIZE));

    for (i = 0; i < req->lbid_len; i++) {
        sect = (req->begin_lbid + i)*8L;
        for (j = 0; j < 8L; j++) {

            ret = query_payload_in_cache(req->drive->volinfo.volid,
                    sect + j, tmp);

            if (ret != 0) {
                dms_printk(LOG_LVL_DEBUG, "payload not found for "
                        "sector: %llu\n", sect + j);
                //memset(0, tmp, sizeof(char)*512);
                //fill with empty
            } else {
                if (memcmp(target, tmp, sizeof(char)*512) != 0) {
                    dms_printk(LOG_LVL_ERR, "Lego: FATAL ERROR: %s "
                            "DATA is in-consist with payload cache for "
                            "sector:%llu\n", __func__, sect + j);
                }
            }

            tmp = tmp + 512;
            target = target + 512;
        }
    }

    ccma_free_pool(MEM_Buf_CPayload, ptr_payload);
}
#endif

static uint32_t _set_bio_rq_empty (struct request *rq,
        uint64_t nn_s, int32_t lens)
{
    dmsc_bio_vec_t bvec;
    struct req_iterator iter;
    int32_t num_set, len_to_set, bvlen, offset_in_bv, bvlen_set;
    uint64_t bvec_s, bvec_e;
    int8_t *bio_kaddr;

    bvec_s = (blk_rq_pos(rq) << BIT_LEN_SECT_SIZE);
    num_set = 0;

    if (nn_s < bvec_s) {
        num_set = bvec_s - nn_s;
        nn_s = bvec_s;
    }

    rq_for_each_segment (bvec, rq, iter) {
        bvlen = DMSC_BVLEN(bvec);
        bvec_e = bvec_s + bvlen - 1;

        if (bvec_e < nn_s) {
            bvec_s = bvec_e + 1;
            continue;
        }

        offset_in_bv = nn_s - bvec_s;
        bvlen_set = bvlen - offset_in_bv;

        len_to_set = lens > bvlen_set ? bvlen_set : lens;

        bio_kaddr = kmap(DMSC_BVPAGE(bvec));
        memset(bio_kaddr + DMSC_BVOFFSET(bvec) + offset_in_bv, 0, len_to_set);
        kunmap(DMSC_BVPAGE(bvec));

        num_set += len_to_set;
        lens -= len_to_set;

        if (lens <= 0) {
            break;
        }

        nn_s += len_to_set;
        bvec_s = bvec_e + 1;
    }

    return num_set;
}

//NOTE: set certain range of bio as 0
void ioReq_RCopyData_zero2bio (io_request_t *ioreq, uint64_t startLB, uint32_t LB_cnt)
{
    struct request *rq;
    struct rw_semaphore *rq_lock;
    uint64_t rq_sect_s, nn_sect_s, nn_sect_e;
    int32_t num_sect_cps, num_bytes_set;
    int32_t len_sects, i, nn_sect_hd_skip, nn_sect_tail_skip;
    int32_t lb_index;

    rq_lock = &ioreq->rq_lock;
    lb_index = startLB - ioreq->usrIO_addr.lbid_start;
    nn_sect_hd_skip = head_sect_skip(ioreq->mask_lb_align[lb_index]);
    nn_sect_tail_skip = tail_sect_skip(ioreq->mask_lb_align[lb_index + LB_cnt - 1]);

    nn_sect_s = (startLB << BIT_LEN_DMS_LB_SECTS) + nn_sect_hd_skip;
    nn_sect_s -= ioreq->drive->sects_shift;

    len_sects = (LB_cnt << BIT_LEN_DMS_LB_SECTS) -
            nn_sect_hd_skip - nn_sect_tail_skip;
    nn_sect_e = nn_sect_s + len_sects - 1;

    rq_sect_s = 0;

    down_read(rq_lock);
    for (i = 0; i < ioreq->num_rqs; i++) {
        rq = ioreq->rqs[i]->rq;

        if (IS_ERR_OR_NULL(rq)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO rq already commit\n");
            break;
        }

        rq_sect_s = blk_rq_pos(rq);
        if (nn_sect_e < rq_sect_s) {
            break;
        }

        if (nn_sect_s >= (rq_sect_s + blk_rq_sectors(rq))) {
            continue;
        }

        num_bytes_set = _set_bio_rq_empty(rq, nn_sect_s << BIT_LEN_SECT_SIZE,
                len_sects << BIT_LEN_SECT_SIZE);
        num_sect_cps = num_bytes_set >> BIT_LEN_SECT_SIZE;
        len_sects -= num_sect_cps;

        if (len_sects <= 0) {
            break;
        }

        nn_sect_s += num_sect_cps;
    }
    up_read(rq_lock);
}
