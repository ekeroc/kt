/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * volume_manager.h
 *
 */

#ifndef _VOLUME_MANAGER_H_
#define _VOLUME_MANAGER_H_

#define DEVICE_NAME_PREFIX  "dms"

typedef enum {
    Vol_Attaching = 0,
    Vol_Ready_for_User, //Devices appears at user space and user can R/W data
    Vol_IO_Blocking,
    Vol_IO_Yielding,
    Vol_IO_Block_Yield,
    Vol_IO_Block_Detach,
    Vol_Detaching,
    Vol_Detached,
} vol_state_type_t;

typedef enum {
    Vol_Attach_Finish,
    Vol_Block_IO,
    Vol_UnBlock_IO,
    Vol_Yield_IO,
    Vol_NoYield_IO,
    Vol_Yield_IO_Timeout,
    Vol_Detach,
    Vol_No_Ligner_IO,
} vol_state_op_t;

typedef enum {
    RW_NO_PERMIT = 0,
    R_ONLY,
    W_ONLY,
    RW_PERMIT,
} vol_rw_perm_type_t;

typedef enum {
    Vol_Data_Replic,
    Vol_Data_Parity,
} vol_protect_type_t;

typedef enum {
    Vol_Deactive = 0, //Vol operation right belong to linux threads
    Vol_Active,       //Vol operation right belong dms_io_worker
    Vol_Waiting,  //Vol operation right belong timeout handler
} vol_active_stat_t;

typedef enum {
    Vol_EnqRqs = 0,      //issued by linux threads
    Vol_RqsDone,         //issued by dms_io_worker
    Vol_WaitForMoreRqs,  //issued by dms_io_worker
    Vol_WaitDone,        //issued by timeout handler
    Vol_NoOP,
    Vol_DoEnActiveQ,
    Vol_DoWaiting,
} vol_active_op_t;

typedef struct vol_protect_method {
    vol_protect_type_t prot_type;
    uint32_t replica_factor;
} vol_protect_method_t;

typedef struct volume_features {
    vol_protect_method_t prot_method;
    bool share_volume;
    bool mdata_cache_enable; //meta data cache enable
    bool pdata_cache_enable; //payload data cache enable
    //bool fc_enable;        //flow control enable, current only global control
    vol_rw_perm_type_t rw_permission; //0x11 : RW, 0x01: Read only, 0x10: Write only, 0x00:
    bool send_rw_payload;
    bool do_rw_IO;
} volume_features_t;

typedef struct volume_ovw {
    struct rw_semaphore ovw_lock;
    int8_t *ovw_memory;
    bool ovw_init;
    bool ovw_reading;
} volume_ovw_t;

typedef struct volume_io_cnt {
    atomic_t linger_wreq_cnt;
    atomic_t linger_rreq_cnt;
    atomic_t disk_open_cnt;
    uint32_t error_commit_cnt;
} volume_io_cnt_t;

typedef struct perf_main_data {
    uint64_t total_time;
    uint64_t total_time_user;
    atomic64_t total_cnt;
    uint64_t total_sectors_done;  // total sectors processed
    uint64_t total_sectors_req;   // total user requested sectors
} perf_main_data_t;

typedef struct perf_minor_data {
    uint64_t total_time;
    uint64_t wait_mdata_t;  //wait metadata
    uint64_t wait_pdata_t;  //wait payload data
    atomic64_t total_cnt;
} perf_minor_data_t;

typedef struct volume_perf_data {
    struct mutex perf_lock;

    /*
     * Avg IO Request latency = total process time / total process cnt
     * Avg IO Request without overlap = (total process time -  total_stay_in_overlap_time) / total process cnt
     * Avg IO Request latency = total process time / total process cnt
     * Avg IO Request without overlap =
     *   (total process time -  total_stay_in_overlap_time) / total process cnt
     */

    /* From create to NN commit (Sum of each request latency) */
    perf_main_data_t perf_io;

    /* From create to put back to pending q */
    uint64_t total_in_ovlp_time;    // total stay in overlap time
    atomic64_t total_in_ovlp_cnt;   // total stay in overlap count


    /***** read case *****/
    perf_main_data_t perf_read;        // From Generate Req to commit to NN
    perf_minor_data_t perf_read_cm;    // read cache miss
    perf_minor_data_t perf_read_ch;    // read cache hit

    /***** write case *****/
    perf_main_data_t perf_write;       // From Generate Req to commit to NN
    perf_minor_data_t perf_write_fw4a; // 4K-aligned first write
    perf_minor_data_t perf_write_fw4nano; // non-4K-aligned frist write, no old metadata
    perf_minor_data_t perf_write_fw4nao;  // non-4K-aligned first write, with old metadata
    perf_minor_data_t perf_write_ow4a_cm; // 4K-aligned overwrite, cache miss
    perf_minor_data_t perf_write_ow4a_ch; // 4K-aligned overwrite, cache hit
    perf_minor_data_t perf_write_ow4na_cm; // non-4K-aligned overwrite, cache miss
    perf_minor_data_t perf_write_ow4na_ch; // non-4K-aligned overwrite, cache hit
} volume_perf_data_t;

struct io_request;

typedef struct volume_gd_param {
    request_fn_proc *request_fn;
    uint32_t nr_reqs;           //MAX_NUM_KERNEL_RQS
    uint32_t logical_blk_size;  //KERNEL_LOGICAL_SECTOR_SIZE
    struct block_device_operations *fop;
    uint32_t max_sectors;       //MAX_SECTS_PER_RQ
} volume_gd_param_t;

