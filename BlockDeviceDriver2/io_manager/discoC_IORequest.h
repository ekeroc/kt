/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IORequest.h
 */

#ifndef IO_MANAGER_DISCOC_IOREQUEST_H_
#define IO_MANAGER_DISCOC_IOREQUEST_H_

typedef enum {
    IOREQ_FEA_IN_REQPOOL,
    IOREQ_FEA_IN_WAITPOOL,
    IOREQ_FEA_IN_IODONEPOOL,
    IOREQ_FEA_CIUSER,
} IOReq_fea_t;

typedef enum {
    LBA_FEA_INIT = 0,
    LBA_FEA_FW_4KALIGN,
    LBA_FEA_FW_non4KALIGN,
    LBA_FEA_OVW_4KALIGN,
    LBA_FEA_OVW_non4KALIGN,
    LBA_FEA_END,
} LBA_fea_stat_t;

#define max_group_size (MAX_LBS_PER_IOR + 1) //NOTE: Should be max number of LBs + 1

extern bool ioReq_chkset_fea(io_request_t *ioreq, uint32_t fea_bit);
extern bool ioReq_chkclear_fea(io_request_t *ioreq, uint32_t fea_bit);
extern void ioReq_add_waitReq(io_request_t *ioreq, struct list_head *waitR);
extern void ioReq_add_wakeReq(io_request_t *ioreq, struct list_head *wakeR);
extern int32_t ioReq_init_request(io_request_t *ioreq, volume_device_t *drv,
        priority_type req_prior, dms_rq_t **rqs,
        uint32_t index_s, uint32_t num_rqs, uint64_t sect_s, uint32_t sect_len);
extern void ioReq_end_request(io_request_t *io_req);

extern bool ioReq_add_ReqPool(io_request_t *ioreq);

extern void ioReq_IODone_endfn(io_request_t *ioReq, uint32_t num_LBs);
extern void ioReq_MDReport_endfn(io_request_t *ioReq, uint32_t num_LBs);

extern int32_t ioReq_gen_ioSegReq(io_request_t *ior);
extern void ioReq_submit_request(io_request_t *ior);
extern void ioReq_submit_reportMDReq(io_request_t *ior);
extern void ioReq_commit_user(io_request_t *dms, bool result);

#endif /* IO_MANAGER_DISCOC_IOREQUEST_H_ */
