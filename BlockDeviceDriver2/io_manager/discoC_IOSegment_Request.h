/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOSegment_Request.h
 */


#ifndef IO_MANAGER_IOSEGMENT_REQUEST_H_
#define IO_MANAGER_IOSEGMENT_REQUEST_H_

typedef enum {
    CI_MDS_OP_REPORT_NONE = 0,
    CI_MDS_OP_REPORT_SUCCESS,  //for write IO
    CI_MDS_OP_REPORT_FAILURE   //for read  IO
} ciMDS_opCode_t; //operation code for committing to MDS (metadata server, namenode)

typedef enum {
    IOSegR_FEA_NON_4KALIGN = 0, //whether segment IO is 4K align
    IOSegR_FEA_OVW,         //whether segment IO is ovw or first write
    IOSegR_FEA_MDCacheH,    //whether segment IO is metadata cache hit
    IOSegR_FEA_HBID_MISMATCH,    //whether segment IO encounter metadata-data unsync
    IOSegR_FEA_CIUSER,
    IOSegR_FEA_TrueProc,     //When a normal ioSegReq enter queryTrue process, we need check each LB state
#ifdef DISCO_PREALLOC_SUPPORT
    IOSegR_FEA_IN_FWWAITQ,
#endif
} IOSegR_fea_t;

typedef enum {
    IOSegREQ_E_INIT_4KW,      //4K align write
    IOSegREQ_E_W_AcqMD_DONE,  //acc MD for w done
    IOSegREQ_E_W_ALLPL_DONE,  //all payload done

    IOSegREQ_E_INIT_R,
    IOSegREQ_E_R_AcqMD_DONE,  //acc MD for w done
    IOSegREQ_E_R_ALLPL_DONE,  //all payload done
    IOSegREQ_E_W_RPhase_DONE,
    IOSegREQ_E_R_RPhase_DONE,

    IOSegREQ_E_MDUD_UNSYNC,
    IOSegREQ_E_ATrueMD_DONE,
} IOSegREQ_FSM_TREvent_t;

typedef enum {
    IOSegREQ_A_START,

    IOSegREQ_A_NoAction,
    IOSegREQ_A_RetryTR,

    IOSegREQ_A_W_AcqMD,
    IOSegREQ_A_DO_WUD,

    IOSegREQ_A_R_AcqMD,
    IOSegREQ_A_DO_RUD,
    IOSegREQ_A_DO_ATrueMD_PROC,
    IOSegREQ_A_DO_ATrueMD_DONE,
    IOSegREQ_A_DO_CI2IOSegREQ,

    IOSegREQ_A_DO_CI2IOREQ,
    IOSegREQ_A_END,
    IOSegREQ_A_UNKNOWN = IOSegREQ_A_END,
} IOSegREQ_FSM_action_t;

typedef enum {
    IOSegREQ_S_START,
    IOSegREQ_S_INIT,
    IOSegREQ_S_W_AcqMD, //accquire metadata
    IOSegREQ_S_WUD,   //write user data

    IOSegREQ_S_R_AcqMD, //accquire metadata
    IOSegREQ_S_RUD,   //Read data for Read
    IOSegREQ_S_ATrueMD_Proc,
    IOSegREQ_S_CI2IOSeg,
    IOSegREQ_S_CI2IOR,

    IOSegREQ_S_END,
    IOSegREQ_S_UNKNOWN = IOSegREQ_S_END,
} IOSegREQ_FSM_state_t;

typedef enum {
    IOSegREQ_CI_S_START,
    IOSegREQ_CI_S_INIT,
    IOSegREQ_CI_S_CI,
} IOSegREQ_ciFSM_state_t;

typedef struct discoC_ioSeg_request_fsm {
    rwlock_t st_lock;
    IOSegREQ_FSM_state_t st_ioSegReq;
    IOSegREQ_ciFSM_state_t cist_ioSegReq;
} IOSegReq_fsm_t;

struct io_request;

/*
 * an io request will aggregate several linux request into 1 io_request
 * an io request will split into several ioseg_req_t base on overwritten flag situation
 *       local metadata cache status
 */
