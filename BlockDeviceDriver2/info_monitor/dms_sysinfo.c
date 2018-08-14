#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/blkdev.h>
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
#include "../vdisk_manager/volume_manager.h"
#include "dms_sysinfo_private.h"
#include "dms_info.h"
#include "../connection_manager/conn_manager_export.h"
#include "../config/dmsc_config.h"
#include "../flowcontrol/FlowControl.h"
#include "../drv_fsm.h"
#include "../drv_main.h"

static int8_t *str_buf;

static void *monitor_seq_start (struct seq_file *s, loff_t *pos)
{
    if (*pos >= MAX_LINE) {
        return NULL;
    }
    ++(*pos);
    return (void *)((uint64_t) *pos);
}

static void *monitor_seq_next (struct seq_file *s, void *p, loff_t *pos)
{
      ++(*pos);
      if (*pos >= MAX_LINE) {
          return NULL;
      }
      return (void *)*pos;
}

static void monitor_seq_stop (struct seq_file *s, void *p)
{}

static bool is_enable_show_performance_data (void)
{
    return dms_client_config->show_performance_data;
}

static int8_t *get_configuration (int8_t *buf)
{
    sprintf(buf, "%d %d %d %d %d %d %d %d %d %llu %llu %d %d %d %d",
            is_enable_show_performance_data(),
            dms_client_config->enable_err_inject_test,
            dms_client_config->enable_fc,
            dms_client_config->enable_read_balance,
            dms_client_config->enable_metadata_cache,
            dms_client_config->enable_share_volume,
            dms_client_config->fp_method,
            dms_client_config->dms_RR_enabled,
            dms_client_config->high_resp_write_io,
            dms_client_config->max_meta_cache_sizes,
            dms_client_config->max_cap_cache_all_hit,
            dms_client_config->CommitUser_min_DiskAck,
            dms_client_config->ioreq_timeout,
            dms_client_config->log_level,
            dms_client_config->dms_socket_keepalive_interval);

    return buf;
}

static int32_t get_dms_lb_size (void)
{
    return DMS_LB_SIZE;
}

static int8_t *get_gen_disk (int8_t *buf)
{
    sprintf(buf,"%d %d %d",
               MAX_SECTS_PER_RQ,
               KERNEL_SECTOR_SIZE,
               get_dms_lb_size());

    return buf;
}

static uint64_t get_total_io_req_buf_size (void)
{
    return get_mem_pool_size(MEM_Buf_IOReq) * get_mem_item_size(MEM_Buf_IOReq);
}

static int8_t *get_total_mem (int8_t *buf)
{
    /* Total Memoy Buf Space */
    sprintf(buf, "%llu",
            get_total_io_req_buf_size());

    return buf;
}

static int8_t *get_used_mem (int8_t *buf)
{
    /* Current Memory Usage */
    sprintf(buf, "%d ",
            get_mem_usage_cnt (MEM_Buf_IOReq));

/*            atomic_read(&iorreq_counter_atomic),
            atomic_read(&copypayload_counter_atomic), 
            atomic_read(&nnreq_counter_atomic),
            atomic_read(&fdn_counter_atomic),
            atomic_read(&locatedreq_counter_atomic), 
            atomic_read(&dnreq_counter_atomic), 
            atomic_read(&wparam_counter_atomic), 
            atomic_read(&packet_counter_atomic), 
            atomic_read(&read_buffer_counter_atomic), 
            atomic_read(&dms_node_count));*/

    return buf;
}

static int32_t get_conn_latency_throughput (uint16_t res_type)
{
   /* switch (res_type) {
    case RES_TYPE_NN:
        seq_print(s, "[%s,%d,%d,%ld,%ld,%ld,%ld,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        );
        break;
    case RES_TYPE_DN:
        seq_print(s, "[%s,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%d,%d,%d]",
        );
        break;
    default:
        break;
    }
    */
    return 1;
}

static int8_t *get_resource_state (int8_t *buf)
{
    dms_res_item_t *cur, *next;
    int32_t len = 0, cnt;
    int8_t hname[IPV4ADDR_NM_LEN];

    if (!is_enable_show_performance_data()) {
        return buf;
    }

    cnt = 0;

    read_lock(&fcRes_qlock);

    list_for_each_entry_safe(cur, next, &fc_reslist, list_resq) {
        if (cur == NULL) {
           continue;
        }

        cnt++;
    }
    len += sprintf(buf + len, "%d ", cnt);

    list_for_each_entry_safe(cur, next, &fc_reslist, list_resq) {
        if (cur == NULL) {
            continue;
        }

        ccma_inet_aton(cur->ipaddr, hname, IPV4ADDR_NM_LEN);
        len += sprintf(buf + len, "%d(%s:%d)",
                get_conn_latency_throughput(cur->res_type),
                                       hname, cur->port);
    }

    read_unlock(&fcRes_qlock);

    return buf;
}

