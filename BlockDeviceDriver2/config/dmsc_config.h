/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dmsc_config.h
 *
 */

#ifndef _DMSC_CONFIG_H_
#define _DMSC_CONFIG_H_

#ifndef DMSC_USER_DAEMON

typedef enum {
    fp_disable,
    fp_sha1,
    fp_md5,
} fp_method_t;

typedef struct configuration {
    bool enable_fc;

    bool enable_read_balance;
    uint32_t read_hbs_next_dn;

    bool enable_metadata_cache;
    int64_t max_meta_cache_sizes; //num of dms block
    int64_t max_cap_cache_all_hit;

    int32_t CommitUser_min_DiskAck;

    fp_method_t fp_method;
    bool enable_share_volume;

    bool show_performance_data;
    int32_t log_level;

    int32_t dms_socket_keepalive_interval; //unit second

    int32_t ioreq_timeout;  //unit second
    uint32_t max_io_flush_wait_time;  //unit second

    bool dms_RR_enabled;
    bool high_resp_write_io;
    uint32_t time_wait_other_replica;
    uint64_t time_wait_IOdone_conn_lost;

    int32_t max_nr_io_reqs;

    uint32_t req_merge_wait_intvl;
    uint32_t req_merge_wait_cnt;
    bool req_merge_enable;
    int32_t clientnode_running_mode;
    bool enable_err_inject_test;

    uint32_t reqrsp_time_thres_show;
    uint32_t nnrate_warn;
    uint32_t nnmaxreq_warn;
    uint32_t ior_perf_warn;
    uint32_t req_qdelay_warn;

    uint32_t mds_ipaddr;
    uint32_t mds_ioport;
    int8_t *mds_name;

    bool cLinux_cache;
    uint32_t cLinux_cache_period;
    uint32_t freeMem_cong;

    bool NNSimulator_on;
    bool DNSimulator_on;
    uint32_t prealloc_flush_intvl;
    struct mutex ErrInjectList_Lock;
    struct list_head ErrInjectList_Header;
} config_t;

extern config_t *dms_client_config;

extern int32_t set_config_item (int8_t *conf_str, int32_t count);
extern int32_t show_configuration (int8_t *buffer, int32_t buff_len);
extern int32_t init_dmsc_config (int32_t drv_r_mode);
extern void release_dmsc_config (void);

#else

extern void config_discoC_UsrD(void);
extern void config_discoC_drv(int32_t chrdev_fd, uint32_t mds_ipaddr);
extern int32_t set_config_val(int8_t *name_str, int32_t name_len,
        int8_t *val_str, int32_t val_len);
#endif

extern void init_config_list(void);

#endif /* _DMSC_CONFIG_H_ */
