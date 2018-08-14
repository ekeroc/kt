/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * FlowControl.c
 * 
 *  Collect data about
 *  a. NN : Avg latency (all / Query Metadata / WriteAndGetOld / WriteWithoutGetOld)
 *     NN : Throughput
 *     NN : IOPS
 *  b. DN : Avg latency (all / Query Metadata / WriteAndGetOld / WriteWithoutGetOld)
 *     DN : Throughput
 *     DN : IOPS
 *
 *  TODO: We should encapsulate performance data into data structure
 *        add_req_response_time should split into small function
 *        Although it's safe now, we should create a ref_cnt in res_item
 *        for make sure it won't be free when someone ref it
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/swap.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../common/dms_client_mm.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../payload_manager/payload_manager_export.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "FlowControl_private.h"
#include "../vdisk_manager/volume_manager.h"
#include "FlowControl.h"
#include "../config/dmsc_config.h"
#include "../common/cache.h"

void Get_NN_Sample_Performance (dms_res_item_t *res_item, int32_t *old_tp,
        int32_t *old_iops, int32_t *curr_tp, int32_t *curr_iops)
{
    int64_t period;

    read_lock(&res_item->res_lock);
    if (res_item->res_state == RESOURCES_STAT_DATA_READY &&
            res_item->is_available) {
        *old_tp = res_item->NN_Process_Sample_TP;
        *old_iops = res_item->NN_Process_Sample_IOPS;

        period = (jiffies_to_msecs(res_item->NN_Process_Sample_End_Time -
                res_item->NN_Process_Sample_Start_Time) / 1000L);
        *curr_tp = do_divide(res_item->NN_Process_Sample_Bytes,
                period);
        *curr_iops = do_divide(atomic64_read(&res_item->NN_Process_Sample_CNT),
                period);
    } else {
        if (!res_item->is_available) {
            *old_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *old_iops = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_iops = DMS_RESOURCE_NOT_AVAILABLE;
        } else {
            *old_tp = DMS_RESOURCE_NOT_READY;
            *old_iops = DMS_RESOURCE_NOT_READY;
            *curr_tp = DMS_RESOURCE_NOT_READY;
            *curr_iops = DMS_RESOURCE_NOT_READY;
        }
    }
    read_unlock(&res_item->res_lock);
}

void Get_DN_Sample_Performance (dms_res_item_t *res_item, int32_t *old_read_tp,
        int32_t *old_read_iops, int32_t *old_write_tp,
        int32_t *old_write_iops, int32_t *curr_read_tp,
        int32_t *curr_read_iops, int32_t *curr_write_tp,
        int32_t *curr_write_iops)
{
    int64_t period_read = 0, period_write = 0;

    read_lock(&res_item->res_lock);
    if (res_item->res_state == RESOURCES_STAT_DATA_READY &&
            res_item->is_available) {
        *old_read_tp = res_item->DN_Process_Sample_Read_TP;
        *old_read_iops = res_item->DN_Process_Sample_Read_IOPS;
        *old_write_tp = res_item->DN_Process_Sample_Write_TP;
        *old_write_iops = res_item->DN_Process_Sample_Write_IOPS;

        period_read = (jiffies_to_msecs(res_item->DN_Process_Sample_Read_End_Time -
                res_item->DN_Process_Sample_Read_Start_Time) / 1000L);

        *curr_read_tp = do_divide(res_item->DN_Process_Sample_Read_Bytes,
                period_read);
        *curr_read_iops = do_divide(atomic64_read(&res_item->DN_Process_Sample_Read_CNT),
                period_read);

        period_write = (jiffies_to_msecs(res_item->DN_Process_Sample_Write_End_Time -
                res_item->DN_Process_Sample_Write_Start_Time) / 1000L);
        *curr_write_tp = do_divide(res_item->DN_Process_Sample_Write_Bytes,
                period_read);
        *curr_write_iops = do_divide(atomic64_read(&res_item->DN_Process_Sample_Write_CNT),
                period_read);
    } else {
        if (!res_item->is_available) {
            *old_read_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *old_read_iops = DMS_RESOURCE_NOT_AVAILABLE;
            *old_write_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *old_write_iops = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_read_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_read_iops = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_write_tp = DMS_RESOURCE_NOT_AVAILABLE;
            *curr_write_iops = DMS_RESOURCE_NOT_AVAILABLE;
        } else {
            *old_read_tp = DMS_RESOURCE_NOT_READY;
            *old_read_iops = DMS_RESOURCE_NOT_READY;
            *old_write_tp = DMS_RESOURCE_NOT_READY;
            *old_write_iops = DMS_RESOURCE_NOT_READY;
            *curr_read_tp =  DMS_RESOURCE_NOT_READY;
            *curr_read_iops = DMS_RESOURCE_NOT_READY;
            *curr_write_tp = DMS_RESOURCE_NOT_READY;
            *curr_write_iops = DMS_RESOURCE_NOT_READY;
        }
    }
    read_unlock(&res_item->res_lock);
}

void Get_NN_Avg_Latency_Time (dms_res_item_t *res_item, int32_t *all,
                              int32_t *q_metadata, int32_t *alloc_no_old,
                              int32_t *alloc_with_old)
{
    uint64_t total_cnt;

    *all = 0;
    *q_metadata = 0;
    *alloc_no_old = 0;
    *alloc_with_old = 0;
    total_cnt = 0;

    read_lock(&res_item->res_lock);
    if (res_item->res_state == RESOURCES_STAT_DATA_READY &&
            res_item->is_available) {
        total_cnt =  atomic64_read(&res_item->Total_process_cnt);
        *all = do_divide(res_item->Total_process_time, total_cnt);

        total_cnt = atomic64_read(&res_item->NN_Process_CNT_Query_Metadata);
        *q_metadata = do_divide(res_item->NN_Process_Time_Query_Metadata,
                total_cnt);

        total_cnt = atomic64_read(&res_item->NN_Process_CNT_Alloc_Metadata_And_NoOld);
        *alloc_no_old = do_divide(res_item->NN_Process_Time_Alloc_Metadata_And_NoOld,
                total_cnt);

        total_cnt = atomic64_read(&res_item->NN_Process_CNT_Alloc_Metadata_And_WithOld);
        *alloc_with_old = do_divide(res_item->NN_Process_Time_Alloc_Metadata_And_WithOld,
                total_cnt);
    } else {
        if (!res_item->is_available) {
            *all = DMS_RESOURCE_NOT_AVAILABLE;
            *q_metadata = DMS_RESOURCE_NOT_AVAILABLE;
            *alloc_no_old = DMS_RESOURCE_NOT_AVAILABLE;
            *alloc_with_old = DMS_RESOURCE_NOT_AVAILABLE;
        } else {
            *all = DMS_RESOURCE_NOT_READY;
            *q_metadata = DMS_RESOURCE_NOT_READY;
            *alloc_no_old = DMS_RESOURCE_NOT_READY;
            *alloc_with_old = DMS_RESOURCE_NOT_READY;
        }
    }
    read_unlock(&res_item->res_lock);
}