static uint64_t divide_from (uint64_t dividend, uint64_t divisor)
{
    if (divisor <= 0) {
        return 0;
    }
    return dividend / divisor;
}

static int8_t *get_thread_performance_data (int8_t *buf, int32_t thread_type)
{
    uint64_t thread_total_time, thread_total_cnt, thread_total_period_time;
    uint64_t thread_sample_start_time, thread_sample_end_time = 0;
    uint64_t thread_sample_bytes, thread_sample_cnt, thread_prev_iops;
    uint64_t thread_prev_tp;
    uint64_t thread_sub_0_total_time, thread_sub_0_total_cnt;
    uint64_t thread_sub_1_total_time, thread_sub_1_total_cnt;
    uint64_t thread_avg_latency;
    uint64_t thread_sub_1_avg_latency, thread_sub_0_avg_latency;
    uint64_t thread_curr_iops = 0, thread_curr_tp = 0;
    uint64_t thread_sample_time;

    Get_Thread_Performance_Data(thread_type, 
                                &thread_total_time,
                                &thread_total_cnt,
                                &thread_total_period_time,
                                &thread_sample_start_time,
                                &thread_sample_end_time,
                                &thread_sample_bytes,
                                &thread_sample_cnt,
                                &thread_prev_iops,
                                &thread_prev_tp,
                                &thread_sub_0_total_time,
                                &thread_sub_0_total_cnt,
                                &thread_sub_1_total_time,
                                &thread_sub_1_total_cnt);

    thread_avg_latency = divide_from(thread_total_time, thread_total_cnt);
    thread_sub_0_avg_latency = divide_from(thread_sub_0_total_time, thread_sub_0_total_cnt);
    thread_sub_1_avg_latency = divide_from(thread_sub_1_total_time, thread_sub_1_total_cnt);
    
    if (thread_sample_end_time > thread_sample_start_time) {
        thread_sample_time = divide_from(jiffies_to_msecs(
                                         thread_sample_end_time - 
                                         thread_sample_start_time), 1000UL); //+++[Revisit]+++ long?
        thread_curr_iops = divide_from(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = divide_from(thread_sample_bytes, thread_sample_time);
    }
    
    thread_total_period_time = divide_from(thread_total_period_time,
                                           thread_total_cnt);

    sprintf(buf, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu "
                 "%llu %llu %llu",
                 thread_avg_latency,
                 thread_total_time,
                 thread_total_cnt,
                 thread_sub_0_avg_latency,
                 thread_sub_0_total_time,
                 thread_sub_0_total_cnt,
                 thread_sub_1_avg_latency,
                 thread_sub_1_total_time,
                 thread_sub_1_total_cnt,
                 thread_prev_iops,
                 thread_prev_tp,
                 thread_curr_iops,
                 thread_curr_tp,
                 thread_total_period_time);

    return buf;
}

static int8_t *get_thread_performance_info (int8_t *buf)
{

    int32_t i, len, thread_type[] = {THD_T_Main,
            THD_T_NNReq_Worker, THD_T_NNAck_Receivor,
            THD_T_DNReq_Worker, THD_T_DNAck_Receivor,
            THD_T_DNAck_Handler, THD_T_NNCIReport_Worker};
    int32_t buf_len = 0;

    int8_t *tmp_buf = discoC_mem_alloc(PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = sizeof(thread_type) / sizeof(int32_t);
    
    for (i = 0; i < len; i++) {
        memset(tmp_buf, 0, PAGE_SIZE);
        buf_len += sprintf(buf + buf_len, 
                           "%s ",
                           get_thread_performance_data(tmp_buf, 
                                                       thread_type[i]));
   }
   discoC_mem_free(tmp_buf);
   return buf;
}

static int8_t *get_thread_state (int8_t *buf)
{
    int32_t len;
    int8_t *tmp_buf;

    if (!is_enable_show_performance_data()) {
        return buf;
    }

    len = 0;
    tmp_buf = discoC_mem_alloc(PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    sprintf(buf + len, " %s", get_thread_performance_info(tmp_buf));
    discoC_mem_free(tmp_buf);
    return buf;
}

static int8_t *get_discoC_conn_state (int8_t *buf)
{
    int32_t len = 0;

    len += connMgr_dump_connStatus(buf + len, PAGE_SIZE);

    return buf;
}

static int32_t get_vol_number (void)
{
    int32_t cnt = 0;
    volume_device_t *vol;

    vol = NULL;
    while(!IS_ERR_OR_NULL(vol = iterate_all_volumes(vol))) {
        cnt++;
    }
 
    return cnt;
}

static int8_t *get_vol_state (int8_t *buf)
{
    sprintf(buf, "%d", get_vol_number());
    return buf;
}

static int32_t get_read_balance_cnt (void)
{
    return 1;
}

static int32_t get_nn_fc_freq (void)
{
    return 1;
}

static int32_t get_fc_lv (void)
{
    return 1;
}

static int8_t *get_fc_state (int8_t *buf)
{
    sprintf(buf, "%d %d %d",
            get_read_balance_cnt(),
            get_nn_fc_freq(),
            get_fc_lv());

    return buf;
}

static int32_t get_attach_success_cnt (void)
{
    return 1;
}

static int32_t get_attach_fail_cnt (void)
{
    return 1;
}

static int32_t get_detach_success_cnt (void)
{
    return 1;
}

static int32_t get_detach_fail_cnt (void)
{
    return 1;
}

static int32_t get_ts_atdetach (void)
{
    return 1;
}

static int8_t *get_attach_detach_state (int8_t *buf)
{
    sprintf(buf, "%d %d %d %d %d",
            get_attach_success_cnt(),
            get_attach_fail_cnt(),
            get_detach_success_cnt(),
            get_detach_fail_cnt(),
            get_ts_atdetach());

   return buf;
}

static int32_t get_ovw_reset_cmd_cnt (void)
{
    return 1;
}

static int32_t get_reset_cache_cnt (void)
{
    return 1;
}

static int8_t *get_reset_state (int8_t *buf)
{
    sprintf(buf, "%d %d",
            get_ovw_reset_cmd_cnt(),
            get_reset_cache_cnt());

    return buf;
}

static int8_t *get_block_io_cnt (int8_t *buf)
{
    int32_t cnt = 1;
    sprintf(buf, "%d", cnt);

    return buf;
}

#if 0
static void get_volume_info (struct seq_file *s)
{
    int32_t i, max_cache_size, curr_cache_item;
    ccma_drive_statistic_t statistic;
    dms_fc_group_t *vol_fc_group;
    volume_device_t *vol;

    vol = NULL;
    while(!IS_ERR_OR_NULL(vol = iterate_all_volumes(vol))) {
 //TODO:   
        MDMgr_get_volume_cacheSize(vol->vol_id, &max_cache_size, &curr_cache_item);
        statistic = all_drives[i]->statistic;
        vol_fc_group = all_drives[i]->vol_fc_group;

        seq_printf(s, "[%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                vol->disk->disk_name,
                        atomic_read(&statistic.linger_wreq_counter),
                        atomic_read(&statistic.linger_rreq_counter),
                        statistic.commit_error_to_user_counter,
                        atomic_read(&statistic.disk_open_counter),
                        all_drives[i]->io_blocking_state,
                        all_drives[i]->enable_resource_control,
                        get_fc_resp_time(vol_fc_group, RES_TYPE_NN),
                        get_fc_resp_time(vol_fc_group, RES_TYPE_DN),
                        curr_cache_item,
                        max_cache_size);
    }
}
#endif

static int32_t monitor_seq_show (struct seq_file *s, void *v)
{
    int32_t i, len;
    int8_t *(*print_func[])(int8_t *buf) ={get_configuration,
                                          get_gen_disk,
                                          get_total_mem,
                                          get_used_mem,
                                          get_discoC_conn_state,
                                          get_resource_state,
                                          get_thread_state,
                                          get_vol_state,
                                          get_fc_state,
                                          get_attach_detach_state,
                                          get_reset_state,
                                          get_block_io_cnt};

    len = sizeof(print_func) / sizeof(int8_t *);

    for (i = 0; i < len; i++) {
        memset(str_buf, 0, PAGE_SIZE);
        seq_printf(s, "%s ",print_func[i](str_buf));
    }

    seq_printf(s, "\n");

    return 0;
}

static int32_t get_vol_id (struct file *filp)
{
    int8_t local_buf[VOL_ID_LEN];
    int32_t vol_id;

    strncpy(local_buf, DMSC_procfp_dname(filp),
            DMSC_procfp_dname_len(filp));
    local_buf[DMSC_procfp_dname_len(filp)] = '\0';
    sscanf(local_buf, "%d", &vol_id);

    return vol_id;
}

static 
volume_device_t *get_ccma_drive (struct file *filp)
{
//    int8_t local_buf[100];
    int32_t volume_id;
    volume_device_t *cur_drv;

    /*strncpy(local_buf, filp->f_dentry->d_name.name, 
                       filp->f_dentry->d_name.len);

    local_buf[filp->f_dentry->d_name.len] = '\0';

    sscanf(local_buf,"%d",&volume_id);*/

    volume_id = get_vol_id(filp);
    cur_drv = find_volume(volume_id);

    return cur_drv;
}

static int8_t *get_perf_main (perf_main_data_t *pmain, int8_t *buf) {
    uint64_t cnt;
    uint64_t total_process_t, total_commit;

    cnt = atomic64_read(&pmain->total_cnt);
    total_process_t = pmain->total_time;
    total_commit = pmain->total_time_user;
    
    sprintf(buf, "%llu %llu %llu %llu %llu %llu %llu",
            total_process_t,
            cnt,
            divide_from(total_process_t, cnt),
            pmain->total_sectors_done,
            pmain->total_sectors_req,
            total_commit,
            divide_from(total_commit,cnt));

    return buf;
}

static int8_t *get_avg_overall_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_main_data_t *pmain;

    pmain = &stat->perf_io;

    return get_perf_main(pmain, buf);
}

static int8_t *get_avg_wait_ovlp_l (volume_perf_data_t *stat, int8_t *buf)
{
    uint64_t cnt;
    uint64_t total_in_ovlp_t;

    cnt = atomic64_read(&stat->total_in_ovlp_cnt);
    total_in_ovlp_t = stat->total_in_ovlp_time;
    sprintf(buf, "%llu %llu %llu",
            total_in_ovlp_t,
            cnt,
            divide_from(total_in_ovlp_t, cnt));

    return buf;
}

static int8_t *get_avg_read_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_main_data_t *pmain;

    pmain = &stat->perf_read;

    return get_perf_main(pmain, buf);
}

static int8_t *get_perf_min_cm (perf_minor_data_t *pmin, int8_t *buf) {
    uint64_t cnt;
    uint64_t total_t;
    total_t = pmin->total_time;

    cnt = atomic64_read(&pmin->total_cnt);
    sprintf(buf, "%llu %llu %llu %llu %llu",
            cnt,
            total_t,
            divide_from(total_t, cnt),
            pmin->wait_mdata_t,
            pmin->wait_pdata_t);

    return buf;
}

static int8_t *get_perf_min_ch (perf_minor_data_t *pmin, int8_t *buf) {
    uint64_t cnt;
    uint64_t total_t;
    total_t = pmin->total_time;

    cnt = atomic64_read(&pmin->total_cnt);
    sprintf(buf, "%llu,%llu,%llu,%llu",
            divide_from(total_t, cnt),
            total_t,
            cnt,
            pmin->wait_pdata_t);

    return buf;
}

static int8_t *get_avg_read_cache_miss_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_read_cm;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_read_cache_hit_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_read_ch;

    return get_perf_min_ch(pminor, buf);
}

