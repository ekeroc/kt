/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_manager_export.h
 *
 * This header file define data structure and API of payload manager that required by
 * non payload_manager components
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_MANAGER_EXPORT_H_
#define PAYLOAD_MANAGER_PAYLOAD_MANAGER_EXPORT_H_

#define MIN_PLREQ_SCHED_TIME  2 //5 seconds
#define MIN_PLREQ_IO_TIME   60  //60 seconds
#define PLREQ_READ_TO_CNT   5

/*
 * PLReq_ciUser : For write payload request. DISCO provide multiple replica protection for data.
 *               For DISCO system point of view, clientnode should send data to all DN and waiting
 *                   the response. Once all DN return ack, clientnode finish whole requests.
 *               For improving the latency, clientnode will commit to user when meeting some criteria
 *                   (example: 1 DN write OK, but still waiting other DN's response).
 *               Under this situation, payload manager will can PLReq_ciUser to notify caller
 *               to commit to user
 * PLReq_endfn_t: When payload finish the R/W payload processing or timeout fired,
 *                payload manager call this callback function and caller take over the
 *                request processing.
 * PLReq_reportMDErr_fn_t: for read payload request, payload manager choose a DN to read data.
 *                Datanode will check whether metadata (HBID) of user data on Datanode's disk
 *                match the metadata (HBID) in DNUData_Request.
 *                If yes, DN will return user data.
 *                In no, DN will return HBID not match errors and HBIDs on disk.
 *                Payload manager will call this callback function to caller.
 * PLReq_copyData_caller: For read payload request, caller has to provide the copy function
 *                        for payload manager to copy data from data_mem_chunks_t to
 *                        caller's internal data buffer
 * PLReq_add_callerRPool:
 * PLReq_rm_callerRPool:
 *                Payload manager allows caller can cancel request so that it will
 *                require some information for caller specify which payload request needed
 *                be canceled. Provide 2 callback functions for payload manager
 *                add/remove payload request from caller's pool
 */
typedef int32_t (PLReq_ciUser) (void *user_data, int32_t numLB, bool ioRes);
typedef int32_t (PLReq_endfn_t) (void *user_data, ReqCommon_t *calleeReq, uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN);
typedef int32_t (PLReq_reportMDErr_fn_t) (void *user_data, uint64_t lb_s, uint32_t numLB);
typedef void (PLReq_copyData_caller) (void *user_data, metaD_item_t *mdItem, uint64_t payload_off, data_mem_chunks_t *payload);
typedef void (PLReq_add_callerRPool) (void *user_data, struct list_head *lst_ptr);
typedef int32_t (PLReq_rm_callerRPool) (void *user_data, struct list_head *lst_ptr);

typedef struct payload_request_func {
    PLReq_add_callerRPool *add_PLReq_CallerReq;
    PLReq_rm_callerRPool *rm_PLReq_CallerReq;
    PLReq_copyData_caller *cpData_CallerReq;
    PLReq_ciUser *ciUser_CallerReq;
    PLReq_reportMDErr_fn_t *reportMDErr_CallerReq;
    PLReq_endfn_t *PLReq_endReq;
} PLReq_func_t;

extern int32_t PLMgr_read_payload(metadata_t *my_metaData, ReqCommon_t *callerReq,
        uint64_t payload_offset, bool send_data, bool rw_data,
        void *usr_data, int32_t *numLB_inval, struct Read_DN_selector *dnSeltor,
        PLReq_func_t *pReq_func);
extern int32_t PLMgr_write_payload(metadata_t *my_metaD, ReqCommon_t *callerReq,
        data_mem_chunks_t *wdata, uint64_t payload_offset, uint8_t *fp_map, uint8_t *fp_cache,
        bool send_data, bool rw_data, void *usr_data, PLReq_func_t *pReq_func);
int32_t PLMgr_init_manager(void);
int32_t PLMgr_rel_manager(void);

#endif /* PAYLOAD_MANAGER_PAYLOAD_MANAGER_EXPORT_H_ */