void Get_DN_Avg_Latency_Time (dms_res_item_t *res_item, int32_t *all,
        int32_t *req_read, int32_t *req_write,
        int32_t *req_write_mamack)
{
    uint64_t total_cnt;

    *all = 0;
    *req_read = 0;
    *req_write = 0;
    *req_write_mamack = 0;
    total_cnt = 0;

    read_lock(&res_item->res_lock);

    if (res_item->res_state == RESOURCES_STAT_DATA_READY &&
            res_item->is_available) {
        total_cnt =  atomic64_read(&res_item->Total_process_cnt);
        *all = do_divide(res_item->Total_process_time, total_cnt);

        total_cnt =  atomic64_read(&res_item->DN_Process_CNT_Read);
        *req_read = do_divide(res_item->DN_Process_Time_Read, total_cnt);

        total_cnt =  atomic64_read(&res_item->DN_Process_CNT_Write);
        *req_write = do_divide(res_item->DN_Process_Time_Write, total_cnt);
        *req_write_mamack = do_divide(res_item->DN_Process_Time_Write_MemAck,
                total_cnt);
    } else {
        if (!res_item->is_available) {
            *all = DMS_RESOURCE_NOT_AVAILABLE;
            *req_read = DMS_RESOURCE_NOT_AVAILABLE;
            *req_write = DMS_RESOURCE_NOT_AVAILABLE;
        } else {
            *all = DMS_RESOURCE_NOT_READY;
            *req_read = DMS_RESOURCE_NOT_READY;
            *req_write = DMS_RESOURCE_NOT_READY;
        }
    }
    read_unlock(&res_item->res_lock);
}

static void init_res_perf_data (dms_res_item_t *res_item)
{
    res_item->Total_process_time = 0;
    atomic64_set(&res_item->Total_process_cnt, 0);

    res_item->NN_Process_Time_Query_Metadata = 0;
    res_item->NN_Process_Time_Alloc_Metadata_And_NoOld = 0;
    res_item->NN_Process_Time_Alloc_Metadata_And_WithOld = 0;

    atomic64_set(&res_item->NN_Process_CNT_Query_Metadata, 0);
    atomic64_set(&res_item->NN_Process_CNT_Alloc_Metadata_And_NoOld, 0);
    atomic64_set(&res_item->NN_Process_CNT_Alloc_Metadata_And_WithOld, 0);

    res_item->NN_Process_Sample_Start_Time = 0;
    res_item->NN_Process_Sample_End_Time = 0;

    res_item->NN_Process_Sample_TP = 0;
    res_item->NN_Process_Sample_IOPS = 0;
    res_item->NN_Process_Sample_Bytes = 0;

    res_item->DN_Process_Time_Read = 0;
    res_item->DN_Process_Time_Write = 0;
    res_item->DN_Process_Time_Write_MemAck = 0;
    atomic64_set(&res_item->DN_Process_CNT_Read, 0);
    atomic64_set(&res_item->DN_Process_CNT_Write, 0);
    res_item->DN_Process_Total_Read_Bytes = 0;
    res_item->DN_Process_Total_Write_Bytes = 0;
    res_item->DN_Process_Sample_Read_Start_Time = 0;
    res_item->DN_Process_Sample_Read_End_Time = 0;
    res_item->DN_Process_Sample_Write_Start_Time = 0;
    res_item->DN_Process_Sample_Write_End_Time = 0;

    res_item->DN_Process_Sample_Read_TP = 0;
    res_item->DN_Process_Sample_Read_IOPS = 0;
    res_item->DN_Process_Sample_Read_Bytes = 0;

    res_item->DN_Process_Sample_Write_TP = 0;
    res_item->DN_Process_Sample_Write_IOPS = 0;
    res_item->DN_Process_Sample_Write_Bytes = 0;
}

void reset_all_res_perf_data (void) {
    dms_res_item_t *cur, *next;

    write_lock(&fcRes_qlock);
    list_for_each_entry_safe(cur, next, &fc_reslist, list_resq) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fc res list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        init_res_perf_data(cur);
    }
    write_unlock(&fcRes_qlock);
}

static void print_resource (int32_t log_level, dms_res_item_t *new_res)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    if (log_level <  dms_client_config->log_level) {
        return;
    }

    ccma_inet_aton(new_res->ipaddr, hname, IPV4ADDR_NM_LEN);
    dms_printk(log_level,
            "DMSC INFO hostname:%s port:%d last_modified_index=%d res_id=%d\n",
            hname, new_res->port, new_res->last_modified_index, new_res->res_id);
    dms_printk(log_level,
            "DMSC INFO num_of_req_time_in_count:%d state:%d res_type=%d "
            "total_response_time=%llu\n",
            new_res->num_of_req_time_in_count, new_res->res_state,
            new_res->res_type, new_res->total_response_time);
}

static dms_res_item_t *create_resource_item (uint32_t ipaddr, uint32_t port,
        uint16_t type, uint32_t res_id, bool is_avail)
{
    dms_res_item_t *new_res;
    int8_t hname[IPV4ADDR_NM_LEN];

    new_res = (dms_res_item_t *)discoC_mem_alloc(sizeof(dms_res_item_t),
            GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(new_res)) {
        ccma_inet_aton(new_res->ipaddr, hname, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to alloc res item for %s:%d\n", hname, port);
        DMS_WARN_ON(true);
        return NULL;
    }

    new_res->ipaddr = ipaddr;

    new_res->port = port;
    new_res->last_modified_index = 0;
    new_res->res_id = res_id;
    rwlock_init(&new_res->res_lock);

    new_res->num_of_req_time_in_count = DEFAULT_NUM_REQ_IN_COUNT;
    new_res->res_response_time =
                       (uint64_t *)discoC_mem_alloc(sizeof(uint64_t) *
                       new_res->num_of_req_time_in_count,
                       GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(new_res)) {
        ccma_inet_aton(new_res->ipaddr, hname, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to allocate res_response_time for %s:%d\n",
                   hname, port);
        discoC_mem_free(new_res);
        new_res = NULL;
        DMS_WARN_ON(true);
        return NULL;
    }

    new_res->res_state = RESOURCES_STAT_DATA_COLLECTING;
    new_res->res_type = type;
    new_res->total_response_time = 0;

    if (type == RES_TYPE_NN) {
        new_res->fc_slow_start_rate = DEFAULT_NN_SLOW_START_RATE;
        new_res->fc_rate_age_time = DEFAULT_NN_RATE_AGE_TIME;
        new_res->fc_curr_rate = DEFAULT_NN_FIXED_RATE;
        new_res->fc_curr_max_reqs = DEFAULT_NN_MAX_REQUESTS;
        new_res->fc_start_max_reqs = DEFAULT_NN_MAX_REQUESTS;
    } else {
        new_res->fc_slow_start_rate = DEFAULT_DN_SLOW_START_RATE;
        new_res->fc_rate_age_time = DEFAULT_DN_RATE_AGE_TIME;
        new_res->fc_curr_rate = DEFAULT_DN_FIXED_RATE;
        new_res->fc_curr_max_reqs = DEFAULT_DN_MAX_REQUESTS;
        new_res->fc_start_max_reqs = DEFAULT_DN_MAX_REQUESTS;
    }

    mutex_init(&new_res->alloc_res_lock);
    atomic_set(&new_res->alloc_out_res_rate, 0);
    atomic_set(&new_res->alloc_out_res_max, 0);
    new_res->last_access_time = 0;
    INIT_LIST_HEAD(&new_res->list_resq);
    init_res_perf_data(new_res);

    new_res->is_available = is_avail;
    atomic64_set(&new_res->number_timeout_occur, 0);
    return new_res;
}

static dms_res_item_t *find_resource_item (uint32_t ipaddr, uint32_t port,
                                           bool enable_lock)
{
    dms_res_item_t *res_item;
    dms_res_item_t *cur, *next;

    res_item = NULL;
    if (enable_lock) {
        write_lock(&fcRes_qlock);
    }

    list_for_each_entry_safe (cur, next, &fc_reslist, list_resq) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN res list broken when find res\n");
            DMS_WARN_ON(true);
            break;
        }

        if (ipaddr == cur->ipaddr && cur->port == port) {
            res_item = cur;
            break;
        }
    }

    if (enable_lock) {
        write_unlock(&fcRes_qlock);
    }

    return res_item;
}