static int8_t *get_avg_write_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_main_data_t *pmain;

    pmain = &stat->perf_write;

    return get_perf_main(pmain, buf);
}

static int8_t *get_avg_fwrite_4a_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_fw4a;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_fwrite_4na_no (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_fw4nano;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_fwrite_4na_o (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_fw4nao;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_owrite_4a_cache_miss_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_ow4a_cm;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_owrite_4a_cache_hit_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_ow4a_ch;

    return get_perf_min_ch(pminor, buf);
}

static int8_t *get_avg_owrite_4na_cache_miss_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_ow4na_cm;

    return get_perf_min_cm(pminor, buf);
}

static int8_t *get_avg_owrite_4na_cache_hit_l (volume_perf_data_t *stat, int8_t *buf)
{
    perf_minor_data_t *pminor;

    pminor = &stat->perf_write_ow4na_ch;

    return get_perf_min_ch(pminor, buf);
}

static int8_t *(*get_avg_func[])(volume_perf_data_t *stat, int8_t *buf) =
         {get_avg_overall_l,
          get_avg_wait_ovlp_l,
          get_avg_read_l,
          get_avg_read_cache_miss_l,
          get_avg_read_cache_hit_l,
          get_avg_write_l,
          get_avg_fwrite_4a_l,
          get_avg_fwrite_4na_no,
          get_avg_fwrite_4na_o,
          get_avg_owrite_4a_cache_miss_l,
          get_avg_owrite_4a_cache_hit_l,
          get_avg_owrite_4na_cache_miss_l,
          get_avg_owrite_4na_cache_hit_l};

