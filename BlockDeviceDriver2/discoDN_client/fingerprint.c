/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * fingerprint.c
 *
 * We can calculate fingerprint at different thread.
 *
 * Candidate 1: thread that submit write payload request
 * Candidate 2: DN send thread
 *
 * Now we choose 2, but current method cannot support multiple threads
 */
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../config/dmsc_config.h"
#include "../metadata_manager/metadata_manager.h"
#include "../lib/sha1.h"
#include "../lib/md5.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "fingerprint.h"

static void _cal_fingerprint (fp_method_t fp_method, int8_t *fp_dst_ptr,
        DNUData_Req_t *dnreq)
{
    //struct payload_segment *data_seg;
    struct iovec *data_seg;
    uint32_t fp_m_index;
    uint32_t num_data_seg, i, j, payload_size, lb_off;
    uint8_t *payload_ptr, *fp_value, *fp_src_ptr;
    uint8_t fp_m_offset;

    num_data_seg = dnreq->num_UDataSeg;
    lb_off = (dnreq->dn_payload_offset >> BIT_LEN_DMS_LB_SIZE);

    for (i = 0; i < num_data_seg; i++) {
        data_seg = &(dnreq->UDataSeg[i]);
        payload_ptr = data_seg->iov_base;
        payload_size = data_seg->iov_len;

        /*
         * NOTE: if there are multiple sender thread, we need lock protection
         */
        for (j = 0; j < payload_size; j += (uint32_t)DMS_LB_SIZE) {
            fp_m_index = lb_off >> BIT_LEN_BYTE;
            fp_m_offset = lb_off & BYTE_MOD_NUM;
            fp_value = &(dnreq->ref_fp_map[fp_m_index]);

            fp_src_ptr = dnreq->ref_fp_cache + lb_off*FP_LENGTH;
            if (test_and_set_bit(fp_m_offset, (volatile void *)fp_value)) {
                memcpy(fp_dst_ptr, fp_src_ptr, FP_LENGTH);
            } else {
                switch (fp_method) {
                case fp_sha1:
                    sha1(payload_ptr, (uint32_t)DMS_LB_SIZE, fp_dst_ptr);
                    break;
                case fp_md5:
                    md5_sum(payload_ptr, (size_t)DMS_LB_SIZE, fp_dst_ptr);
                    break;
                default:
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN wrong fp method %d\n", fp_method);
                    DMS_WARN_ON(true);
                    break;
                }
                memcpy(fp_src_ptr, fp_dst_ptr , FP_LENGTH);
            }
            lb_off++;
            payload_ptr = payload_ptr + (uint32_t)DMS_LB_SIZE;
            fp_dst_ptr += sizeof(uint8_t)*FP_LENGTH;
        }
    }
}

/*
 * get_fingerprint: get fingerprint
 * @fp_method:  which fingerprint generate algorithm (MD5 or sha1)
 * @fp_dst_ptr: data buffer that hold fingerprint for sending to DN
 * @dnreq: datanode request the has data for calculating fingerprint
 *
 * Return: size of fingerprint data
 */
uint32_t get_fingerprint (fp_method_t fp_method, int8_t *fp_dst_ptr,
        DNUData_Req_t *dnreq)
{
    uint32_t fp_total_len;

    fp_total_len = FP_LENGTH*dnreq->num_hbids;

    if (fp_disable == fp_method) {
        memset(fp_dst_ptr, 0, sizeof(uint8_t)*fp_total_len);
    } else {
        _cal_fingerprint(fp_method, fp_dst_ptr, dnreq);
    }

    return fp_total_len;
}