static dms_res_item_t *add_resource_item (uint32_t ipaddr, uint32_t port,
        res_type_t type, bool is_avail)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    dms_res_item_t *new_res;
    bool list_bug;

    list_bug = false;
    write_lock(&fcRes_qlock);

    do {
        new_res = find_resource_item(ipaddr, port, false);
        if (IS_ERR_OR_NULL(new_res)) {
            new_res = create_resource_item(ipaddr, port, type,
                    g_res_id, is_avail);

            if (unlikely(IS_ERR_OR_NULL(new_res))) {
                ccma_inet_aton(new_res->ipaddr, hname, IPV4ADDR_NM_LEN);
                dms_printk(LOG_LVL_WARN,
                        "DMSC WANR cannot create resource item for %s:%d\n",
                        hname, port);
                DMS_WARN_ON(true);
                break;
            }

            g_res_id++;
            //print_resource(new_res);
            //lego: change from list_add to list_add_tail
            if (dms_list_add_tail(&new_res->list_resq, &fc_reslist)) {
                list_bug = true;
            }
        }
    } while (0);

    write_unlock(&fcRes_qlock);

    if (list_bug) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_add_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }
    return new_res;
}

static void free_resource_item (dms_res_item_t *res_item) {
    if (!IS_ERR_OR_NULL(res_item)) {
        discoC_mem_free(res_item->res_response_time);
        discoC_mem_free(res_item);
        res_item = NULL;
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN free null res item\n");
        DMS_WARN_ON(true);
    }
}

int32_t Check_DN_Resources_Is_Available (uint32_t ipaddr, uint32_t port)
{
    dms_res_item_t *res_item;
    long long ret_num_timeout;

    res_item = add_resource_item(ipaddr, port, RES_TYPE_DN, true);
    if (!IS_ERR_OR_NULL(res_item)) {
        if (!res_item->is_available) {
            return RESOURCES_IS_NOT_AVAILABLE;
        }
        ret_num_timeout = atomic64_read(&res_item->number_timeout_occur);
    } else {
        ret_num_timeout = RESOURCES_IS_NOT_AVAILABLE;
    }

    return ret_num_timeout;
}

dms_res_item_t *add_NN_Resource (uint32_t ipaddr, uint32_t port)
{
    dms_res_item_t *res_item;

    res_item = add_resource_item(ipaddr, port, RES_TYPE_NN, true);

    return res_item;
}

void set_res_available (uint32_t ipaddr, uint32_t port, bool is_available)
{
    dms_res_item_t * res_item = NULL;
    int i;

    res_item = find_resource_item(ipaddr, port, false);
    if (!IS_ERR_OR_NULL(res_item)) {
        res_item->is_available = is_available;
        if (!is_available) {
            res_item->total_response_time = 0;
            for (i = 0; i < res_item->num_of_req_time_in_count; i++) {
                res_item->res_response_time[i] = 0L;
            }
            //Lego: 20130726, reset the number of timeout occurred
            atomic64_set(&res_item->number_timeout_occur, 0);
        }
    }
}

void add_request_timeout_record (uint32_t ipaddr, uint32_t port)
{
    dms_res_item_t *res_item = NULL;
    res_item = find_resource_item(ipaddr, port, false);
    if (!IS_ERR_OR_NULL(res_item)) {
        atomic64_inc(&res_item->number_timeout_occur);
    }
}

void dec_request_timeout_record (uint32_t ipaddr, uint32_t port)
{
    dms_res_item_t *res_item = NULL;
    res_item = find_resource_item(ipaddr, port, false);
    if (!IS_ERR_OR_NULL(res_item)) {
        if (atomic64_read(&res_item->number_timeout_occur) > 1) {
            atomic64_dec(&res_item->number_timeout_occur);
        }
    }
}

int32_t get_res_avg_resp_time (dms_res_item_t *res_item)
{
    int32_t avg_res = 0;

    if (IS_ERR_OR_NULL(res_item)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null res when get res avg response time\n");
        DMS_WARN_ON(true);
        return avg_res;
    }

    read_lock(&res_item->res_lock);

    if (res_item->res_state == RESOURCES_STAT_DATA_READY &&
            res_item->is_available) {
        avg_res = do_divide(res_item->total_response_time,
                res_item->num_of_req_time_in_count);
        if (avg_res > dms_client_config->ioreq_timeout || avg_res < 0) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO avg response time %llu\n",
                    res_item->total_response_time);
            print_resource(LOG_LVL_WARN, res_item);
        }

    } else {
        if (!res_item->is_available) {
            avg_res = DMS_RESOURCE_NOT_AVAILABLE;
        } else {
            avg_res = DMS_RESOURCE_NOT_READY;
        }
    }

    read_unlock(&res_item->res_lock);

    return avg_res;
}