/* NOTE: Replace char with int8_t will cause compile warning
 *       sizeof(array type) will works for static alloc
 *       fin can be static only.
 *       fin is indicate the read finish, bcz this function will be called many
 *       time when user issue 1 read op.
 */
static ssize_t 
sys_vol_read (struct file *filp, char __user *buffer,
          size_t count, loff_t *offset)
{
    static int32_t fin = 0;
    int8_t *local_buf, *ptr;
    int32_t read_num, i, len;
    volume_device_t *cur_drv;
    volume_perf_data_t *stat;
    struct mutex *performance_data_lock;
    int8_t buf[100];

    if (fin == 1) {
        fin = 0;
        return fin;
    }

    cur_drv = get_ccma_drive(filp);
    
    if (cur_drv == NULL) {
        return 0;
    }

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    read_num = sprintf(local_buf,"%d %d %d ",
            (int32_t)is_vol_mcache_enable(cur_drv),
            get_volume_replica(cur_drv),
            is_vol_share(cur_drv));

    if (!is_enable_show_performance_data()) {
       goto VOL_R_OUT; 
    }

    stat = &cur_drv->perf_data;
    performance_data_lock = &stat->perf_lock;
    
    //NOTE: sizeof(get_avg_func) only works for static alloc
    len = sizeof(get_avg_func) / sizeof(get_avg_func[0]);
    ptr = local_buf;
    mutex_lock(performance_data_lock);
    
    for (i = 0; i < len; i++) {
        memset(buf, 0, sizeof(buf));
        read_num += sprintf(local_buf + read_num, "%s ",
                get_avg_func[i](stat, buf));
    }

    mutex_unlock(performance_data_lock);

VOL_R_OUT:
    fin = 1;
    local_buf[read_num] = '\n';
    read_num++;
    if(read_num >= count) {
        read_num = count;
    }

    for (i = 0; i < read_num; i++) {
        put_user(local_buf[i], buffer + i);
    }

    discoC_mem_free(local_buf);
    return read_num;
}