typedef struct discoC_ioSegment_request {
    ReqCommon_t ioSR_comm;
    struct list_head list2ioReq;
#ifdef DISCO_PREALLOC_SUPPORT
    struct list_head entry_fwwaitpool;
#endif
    struct io_request *ref_ioReq;

    IOSegReq_fsm_t ioSegReq_stat;
    UserIO_addr_t ioSReq_ioaddr;

    uint16_t mdata_opcode; //TODO: should be removed at future
    iorw_dir_t io_dir_true;

    uint8_t *lb_sect_mask;

    metadata_t MetaData;
    uint64_t payload_offset; //unit : bytes

    bool ciUser_result;  //io result for committing to user (true / false: IO success / IO failure)
    ciMDS_opCode_t ciMDS_status;  //TODO: might be changed at future
    atomic_t ciUser_remainLB;    //remain LBs to wait for committing to user
    atomic_t ciMDS_remainLB;      //remain LBs to wait for committing to MDS
    uint8_t userLB_state[NUM_BYTES_MAX_LB]; //for queryTrue
    spinlock_t ciUser_slock;  //TODO: will rwlock better?

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    uint64_t ts_dn_logging;
#endif

    /*
     * TODO: add in future
     * io remaining time
     */
    uint64_t t_send_DNReq; //timestamp that send 1st DN Request
    uint64_t t_rcv_DNResp; //timestamp that rcv last DN Response
    uint64_t t_txMDReq;
    uint64_t t_rxMDReqAck;

    struct list_head list_pReqPool;
    spinlock_t pReq_plock;
    atomic_t num_pReq;

    struct list_head leaf_ioSRPool; //for link all ioSReq that perform true MD process
    spinlock_t leaf_ioSR_plock;
    atomic_t num_leaf_ioSR;
} ioSeg_req_t;

/******** set/check feature related API ********/
extern bool ioSReq_get_ovw(ioSeg_req_t *ioSegReq);
extern void ioSReq_set_ovw(ioSeg_req_t *ioSReq);
extern bool ioSReq_chk_MDCacheHit(ioSeg_req_t *ioSegReq);
extern void ioSReq_set_MDCacheHit(ioSeg_req_t *ioSReq);
extern bool ioSReq_chk_4KAlign(ioSeg_req_t *ioSegReq);
extern void ioSReq_set_HBIDErr(ioSeg_req_t *ioSReq);
extern bool ioSReq_chk_HBIDErr(ioSeg_req_t *ioSReq);
extern bool ioSReq_chkset_ciUser(ioSeg_req_t *ioSReq);
extern bool ioSReq_chk_TrueMDProc(ioSeg_req_t *ioSReq);
extern bool ioSReq_chkset_TrueMDProc(ioSeg_req_t *ioSReq);
extern void ioSReq_resetFea_PartialRDone(ioSeg_req_t *ioSReq);

/******** create / free / initialize request ********/
extern ioSeg_req_t *ioSReq_alloc_request(void);
extern void ioSReq_free_request(ioSeg_req_t *ioSegReq);
extern void ioSReq_init_request(ioSeg_req_t *ioSegReq, UserIO_addr_t *ioaddr, iorw_dir_t ioDir, priority_type reqPrior);
extern int32_t ioSReq_set_request(ioSeg_req_t *ioSegReq, struct io_request *refIOR,
        uint8_t *lb_sect_mask, uint64_t payload_off, bool is_ovw, bool is_chit, bool is4Kalign);

/******** callback function for payload request ********/
extern void ioSReq_add_payloadReq(void *ioSReq, struct list_head *list);
extern int32_t ioSReq_rm_payloadReq(void *ioSReq, struct list_head *list);
extern void ioSReq_rm_ioSLeafReq(ioSeg_req_t *rootR, struct list_head *leafR);
extern void ioSReq_add_ioSLeafReq(ioSeg_req_t *rootR, struct list_head *leafR);

extern int32_t ioSReq_submit_reportMD(ioSeg_req_t *ioSReq, bool mdch_on, uint16_t vol_replica);
extern int32_t ioSReq_submit_acquireMD(ioSeg_req_t *ioSReq, bool mdch_on, uint16_t vol_replica);

extern void ioSReq_update_udata_ts(ioSeg_req_t *ioSReq, uint64_t ts_send, uint64_t ts_rcv);
extern void ioSReq_update_mdata_ts(ioSeg_req_t *ioSReq, uint64_t ts_send, uint64_t ts_rcv);

extern void ioSReq_commit_user(ioSeg_req_t *ioSReq, bool result);
#ifdef DISCO_PREALLOC_SUPPORT
extern bool ioSReq_alloc_freeSpace(ioSeg_req_t *ioSegReq, bool do_lock);
#endif
#endif /* IO_MANAGER_IOSEGMENT_REQUEST_H_ */