int32_t add_req_response_time (uint64_t ts_start, uint64_t ts_end, uint64_t w_memack_ts_end,
        res_type_t type, uint16_t sub_type,
        uint32_t ipaddr, uint32_t port,
        uint64_t num_of_bytes)
{
    uint64_t time_period, tmp_time_period;
    long long time_period_u_sec, time_period_u_sec_w_memack;
    dms_res_item_t *res_item;
    int8_t hname[IPV4ADDR_NM_LEN];

    time_period = 0;
    tmp_time_period = 0;
    time_period_u_sec = 0;
    time_period_u_sec_w_memack = 0;
    res_item = NULL;

    if (time_after64(ts_start, ts_end)) {
        time_period = 0L;
        time_period_u_sec = -1L;
    } else {
        time_period = jiffies_to_msecs(ts_end - ts_start) / 1000L;
        time_period_u_sec = jiffies_to_usecs(ts_end - ts_start);
    }

    if (time_after64(ts_start, w_memack_ts_end)) {
        time_period_u_sec_w_memack = -1L;
    } else {
        time_period_u_sec_w_memack =
                jiffies_to_usecs(w_memack_ts_end - ts_start);
    }

    res_item = add_resource_item(ipaddr, port, type, true);
    if (unlikely(IS_ERR_OR_NULL(res_item))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Cannot add new res when add req resp time\n");
        DMS_WARN_ON(true);
        return 0;
    }

    if (!res_item->is_available) {
        return 0;
    }

    if (time_period > dms_client_config->reqrsp_time_thres_show) {
        ccma_inet_aton(ipaddr, hname, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO PERF %s:%d response slow %llu seconds\n",
                hname, port, time_period);
    }

    write_lock(&res_item->res_lock);
    res_item->last_modified_index++;
    if (res_item->res_state == RESOURCES_STAT_DATA_COLLECTING &&
        res_item->last_modified_index == res_item->num_of_req_time_in_count) {
        res_item->res_state = RESOURCES_STAT_DATA_READY;
    }

    if (res_item->last_modified_index == res_item->num_of_req_time_in_count) {
        res_item->last_modified_index = 0;
    }

    res_item->total_response_time += time_period;
    res_item->total_response_time -=
            res_item->res_response_time[res_item->last_modified_index];
    res_item->res_response_time[res_item->last_modified_index] = time_period;

    if (dms_client_config->show_performance_data) {
        if (time_period_u_sec >= 0) {
            res_item->Total_process_time = res_item->Total_process_time
                    + time_period_u_sec;
            if (res_item->Total_process_time < 0) {
                //Overflow, clean performance data and re-calculate
                init_res_perf_data(res_item);
                res_item->Total_process_time = time_period_u_sec;
            }

            atomic64_inc(&res_item->Total_process_cnt);

            if (type == RES_TYPE_NN) {
                res_item->NN_Process_Sample_End_Time = ts_end;
                res_item->NN_Process_Sample_Bytes += (num_of_bytes >> BIT_LEN_KBYTES);
                atomic64_inc(&res_item->NN_Process_Sample_CNT);
                if (atomic64_read(&res_item->Total_process_cnt) %
                        MAX_NUM_OF_SAMPLE == 0) {
                    res_item->NN_Process_Sample_Start_Time = ts_start;
                    res_item->NN_Process_Sample_Bytes = (num_of_bytes >> BIT_LEN_KBYTES);
                    atomic64_set(&res_item->NN_Process_Sample_CNT, 1);
                } else if (atomic64_read(&res_item->Total_process_cnt) %
                        MAX_NUM_OF_SAMPLE == (MAX_NUM_OF_SAMPLE - 1)) {
                    if (res_item->NN_Process_Sample_End_Time >
                        res_item->NN_Process_Sample_Start_Time) {
                        tmp_time_period =
                            jiffies_to_msecs(res_item->NN_Process_Sample_End_Time -
                                         res_item->NN_Process_Sample_Start_Time);
                    } else {
                        tmp_time_period = 0;
                    }

                    if (tmp_time_period > 0) {
                        res_item->NN_Process_Sample_IOPS =
                                (MAX_NUM_OF_SAMPLE*1000L) / tmp_time_period;
                        res_item->NN_Process_Sample_TP =
                                (res_item->NN_Process_Sample_Bytes*1000L) /
                                tmp_time_period;
                    }  else {
                        res_item->NN_Process_Sample_IOPS = 0;
                        res_item->NN_Process_Sample_TP = 0;
                    }
                }

                if (sub_type == NN_RESPONSE_SUB_TYPE_QUERY_METADATA) {
                    res_item->NN_Process_Time_Query_Metadata +=
                            time_period_u_sec;
                    atomic64_inc(&res_item->NN_Process_CNT_Query_Metadata);
                } else if (sub_type == NN_RESPONSE_SUB_TYPE_ALLOC_METADATA_NoOld) {
                    res_item->NN_Process_Time_Alloc_Metadata_And_NoOld +=
                            time_period_u_sec;
                    atomic64_inc(&res_item->NN_Process_CNT_Alloc_Metadata_And_NoOld);
                } else if (sub_type == NN_RESPONSE_SUB_TYPE_ALLOC_METADATA_WithOld) {
                    res_item->NN_Process_Time_Alloc_Metadata_And_WithOld +=
                            time_period_u_sec;
                    atomic64_inc(&res_item->NN_Process_CNT_Alloc_Metadata_And_WithOld);
                } else {
                    dms_printk(LOG_LVL_INFO,
                            "DMSC INFO No such sub_type for NN %d\n", sub_type);
                }
            } else if (type == RES_TYPE_DN) {
                if (sub_type == DN_RESPONSE_SUB_TYPE_Read) {
                    res_item->DN_Process_Time_Read += time_period_u_sec;
                    atomic64_inc(&res_item->DN_Process_CNT_Read);
                    res_item->DN_Process_Total_Read_Bytes+= (num_of_bytes >> BIT_LEN_KBYTES);
                    atomic64_inc(&res_item->DN_Process_Sample_Read_CNT);

                    res_item->DN_Process_Sample_Read_End_Time = ts_end;
                    res_item->DN_Process_Sample_Read_Bytes += (num_of_bytes >> BIT_LEN_KBYTES);
                    if (atomic64_read(&res_item->DN_Process_CNT_Read) %
                            MAX_NUM_OF_SAMPLE == 0) {
                        res_item->DN_Process_Sample_Read_Start_Time = ts_start;
                        res_item->DN_Process_Sample_Read_Bytes = (num_of_bytes >> BIT_LEN_KBYTES);
                        atomic64_set(&res_item->DN_Process_Sample_Read_CNT, 1);
                    } else if (atomic64_read(&res_item->DN_Process_CNT_Read) %
                            MAX_NUM_OF_SAMPLE == (MAX_NUM_OF_SAMPLE - 1)) {
                        if (res_item->DN_Process_Sample_Read_End_Time >
                                res_item->DN_Process_Sample_Read_Start_Time) {
                            tmp_time_period =
                                    jiffies_to_msecs(res_item->DN_Process_Sample_Read_End_Time -
                                    res_item->DN_Process_Sample_Read_Start_Time);
                        } else {
                            tmp_time_period = 0;
                        }

                        if (tmp_time_period > 0) {
                            res_item->DN_Process_Sample_Read_IOPS =
                                    (MAX_NUM_OF_SAMPLE*1000L) / tmp_time_period;
                            res_item->DN_Process_Sample_Read_TP =
                                    (res_item->DN_Process_Sample_Read_Bytes*1000L) /
                                    tmp_time_period;
                        }  else {
                            res_item->DN_Process_Sample_Read_IOPS = 0;
                            res_item->DN_Process_Sample_Read_TP = 0;
                        }
                    }
                } else if (sub_type == DN_RESPONSE_SUB_TYPE_Write) {
                    if (time_period_u_sec != -1L && time_period_u_sec_w_memack != -1L) {
                        res_item->DN_Process_Time_Write += time_period_u_sec;
                        res_item->DN_Process_Time_Write_MemAck +=
                                time_period_u_sec_w_memack;

                        atomic64_inc(&res_item->DN_Process_CNT_Write);
                        res_item->DN_Process_Total_Write_Bytes+=  (num_of_bytes/1024L);
                        atomic64_inc(&res_item->DN_Process_Sample_Write_CNT);

                        res_item->DN_Process_Sample_Write_End_Time = ts_end;
                        res_item->DN_Process_Sample_Write_Bytes += (num_of_bytes >> BIT_LEN_KBYTES);
                        if (atomic64_read(&res_item->DN_Process_CNT_Write) %
                                MAX_NUM_OF_SAMPLE == 0) {
                            res_item->DN_Process_Sample_Write_Start_Time = ts_start;
                            res_item->DN_Process_Sample_Write_Bytes = (num_of_bytes >> BIT_LEN_KBYTES);
                            atomic64_set(&res_item->DN_Process_Sample_Write_CNT, 1);
                        } else if (atomic64_read(&res_item->DN_Process_CNT_Write) %
                                MAX_NUM_OF_SAMPLE == (MAX_NUM_OF_SAMPLE - 1)) {
                            if (res_item->DN_Process_Sample_Write_End_Time >
                                    res_item->DN_Process_Sample_Write_Start_Time) {
                                tmp_time_period =
                                    jiffies_to_msecs(res_item->DN_Process_Sample_Write_End_Time -
                                    res_item->DN_Process_Sample_Write_Start_Time);
                            } else {
                                tmp_time_period = 0;
                            }

                            if (tmp_time_period > 0) {
                                res_item->DN_Process_Sample_Write_IOPS =
                                        (MAX_NUM_OF_SAMPLE*1000L) / tmp_time_period;
                                res_item->DN_Process_Sample_Write_TP =
                                        (res_item->DN_Process_Sample_Write_Bytes*1000L) /
                                        tmp_time_period;
                            } else {
                                res_item->DN_Process_Sample_Write_IOPS = 0;
                                res_item->DN_Process_Sample_Write_TP = 0;
                            }
                        }
                    }
                } else {
                    dms_printk(LOG_LVL_INFO,
                            "DMSC INFO No such sub_type for DN %d\n", sub_type);
                }
            }
        }
    }

    write_unlock(&res_item->res_lock);

    return 0;
}