typedef struct dev_minor_num {
    struct list_head list;
    uint32_t minor_number;
} dev_minor_num_t;

typedef struct volume_device {
    struct list_head list; //for volume tables

    struct list_head active_list;  //active list dms_io_worker
    struct list_head vol_rqs_list; //store un-handled rq
    struct mutex vol_rqs_lock;
    atomic_t cnt_rqs; //record number of rqs in vol_rqs_list
    uint32_t rq_agg_wait_cnt;
    worker_t reagg_work;
    vol_active_stat_t vol_act_stat;
    struct mutex vol_act_lock;
    atomic_t w_sects;
    atomic_t r_sects;
    uint64_t sect_start;

    uint32_t nnIPAddr;
    uint32_t nnClientID;
    int8_t nnSrv_name[MAX_LEN_NNSRV_NM];
    uint32_t vol_id;
    uint64_t capacity;
    atomic_t lgr_rqs; //number of rqs that not yet commit to user
    dev_minor_num_t *dev_minor_num;

    volume_features_t vol_feature;
    volume_ovw_t ovw_data;
    //void *mdata_cache; //for future purpose
    //void *pdata_cache; //for future purpose

    struct rw_semaphore vol_state_lock;
    vol_state_type_t vol_state;
    /*
     * vol state : ready for user should has two different, stop or start
     * and has the same behavior. (block io, unblock io ...)
     * when vm is stop, means that no rw permission, block device queue stop
     * no metadata cache, no overwritten flags.
     * TODO: figure a better way to integrate is_vol_stop into ready_for_user
     * TODO: Should we protect tis variable?
     */
    bool is_vol_stop;

    struct workqueue_struct *retry_workq;

    struct dms_fc_group *vol_fc_group;

    struct gendisk *disk;
    struct request_queue *queue;
    spinlock_t qlock;
    request_fn_proc *request_fn;
    uint32_t logical_sect_size;
    uint32_t kernel_sect_size;

    struct Read_DN_selector dnSeltor;

    volume_io_cnt_t io_cnt;
    uint32_t sects_shift;
    volume_perf_data_t perf_data;
} volume_device_t;

extern bool is_vol_io_done(volume_device_t *vol, bool chk_rqs);
extern bool wait_vol_io_done(volume_device_t *vol, bool chk_rqs);

extern volume_device_t *find_volume(uint32_t vol_id);
extern void reset_vol_perf_data(volume_device_t *vol);
extern void vm_update_perf_data(io_request_t *io_req);

extern void vm_reset_allvol_iocnt(void);
extern void vm_dec_iocnt(volume_device_t *vol_dev, uint16_t iorw);
extern void vm_inc_iocnt(volume_device_t *vol_dev, uint16_t iorw);

extern void reset_vol_iocnt(volume_device_t *vol);
extern bool vol_no_perm_to_io(volume_device_t *vol, int32_t io_rw);
extern void set_vol_ovw_read_state(uint32_t vol_id, bool read_done);
extern uint32_t get_volume_replica(volume_device_t *vol);
extern bool is_vol_mcache_enable(volume_device_t *vol);
extern bool is_vol_share(volume_device_t *vol);
extern void set_vol_mcache(volume_device_t *vol, bool enable);
extern bool is_vol_yield_io(volume_device_t *vol);
extern void set_volume_not_yield(volume_device_t *vol);
extern bool is_vol_block_io(volume_device_t *vol);
extern bool all_volumes_removeable(void);
extern uint32_t detach_all_volumes(void);
extern void set_volume_rw_permission(uint32_t volid,
        vol_rw_perm_type_t rw_perm);
extern vol_rw_perm_type_t get_vol_rw_perm(volume_device_t *vol);
extern void set_vol_rw_perm(volume_device_t *vol, vol_rw_perm_type_t io_rw);
extern bool is_vol_send_rw_payload(volume_device_t *vol);
extern void set_vol_send_rw_payload(volume_device_t *vol, bool send_rw_payload);
extern void set_vol_do_rw_IO(volume_device_t *vol, bool do_rw_IO);
extern bool is_vol_do_rw_IO (volume_device_t *vol);

extern void set_vol_share(volume_device_t *vol, bool enable);
extern void set_volume_state_attach_done(uint32_t nnIPaddr, uint32_t vol_id);
extern void unblock_volume_io(uint32_t vol_id);

extern volume_device_t *iterate_all_volumes(volume_device_t *old_vol);
extern void iterate_break(volume_device_t *vol);

extern void set_vol_major_num(int32_t blk_major);
extern void set_max_attach_vol(uint32_t max_att_vol);

extern void enq_dmsrq_vol_list(volume_device_t *vol, dms_rq_t *dmsrq);
extern void enq_dmsrq_vol_list_batch(volume_device_t *vol, dms_rq_t **dmsrqs,
        uint32_t len, uint64_t sect_s);
extern void update_vol_rq_list_start(volume_device_t *vol);
extern int32_t deq_dms_rq_list(volume_device_t *vol, dms_rq_t **arr_dmsrqs,
        uint32_t arr_index_s, uint64_t *sect_start);
extern vol_active_op_t update_vol_active_stat(volume_device_t *vol,
        vol_active_op_t op);

extern int32_t init_vol_manager(int32_t blk_major_num, int32_t max_att_vol);
extern void release_vol_manager(void);

extern uint32_t show_volume_info(int8_t *buffer, volume_device_t *vol);
#endif /* _VOLUME_MANAGER_H_ */
