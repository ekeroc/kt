/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IO_Manager.h
 */

#ifndef IO_MANAGER_DISCOC_IO_MANAGER_H_
#define IO_MANAGER_DISCOC_IO_MANAGER_H_

typedef struct dms_rq {
    struct list_head list;
    struct request *rq;
    atomic_t num_ref_ior;  //NOTE: head and tail rq might belong two io reqs
    bool result;
} dms_rq_t;

struct volume_device;
struct io_request;

typedef struct volume_io_request_pool {
    struct mutex ioReq_plock;
    struct list_head ioReq_pool;
    struct list_head ioReq_ovlp_pool;
    struct list_head list2volIORQ;
#ifdef DISCO_PREALLOC_SUPPORT
    struct list_head ioSReq_fw_waitpool;
    int32_t numWaitReqs;
    struct mutex ioSReq_fw_waitplock;
#endif
    uint32_t volumeID;
    atomic_t numIOReq_pool;
    atomic_t numIOReq_waitpool;
} volume_IOReqPool_t;

typedef enum {
    IOREQ_E_INIT_DONE,
    IOREQ_E_OvlpOtherIO,
    IOREQ_E_OvlpIODone,
    IOREQ_E_RWUDataDone,
    IOREQ_E_ReportMDDone,
    IOREQ_E_HealDone,

    IOREQ_E_IOSeg_IODone,
    IOREQ_E_IOSeg_ReportMDDone,
} IOR_FSM_TREvent_t;

typedef enum {
    IOREQ_A_START,
    IOREQ_A_NoAction,

    IOREQ_A_AddOvlpWaitQ,
    IOREQ_A_SubmitIO,
    IOREQ_A_DO_ReportMD,
    IOREQ_A_DO_Healing,
    IOREQ_A_FreeReq,

    IOREQ_A_IOSegIODoneCIIOR,
    IOREQ_A_IOSegMDReportDoneCIIOR,
    IOREQ_A_END,
    IOREQ_A_UNKNOWN = IOREQ_A_END,
} IOR_FSM_action_t;

typedef enum {
    IOREQ_S_START,
    IOREQ_S_INIT,
    IOREQ_S_RWUserData,
    IOREQ_S_OvlpWait,
    IOREQ_S_ReportMD,
    IOREQ_S_SelfHealing,  //for support self-healing, we need namenode return new location when reportMD
                          //so that we can write data to new location and recovery the protection back to
                          //original promise
    IOREQ_S_ReqDone,
    IOREQ_S_END,
    IOREQ_S_UNKNOWN = IOREQ_S_END,
} IOR_FSM_state_t;

typedef struct discoC_io_request_fsm {
    rwlock_t st_lock;
    IOR_FSM_state_t st_ioReq;
} IOReq_fsm_t;

typedef struct io_request {
    dms_rq_t *rqs[MAX_NUM_RQS_MERGE];
    uint32_t num_rqs;
    struct rw_semaphore rq_lock;
    data_mem_chunks_t payload;

    //pointer for participating to the io_req_Q linked list
    struct list_head list2ioReqPool;
    struct list_head list2ioDoneWPool;
    volume_IOReqPool_t *ref_ioReqPool;

    UserIO_addr_t usrIO_addr;
    uint32_t ioReqID;
    uint32_t ioReq_fea;
    IOReq_fsm_t ioReq_stat;
    uint16_t iodir;
    uint16_t reqPrior;
    bool is_ovlp_ior;
    bool is_dms_lb_align;
    bool is_mdcache_on;
    //record the commit result of nn_reqs, if one of them fail, means io_req fail.
    bool ioResult;
    atomic_t ref_cnt;

    //used for linking up all nn_reqs belong to this io_req
    uint64_t kern_sect;
    uint32_t nr_ksects;
    uint8_t mask_lb_align[MAX_LBS_PER_IOR];

    struct volume_device *drive;
    uint16_t volume_replica;

    struct list_head ioSegReq_lPool; //list pool for io segment request
    spinlock_t ioSegReq_plock;       //list pool lock for ioSegReq_lPool
    atomic_t ciUser_remainLB;
    atomic_t ciMDS_remainLB;
    atomic_t num_of_ci2nn;

    struct list_head wait_list;
    struct list_head wake_list;

    spinlock_t ovlp_group_lock;
    atomic_t cnt_ovlp_wait;
    atomic_t cnt_ovlp_wake;

    worker_t retry_work;

    //struct timespec io_req_start_handle_time;
    uint64_t t_ioReq_create;
    uint64_t t_ioReq_start;
    uint64_t t_ioReq_start_acquirMD;
    uint64_t t_ioReq_ciUser;
    uint64_t t_ioReq_ciMDS;

    uint8_t fp_map[MAX_SIZE_FPMAP];
    uint8_t fp_cache[MAX_LBS_PER_IOR*FP_LENGTH];
} io_request_t;

/*
 * An io requests will split into several io requests according to
 * ovw state / partial write state / cache state
 * An splitted io requests will contain a NN requests and a FDN requests
 */
typedef struct discoC_io_manager {
    atomic_t cnt_onfly_ioReq;
    atomic_t cnt_onfly_ovlpioReq;
    atomic_t ioReqID_gentor;  //gentor: generator
    atomic_t cnt_split_io_req;
    atomic_t ioSegID_gentor; //gentor: generator
    atomic_t ioSleafR_ID_gentor;
    int32_t ioSeg_mpool_ID;
    int32_t ioSeg_leafR_mpool_ID;
    int32_t io_wBuffer_mpool_ID;
    int32_t io_dmsrq_mpool_ID;
} io_manager_t;

extern io_manager_t discoCIO_Mgr;

extern int32_t IOMgr_get_IOWbuffer_mpoolID(void);
extern uint32_t IOMgr_alloc_ioReqID(void);
extern dms_rq_t *IOMgr_alloc_dmsrq(void);
extern void IOMgr_free_dmsrq(dms_rq_t *rq);

#endif /* IO_MANAGER_DISCOC_IO_MANAGER_H_ */