void update_res_rate_timeout (dms_res_item_t *res_item)
{
    int32_t min_max, min_rate;

    if (IS_ERR_OR_NULL(res_item)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN update null res when timeout\n");
        DMS_WARN_ON(true);
        return;
    }

    mutex_lock(&res_item->alloc_res_lock);
    min_max = res_item->fc_curr_max_reqs >= MIN_NN_MAX_REQUESTS_TIMEOUT ?
            res_item->fc_curr_max_reqs >> 1 : MIN_NN_MAX_REQUESTS_TIMEOUT;

    res_item->fc_curr_max_reqs = min_max;

    min_rate = res_item->fc_curr_rate > MIN_NN_FIXED_RATE_TIMEOUT ?
            res_item->fc_curr_rate >> 1 : MIN_NN_FIXED_RATE_TIMEOUT;

    res_item->fc_curr_rate = min_rate;
    mutex_unlock(&res_item->alloc_res_lock);
}

void update_resource_rate (dms_res_item_t *res_item,
        int32_t slow_start_rate, int32_t start_max_reqs,
        int32_t rate_age_time, int32_t curr_rate, int32_t curr_max_reqs)
{
    //Lego: Do we need a lock here?
    int32_t min_max;

    if (unlikely(0 == curr_max_reqs)) {
       dms_printk(LOG_LVL_WARN,
               "DMSC WARN max reqs is 0 from NN, set as %d\n",
               MIN_NN_MAX_REQUESTS_TIMEOUT);
    }

    if ((curr_rate != -1 && curr_rate <= dms_client_config->nnrate_warn) ||
            curr_max_reqs <= dms_client_config->nnmaxreq_warn) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO PERF get low rate from nn (%d, %d)\n",
                curr_rate, curr_max_reqs);
    }

    min_max = curr_max_reqs > 0 ?
            curr_max_reqs : MIN_NN_MAX_REQUESTS_TIMEOUT;
    res_item->fc_slow_start_rate = slow_start_rate;
    res_item->fc_rate_age_time = rate_age_time;
    res_item->fc_curr_rate = curr_rate;
    res_item->fc_curr_max_reqs = min_max;
    res_item->fc_start_max_reqs = start_max_reqs;
}

void show_resource_rate (dms_res_item_t *res_item)
{
    dms_printk(LOG_LVL_INFO,
            "DMSC INFO (slow_s, rate_age, cur_rate, cur_max, start_max) : "
            "(%d, %d, %d, %d, %d)\n", res_item->fc_slow_start_rate,
            res_item->fc_rate_age_time, res_item->fc_curr_rate,
            res_item->fc_curr_max_reqs, res_item->fc_start_max_reqs);
}

void get_nn_res_rate (int32_t *slow_s_rate, int32_t *slow_s_max,
        int32_t *curr_rate, int32_t *curr_max,
        int32_t *alloc_rate, int32_t *alloc_max)
{
    dms_res_item_t *cur, *next;

    read_lock(&fcRes_qlock);

    list_for_each_entry_safe (cur, next, &fc_reslist, list_resq) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN res list broken when get res rate\n");
            DMS_WARN_ON(true);
            break;
        }

        if (RES_TYPE_NN == cur->res_type) {
            *slow_s_rate = cur->fc_slow_start_rate;
            *slow_s_max = cur->fc_start_max_reqs;
            *curr_rate = cur->fc_curr_rate;
            *curr_max = cur->fc_curr_max_reqs;
            *alloc_rate = atomic_read(&cur->alloc_out_res_rate);
            *alloc_max = atomic_read(&cur->alloc_out_res_max);

            break;
        }
    }
    read_unlock(&fcRes_qlock);
}

void update_all_resource_rate (uint64_t curr_time) {
    dms_res_item_t *cur, *next;
    int64_t time_period;

    write_lock(&fcRes_qlock); //TODO we should change to mutex?

    list_for_each_entry_safe (cur, next, &fc_reslist, list_resq) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN res list broken when update res rate\n");
            DMS_WARN_ON(true);
            break;
        }

        atomic_set(&cur->alloc_out_res_rate, 0);

        if (time_after64(curr_time, cur->last_access_time)) {
            time_period =
                    jiffies_to_msecs(curr_time - cur->last_access_time) / 1000L;
            if (time_period > cur->fc_rate_age_time) {
                cur->fc_curr_rate = cur->fc_slow_start_rate;
                cur->fc_curr_max_reqs = cur->fc_start_max_reqs;
            }
        }
    }

    write_unlock(&fcRes_qlock);
}