static ssize_t 
sys_vol_write (struct file * filp, const char __user *buffer,
           size_t count, loff_t * offset)
{
    return 0;
}

struct seq_operations monitor_seq_ops = {
    .start = monitor_seq_start,
    .stop = monitor_seq_stop,
    .next = monitor_seq_next,
    .show = monitor_seq_show
};

static int32_t monitor_seq_open (struct inode *inode, struct file *file)
{
    return seq_open(file, &monitor_seq_ops);
};

struct file_operations monitor_seq_file_ops = {
    .open = monitor_seq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release
};

static struct file_operations sys_vol_proc_ops = {
    .read    = sys_vol_read,
    .write   = sys_vol_write
};

void create_sys_vol_proc (uint32_t volume_id)
{
    struct proc_dir_entry *vol_proc;
    int8_t buffer[VOL_ID_LEN];

    sprintf(buffer, "%d", volume_id);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    vol_proc = create_proc_entry(buffer, 0, proc_sys_vol_dir);
    if (vol_proc) {
        vol_proc->proc_fops = &sys_vol_proc_ops;
    }
#else
    vol_proc = proc_create(buffer, 0, proc_sys_vol_dir, &sys_vol_proc_ops);
#endif

    create_vol_proc(volume_id);
}

void delete_sys_vol_proc (uint32_t volume_id)
{
    int8_t buffer[VOL_ID_LEN];

    sprintf(buffer, "%d", volume_id);
    remove_proc_entry(buffer, proc_sys_vol_dir);

    delete_vol_proc(volume_id);
}

void init_sys_procfs (void)
{
    proc_ccma = proc_mkdir(NAME_PROC_CCMA, NULL);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_sys_monitor = create_proc_entry(NAME_PROC_SYS_MON, 0, proc_ccma);
    if (proc_sys_monitor) {
        proc_sys_monitor->proc_fops = &monitor_seq_file_ops;
    }
#else
    proc_sys_monitor = proc_create(NAME_PROC_SYS_MON, 0, proc_ccma,
            &monitor_seq_file_ops);
#endif

    proc_sys_vol_dir = proc_mkdir(NAME_PROC_SYS_VOL, proc_ccma);
    str_buf = discoC_mem_alloc(PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);

    init_procfs(proc_ccma);
}

void release_sys_procfs (void)
{
    release_procfs(proc_ccma);

    remove_proc_entry(NAME_PROC_SYS_VOL, proc_ccma);
    remove_proc_entry(NAME_PROC_SYS_MON, proc_ccma);
    remove_proc_entry(NAME_PROC_CCMA, NULL);
    discoC_mem_free(str_buf);
}
