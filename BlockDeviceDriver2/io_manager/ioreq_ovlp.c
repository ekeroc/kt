/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * ioreq_ovlp.c
 *
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/swap.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/common.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "ioreq_ovlp.h"
#include "discoC_IOReq_fsm.h"
#include "discoC_IORequest.h"

/*
 * return: false if no overlap; true otherwise
 */
static IOR_ovlp_wait_t chk_lb_ovlp (UserIO_addr_t *ior_prev,
        UserIO_addr_t *ior_next, uint64_t *ovlp_s, uint32_t *ovlp_l)
{
    uint64_t prev_e, next_e;
    IOR_ovlp_wait_t ret;

    ret = IOR_Wait_NoOvlp;

    do {
        next_e = ior_next->lbid_start + ior_next->lbid_len - 1;

        if (ior_prev->lbid_start > next_e) {
            break;
        }

        prev_e = ior_prev->lbid_start + ior_prev->lbid_len - 1;

        if (ior_next->lbid_start > prev_e) {
            break;
        }

        if (ior_prev->lbid_start > ior_next->lbid_start) {
            *ovlp_s = ior_prev->lbid_start;

            ret = IOR_Wait_Payload_Partial_Ovlp;
            if (prev_e <= next_e) {
                *ovlp_l = ior_prev->lbid_len;
            } else {
                *ovlp_l = next_e - ior_prev->lbid_start + 1;
            }
        } else if (ior_prev->lbid_start == ior_next->lbid_start) {
            *ovlp_s = ior_prev->lbid_start;

            if (prev_e < next_e) {
                ret = IOR_Wait_Payload_Partial_Ovlp;
                *ovlp_l = ior_prev->lbid_len;
            } else {
                ret = IOR_Wait_Payload_All_Ovlp;
                *ovlp_l = ior_next->lbid_len;
            }
        } else {
            *ovlp_s = ior_next->lbid_start;

            ret = IOR_Wait_Payload_Partial_Ovlp;
            if (prev_e >= next_e) {
                *ovlp_l = ior_next->lbid_len;
            } else {
                *ovlp_l = prev_e - ior_next->lbid_start + 1;
            }
        }
    } while (0);

    return ret;
}

/*
 *  ret true  : no ovlp check
 *      false : do ovlp check
 */
static bool chk_io_rw (io_request_t *ior_prev, io_request_t *ior_next)
{
    bool ret;

    ret = true;

    do {
        //Oning io do write, do check
        if (KERNEL_IO_WRITE == ior_prev->iodir) {
            ret = false;
            break;
        }

        //Oning io do read, cur do write, do check
        if (KERNEL_IO_WRITE == ior_next->iodir) {
            ret = false;
            break;
        }
    } while (0);

    return ret;
}

static ovlp_item_t *gen_ovlp_item (io_request_t *ior_prev,
        io_request_t *ior_next, IOR_ovlp_wait_t wait_type,
        uint64_t ovlp_s, uint32_t ovlp_l)
{
    ovlp_item_t *ovlpItem;

    ovlpItem = discoC_mem_alloc(sizeof(ovlp_item_t), GFP_NOWAIT | GFP_ATOMIC);
    if (unlikely(IS_ERR_OR_NULL(ovlpItem))) {
        /*
         * Lego: NOTE Maybe we should fail this request,
         * bcz it might make data in-consistent.
         */

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN alloc mem for ovlp item fail\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    ovlpItem->process_ior = ior_prev;
    ovlpItem->wait_ior = ior_next;
    ovlpItem->wait_type = wait_type;

    INIT_LIST_HEAD(&ovlpItem->wait_list);
    INIT_LIST_HEAD(&ovlpItem->wake_list);

    ovlpItem->ovlp_s = ovlp_s;
    ovlpItem->ovlp_l = ovlp_l;
    return ovlpItem;
}

IOR_ovlp_res_t ioReq_chk_ovlp (struct list_head *ovlp_q, io_request_t *io_req)
{
    io_request_t *cur, *next;
    IOR_ovlp_res_t ret;
    IOR_ovlp_wait_t ovlp_w_type;
    ovlp_item_t *ovlpItem;
    uint64_t ovlp_s;
    uint32_t ovlp_l;

    ret = IOR_ovlp_NoOvlp;

    list_for_each_entry_safe_reverse (cur, next, ovlp_q, list2ioReqPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN ovlp q list broken\n");
            DMS_WARN_ON(true);
            ret = IOR_ovlp_ListBroken;
            break;
        }

        if (io_req == cur) {
            ret = IOR_ovlp_InQ;
            break;
        }

        if (chk_io_rw(cur, io_req)) {
            continue;
        }

        ovlp_w_type = chk_lb_ovlp(&(cur->usrIO_addr), &(io_req->usrIO_addr),
                &ovlp_s, &ovlp_l);
        if (IOR_Wait_NoOvlp == ovlp_w_type) {
            continue;
        }

        io_req->is_ovlp_ior = true;

        ovlpItem = gen_ovlp_item(cur, io_req, ovlp_w_type, ovlp_s, ovlp_l);
        if (unlikely(IS_ERR_OR_NULL(ovlpItem))) {
            /*
             * Lego: NOTE Maybe we should fail this request,
             * bcz it might make data in-consistent.
             */
            break;
        }

        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG cur ior (%d %llu %d) ovlp with ior (%d %llu %d)\n",
                io_req->ioReqID, io_req->kern_sect, io_req->nr_ksects,
                cur->ioReqID, cur->kern_sect, cur->nr_ksects);

        ioReq_add_waitReq(io_req, &ovlpItem->wait_list);
        ioReq_add_wakeReq(cur, &ovlpItem->wake_list);

        ret = IOR_ovlp_Ovlp;
    }

    return ret;
}