void dec_res_rate_max_reqs (dms_res_item_t *res_item)
{
    if (IS_ERR_OR_NULL(res_item)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null res when dec max reqs\n");
        DMS_WARN_ON(true);
        return;
    }

    if (atomic_sub_return(1, &res_item->alloc_out_res_max) <
            res_item->fc_curr_max_reqs) {
        atomic_set(&wait_flag, 0);

        if (waitqueue_active(&max_reqs_wq)) {
            wake_up_interruptible_all(&max_reqs_wq);
        }
    }
}

//#define FC_NO_BLOCK    0
//#define FC_MAX_RES_REQUESTS    -1
//#define    FC_RES_CONNECTION_LOST    -2
static int32_t alloc_res (dms_res_item_t *res_item, uint64_t io_start_time,
                                              int32_t num_reqs_need)
{
    int32_t ret = FC_NO_BLOCK;
    uint64_t curr_time;
    int32_t local_res_max, local_res_rate;

    if (unlikely(IS_ERR_OR_NULL(res_item))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null res when get res\n");
        DMS_WARN_ON(true);
        return ret;
    }

    curr_time = jiffies_64;
    mutex_lock(&res_item->alloc_res_lock);

    local_res_max = atomic_add_return(num_reqs_need,
                                      &res_item->alloc_out_res_max);
    local_res_rate = atomic_add_return(num_reqs_need,
                                      &res_item->alloc_out_res_rate);

    if (local_res_max <= res_item->fc_curr_max_reqs) {
        if (res_item->fc_curr_rate == NN_RATE_NO_LIMIT) {
            ret = FC_NO_BLOCK;
        } else if (local_res_rate < res_item->fc_curr_rate) {
            ret = FC_NO_BLOCK;
        } else {
            if (time_after64(curr_time, io_start_time)) {
                ret = jiffies_to_msecs(curr_time - io_start_time);
                if (ret > 1000) {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN rate ctl sleep over 1 sec\n");
                    ret = 1000;
                }
            } else {
                ret = FC_NO_BLOCK;
            }
        }
    } else {
        ret = FC_MAX_RES_REQUESTS;
        atomic_set(&wait_flag, 1);
    }

    if (ret != FC_NO_BLOCK) {
        atomic_sub(num_reqs_need, &res_item->alloc_out_res_max);
        atomic_sub(num_reqs_need, &res_item->alloc_out_res_rate);
    } else {
        res_item->last_access_time = jiffies_64;
    }

    mutex_unlock(&res_item->alloc_res_lock);

    return ret;
}

void alloc_res_no_fc (dms_res_item_t *res_item, int32_t num_reqs_need) {
    if (unlikely(IS_ERR_OR_NULL(res_item))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null group when get res no fc\n");
        DMS_WARN_ON(true);
        return;
    }

    mutex_lock(&res_item->alloc_res_lock);
    atomic_add_return(num_reqs_need, &res_item->alloc_out_res_max);
    atomic_add_return(num_reqs_need, &res_item->alloc_out_res_rate);
    mutex_unlock(&res_item->alloc_res_lock);
}

/*
 *
 * TODO: Combine should_trigger_fc and do_flow_control
 * ret : 0 , don't perform fc
 * ret : > 0, encounter fc rate limitation, should sleep
 * ret : -1, encounter fc max requests, should be sleep and wait to be woke up
 * ret : -2, nn un-available , sleep for a while
 *
 * TODO: How to handle if max num of req < need_nn_res
 *                     if rate < need_nn_res
 *
 */
int32_t should_trigger_fc (dms_res_item_t *res_item, int32_t need_nn_res,
    int64_t io_start_time)
{
    int32_t ret;

    ret = FC_NO_BLOCK;

    if (need_nn_res) {
        if (!IS_ERR_OR_NULL(res_item)) {
            if (res_item->is_available) {
                ret = alloc_res(res_item, io_start_time, need_nn_res);
            } else {
                ret = FC_RES_CONNECTION_LOST;
            }
        } else {
            ret = FC_NO_BLOCK;
        }
    }

    return ret;
}

void do_flow_control (int16_t STOP_TYPE, int32_t wait_time)
{
    unsigned long ret_msleep;

    switch (STOP_TYPE) {
    case WAIT_TYPE_FIX_TIME:
        ret_msleep = msleep_interruptible(wait_time);
        if (ret_msleep != 0) {
            msleep(ret_msleep);
        }
        break;
    case WAIT_TYPE_ALL_IO_DONE:
        //TODO: Not implement now
        break;
    case WAIT_TYPE_WAIT_Q:
        wait_event_interruptible_timeout(max_reqs_wq,
                !atomic_read(&wait_flag), wait_time);
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknown do fc type\n");
        DMS_WARN_ON(true);
        break;
    }
}

static int32_t release_all_resource_items (void)
{
    dms_res_item_t *cur, *next, *tmp;

REL_RES_ITEM:
    write_lock(&fcRes_qlock);
    tmp = NULL;
    list_for_each_entry_safe (cur, next, &fc_reslist, list_resq) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN res list broken when free all\n");
            DMS_WARN_ON(true);
            tmp = NULL;
        }

        if (dms_list_del(&cur->list_resq)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
        }

        tmp = cur;

        break;
    }

    write_unlock(&fcRes_qlock);

    if (!IS_ERR_OR_NULL(tmp)) {
        free_resource_item(tmp);
        goto REL_RES_ITEM;
    }

    return 0;
}

void init_flow_control (void)
{
    rwlock_init(&fcRes_qlock);
    INIT_LIST_HEAD(&fc_reslist);
    g_res_id = 0;

    init_waitqueue_head(&max_reqs_wq);
    atomic_set(&wait_flag, 0);
}

void release_flow_control (void)
{
    release_all_resource_items();
}

//caller make sure group is not NULL
static int32_t get_slowest_res_resp_time (dms_fc_group_t *group, uint16_t type)
{
    int32_t i, slowest_res = -1, res;

    if (type == RES_TYPE_NN) {
        for (i = 0; i < MAX_NN_RES_IN_SPEED_GROUP; i++) {
            if (group->nn_speed_group[i] != NULL) {
                res = get_res_avg_resp_time(group->nn_speed_group[i]);
                if (res >= 0) {
                    if (res >= slowest_res) {
                        slowest_res = res;
                    }
                } else if (res == DMS_RESOURCE_NOT_READY) {
                    slowest_res = DMS_RESOURCE_NOT_READY;
                    break;
                }
            }
        }
    } else {
        for (i = 0; i < MAX_DN_RES_IN_SPEED_GROUP; i++) {
            if (group->dn_speed_group[i] != NULL) {
                res = get_res_avg_resp_time(group->dn_speed_group[i]);

                if (res >= 0) {
                    if (res >= slowest_res) {
                        slowest_res = res;
                    }
                } else if (res == DMS_RESOURCE_NOT_READY) {
                    slowest_res = DMS_RESOURCE_NOT_READY;
                    break;
                }
            }
        }
    }

    return slowest_res;
}

int32_t get_fc_resp_time (dms_fc_group_t *group, res_type_t type)
{
    int32_t avg_response_time;

    avg_response_time = 0;

    if (IS_ERR_OR_NULL(group)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null group when get res response time\n");
        DMS_WARN_ON(true);
        return avg_response_time;
    }

    if (type == RES_TYPE_DN) {
        avg_response_time = get_slowest_res_resp_time(group, RES_TYPE_DN);
    } else {
        avg_response_time = get_slowest_res_resp_time(group, RES_TYPE_NN);
    }

    if (avg_response_time == DMS_RESOURCE_NOT_READY) {
        avg_response_time = 0;
    } else if (avg_response_time == DMS_RESOURCE_NOT_AVAILABLE) {
        avg_response_time = 0;
    }

    return avg_response_time;
}

static int32_t add_res_to_fc_group (dms_res_item_t *res_item,
                                    dms_fc_group_t *group, uint16_t type)
{
    int32_t i, add_index, found_flag;

    if (unlikely(IS_ERR_OR_NULL(group))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN add to null fc group\n");
        DMS_WARN_ON(true);
        return -1;
    }

    add_index = -1;
    found_flag = 0;

    mutex_lock(&group->group_lock);
    if (type == RES_TYPE_NN) {
        for (i = 0; i < MAX_NN_RES_IN_SPEED_GROUP; i++) {
            if (res_item == group->nn_speed_group[i]) {
                found_flag = 1;
                break;
            }

            if (add_index == -1 && group->nn_speed_group[i] == NULL) {
                add_index = i;
            }
        }

        if (found_flag == 0) {
            if (add_index > -1) {
                if (group->nn_speed_group[add_index] != NULL) {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN NN_RES add_index %d is not NULL\n",
                            add_index);
                } else {
                    group->nn_speed_group[add_index] = res_item;
                }
            } else {
               dms_printk(LOG_LVL_INFO, "DMSC INFO Fail to find "
                          "a empty slot of volume %llu for adding resource item "
                          "with type %d\n",
                          group->volume_id, type);
            }
        }
    } else {
        for (i = 0; i < MAX_DN_RES_IN_SPEED_GROUP; i++) {
            if (res_item == group->dn_speed_group[i]) {
                found_flag = 1;
                break;
            }

            if (add_index == -1 && group->dn_speed_group[i] == NULL) {
                add_index = i;
            }
        }

        if (found_flag == 0) {
            if (add_index > -1) {
                if (group->dn_speed_group[add_index] != NULL) {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN DN_RES add_index %d is not NULL\n",
                            add_index);
                } else {
                    group->dn_speed_group[add_index] = res_item;
                }
            } else {
                dms_printk(LOG_LVL_INFO, "DMSC INFO Fail to find a empty "
                           "slot of volume %llu for adding resource "
                           "item with type %d\n",
                           group->volume_id, type);
            }
        }
    }

    mutex_unlock(&group->group_lock);

    return 0;
}

int32_t add_res_to_fc_group_by_hostname (uint32_t ipaddr, uint32_t port,
        dms_fc_group_t *group, uint16_t type) {
    dms_res_item_t *res_item;

    res_item = find_resource_item(ipaddr, port, true);

    if (unlikely(IS_ERR_OR_NULL(res_item))) {
        return -1;
    }

    return add_res_to_fc_group(res_item, group, type);
}

void release_fc_group (dms_fc_group_t *vol_fc_group)
{
    int i;

    if (IS_ERR_OR_NULL(vol_fc_group)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN try to free a null volume fc group\n");
        DMS_WARN_ON(true);
        return;
    }

    for (i = 0; i < MAX_NN_RES_IN_SPEED_GROUP; i++) {
        vol_fc_group->nn_speed_group[i] = NULL;
    }

    for (i = 0; i < MAX_DN_RES_IN_SPEED_GROUP; i++) {
        vol_fc_group->dn_speed_group[i] = NULL;
    }

    discoC_mem_free(vol_fc_group);
    vol_fc_group = NULL;
}

dms_fc_group_t *create_fc_group (uint64_t volumeID)
{
    int32_t i;

    dms_fc_group_t *new_group =
            (dms_fc_group_t *)discoC_mem_alloc(sizeof(dms_fc_group_t),
                                 GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(new_group)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Cannot create group for volume %llu\n",
                volumeID);
        DMS_WARN_ON(true);
        return NULL;
    }

    new_group->volume_id = volumeID;

    //Lego: If we can generate a unique id for each NN and DN, we should change
    //      array to hash
    for (i = 0; i < MAX_NN_RES_IN_SPEED_GROUP; i++) {
        new_group->nn_speed_group[i] = NULL;
    }

    for (i = 0; i < MAX_DN_RES_IN_SPEED_GROUP; i++) {
        new_group->dn_speed_group[i] = NULL;
    }

    mutex_init(&new_group->group_lock);
    return new_group;
}

uint32_t get_vol_active_dn (int8_t *buffer, dms_fc_group_t *group)
{
    int32_t cnt, i;
    int8_t *ptr;
    dms_res_item_t *resp;

    cnt = 0;
    ptr = buffer;
    read_lock(&fcRes_qlock);
    for (i = 0; i < MAX_DN_RES_IN_SPEED_GROUP; i++) {
        if (IS_ERR_OR_NULL(group->dn_speed_group[i])) {
            continue;
        }

        resp = group->dn_speed_group[i];

        *((int32_t *)ptr) = resp->ipaddr;
        ptr += sizeof(int32_t);
        *((int32_t *)ptr) = resp->port;
        ptr += sizeof(int32_t);

        cnt++;
    }

    read_unlock(&fcRes_qlock);

    return cnt;
}

uint32_t get_dn_latency_info (int8_t *buffer)
{
    dms_res_item_t *cur, *next;
    int32_t dn_latency_all, dn_latency_read, dn_latency_write;
    uint32_t dn_latency_write_memack, cnt;
    int8_t *ptr;

    ptr = buffer;
    cnt = 0;
    read_lock(&fcRes_qlock);

    list_for_each_entry_safe(cur, next, &fc_reslist, list_resq) {
        if (cur->res_type != RES_TYPE_DN) {
            continue;
        }

        Get_DN_Avg_Latency_Time(cur, &dn_latency_all,
                &dn_latency_read, &dn_latency_write,
                &dn_latency_write_memack);
        *((int32_t *)ptr) = cur->ipaddr;
        ptr += sizeof(int32_t);

        *((int32_t *)ptr) = cur->port;
        ptr += sizeof(int32_t);

        *((int32_t *)ptr) = dn_latency_write;
        ptr += sizeof(int32_t);

        *((int32_t *)ptr) = dn_latency_write;
        ptr += sizeof(int32_t);

        *((int32_t *)ptr) = dn_latency_read;
        ptr += sizeof(int32_t);

        *((int32_t *)ptr) = dn_latency_read;
        ptr += sizeof(int32_t);

        cnt++;
    }
    read_unlock(&fcRes_qlock);

    return cnt;
}