#if 0
static void show_ovlp_item (ovlp_item_t *item)
{
    io_request_t *prev_ior, *next_ior;

    prev_ior = item->process_ior;
    next_ior = item->wait_ior;
    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG prev %d (%llu %d) wait %d (%llu %d)\n",
            prev_ior->ioReqID, prev_ior->begin_lbid, prev_ior->lbid_len,
            next_ior->ioReqID, next_ior->begin_lbid, next_ior->lbid_len);
    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG wait type %d ovlp_s %llu ovlp_l %d\n",
            item->wait_type, item->ovlp_s, item->ovlp_l);
}
#endif

int32_t ioReq_get_ovlpReq (struct list_head *ovlp_q, struct list_head *buff_list,
        io_request_t *io_req)
{
    ovlp_item_t *cur, *next;
    io_request_t *wake_ior;
    uint32_t cnt, num_ovlp_ior;
    IOR_FSM_action_t action;

    num_ovlp_ior = 0;

    spin_lock(&io_req->ovlp_group_lock);
    list_for_each_entry_safe (cur, next, &io_req->wake_list, wake_list) {
        //show_ovlp_item(cur);
        atomic_dec(&io_req->cnt_ovlp_wake);
        wake_ior = cur->wait_ior;

        spin_lock(&wake_ior->ovlp_group_lock);

        list_del(&cur->wait_list);
        cnt = atomic_sub_return(1, &wake_ior->cnt_ovlp_wait);
        spin_unlock(&wake_ior->ovlp_group_lock);

        if (cnt == 0) {
            action = IOReq_update_state(&wake_ior->ioReq_stat, IOREQ_E_OvlpIODone);
            if (IOREQ_A_SubmitIO == action) {
                list_move_tail(&wake_ior->list2ioReqPool, buff_list);
                num_ovlp_ior++;
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN fail ioReq stat tr OvlpIODone %s %s\n",
                        IOReq_get_state_fsm_str(&wake_ior->ioReq_stat),
                        IOReq_get_action_str(action));
            }
        }
    }

    spin_unlock(&io_req->ovlp_group_lock);

    //NOTE: avoid free inside spin lock
    list_for_each_entry_safe (cur, next, &io_req->wake_list, wake_list) {
        list_del(&(cur->wake_list));
        discoC_mem_free(cur);
    }

    return num_ovlp_ior;
}

void ioReq_rm_all_ovlpReq (io_request_t *io_req)
{
    ovlp_item_t *cur, *next;
    struct list_head buff;
    bool flag;

    //NTOE: Should be 0

    INIT_LIST_HEAD(&buff);
    flag = false;

    spin_lock(&io_req->ovlp_group_lock);
    list_for_each_entry_safe (cur, next, &io_req->wake_list, wake_list) {
        if (IS_ERR_OR_NULL(cur)) {
            flag = true;
            break;
        }

        list_move_tail(&cur->wake_list, &buff);
        atomic_dec(&io_req->cnt_ovlp_wake);
    }
    spin_unlock(&io_req->ovlp_group_lock);

    if (list_empty(&buff) == false) {
        list_for_each_entry_safe (cur, next, &buff, wake_list) {
            list_del(&cur->wake_list);
            discoC_mem_free(cur);
        }
    }

    if (unlikely(flag)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN overlap ior group list broken of iorid=%d\n",
                io_req->ioReqID);
        DMS_WARN_ON(true);
    }
}