uint32_t show_res_info (int8_t *buffer, int32_t buff_len)
{
    int32_t len;
    dms_res_item_t *cur, *next;
    int32_t nn_latency_all, nn_latency_q_metadata, nn_latency_alloc_no_old;
    int32_t nn_latency_alloc_with_old;
    int32_t dn_latency_all, dn_latency_read, dn_latency_write;
    int32_t dn_latency_write_memack;
    int32_t nn_old_tp, nn_old_iops, nn_curr_tp, nn_curr_iops;
    int32_t dn_old_read_tp, dn_old_read_iops, dn_old_write_tp, dn_old_write_iops;
    int32_t dn_curr_read_tp, dn_curr_read_iops, dn_curr_write_tp, dn_curr_write_iops;
    int32_t limit;
    int8_t hostname[IPV4ADDR_NM_LEN];

    len = 0;
    limit = buff_len - 80;
    len += sprintf(buffer + len,
            "**************** Memory information ****************\n");
    len += sprintf(buffer + len, "System Free Pages: %ld\n",
            (unsigned long)nr_free_pages());
    len += sprintf(buffer + len, "Pre-allocate memory buffer:\n");

    len += sprintf(buffer+len, "Current memory buffer usage:\n");
    len += sprintf(buffer+len, "io_req/radix_tree/tree_leaf "
            "memory usage: %d %d %d\n",
            get_mem_usage_cnt(MEM_Buf_IOReq),
            atomic_read(&dms_node_count),
            cacheMgr_get_num_treeleaf());

    if (len >= limit) {
        return len;
    }

    if (dms_client_config->show_performance_data) {
        len += sprintf(buffer + len, "\n**************** Requests Performance "
                "information ****************\n");
        read_lock(&fcRes_qlock);

        list_for_each_entry_safe(cur, next, &fc_reslist, list_resq) {
            if (IS_ERR_OR_NULL(cur)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN conn list broken when "
                        "show info\n");
                DMS_WARN_ON(true);
                break;
            }

            if (cur->res_type == RES_TYPE_NN) {
                ccma_inet_aton(cur->ipaddr, hostname, IPV4ADDR_NM_LEN);
                len += sprintf(buffer + len, "NN: %s:%d is avail: %d "
                        "Avg Response time: %d (seconds)\n",
                        hostname, cur->port, cur->is_available,
                        get_res_avg_resp_time(cur));
                Get_NN_Avg_Latency_Time(cur, &nn_latency_all,
                        &nn_latency_q_metadata, &nn_latency_alloc_no_old,
                        &nn_latency_alloc_with_old);
                Get_NN_Sample_Performance(cur, &nn_old_tp, &nn_old_iops,
                        &nn_curr_tp, &nn_curr_iops);
                len += sprintf(buffer + len, "All Req CNT: Overall: %ld \t "
                        "QueryMetadata: %ld \t Alloc_No_Old: %ld \t "
                        "Alloc_With_Old: %ld\n",
                        atomic64_read(&cur->Total_process_cnt),
                        atomic64_read(&cur->NN_Process_CNT_Query_Metadata),
                        atomic64_read(&cur->NN_Process_CNT_Alloc_Metadata_And_NoOld),
                        atomic64_read(&cur->NN_Process_CNT_Alloc_Metadata_And_WithOld));
                len += sprintf(buffer + len, "Avg Latency: Overall: %d \t "
                        "QueryMetadata: %d \t Alloc_No_Old: %d \t "
                        "Alloc_With_Old: %d (usecs)\n", nn_latency_all,
                        nn_latency_q_metadata, nn_latency_alloc_no_old,
                        nn_latency_alloc_with_old);
                len += sprintf(buffer + len, "Sample Performance: Pre %d "
                        "Reqs: TP %d (kbytes/msecs) IOPS: %d (cnt/msecs) \t "
                        "Current: TP %d (kbytes/msecs) IOPS: %d (cnt/msecs)\n\n",
                        MAX_NUM_OF_SAMPLE, nn_old_tp, nn_old_iops,
                        nn_curr_tp, nn_curr_iops);
            } else {
                ccma_inet_aton(cur->ipaddr, hostname, IPV4ADDR_NM_LEN);
                len += sprintf(buffer + len, "DN: %s:%d is avail: %d "
                        "Avg Response time: %d (seconds)\n",
                        hostname, cur->port, cur->is_available,
                        get_res_avg_resp_time(cur));
                Get_DN_Avg_Latency_Time(cur, &dn_latency_all,
                        &dn_latency_read, &dn_latency_write,
                        &dn_latency_write_memack);
                Get_DN_Sample_Performance(cur, &dn_old_read_tp,
                        &dn_old_read_iops, &dn_old_write_tp,
                        &dn_old_write_iops, &dn_curr_read_tp,
                        &dn_curr_read_iops, &dn_curr_write_tp,
                        &dn_curr_write_iops);
                len += sprintf(buffer + len, "All Req CNT: Overall: %ld \t "
                        "Read: %ld \t Write: %ld\n",
                        atomic64_read(&cur->Total_process_cnt),
                        atomic64_read(&cur->DN_Process_CNT_Read),
                        atomic64_read(&cur->DN_Process_CNT_Write));
                len += sprintf(buffer + len, "Avg Latency: Overall: %d \t "
                        "Read: %d \t Write: %d (usecs) \t "
                        "Write MemAck: %d (usecs)\n", dn_latency_all,
                        dn_latency_read, dn_latency_write,
                        dn_latency_write_memack);
                len += sprintf(buffer + len, "Sample Performance: Pre %d "
                        "Reqs: Read TP %d (kbytes/msecs) "
                        "Read IOPS: %d (cnt/msecs) \t "
                        "Current: Read TP %d (kbytes/msecs) "
                        "Read IOPS: %d (cnt/msecs)\n", MAX_NUM_OF_SAMPLE,
                        dn_old_read_tp, dn_old_read_iops,
                        dn_curr_read_tp, dn_curr_read_iops);
                len += sprintf(buffer + len, "Sample Performance: Pre %d "
                        "Reqs: Write TP %d  (kbytes/msecs) "
                        "Write IOPS: %d (cnt/msecs) \t "
                        "Current: Write TP %d (kbytes/msecs) "
                        "Write IOPS: %d (cnt/msecs)\n\n", MAX_NUM_OF_SAMPLE,
                        dn_old_write_tp, dn_old_write_iops,
                        dn_curr_write_tp, dn_curr_write_iops);
            }

            if (len >= limit) {
                break;
            }
        }
        read_unlock(&fcRes_qlock);
    }

    return len;
}
