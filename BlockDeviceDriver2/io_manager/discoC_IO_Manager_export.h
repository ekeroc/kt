/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IO_Manager_export.h
 */

#ifndef IO_MANAGER_DISCOC_IO_MANAGER_EXPORT_H_
#define IO_MANAGER_DISCOC_IO_MANAGER_EXPORT_H_

extern void commit_user_request(struct request *rq, bool result);
extern void IOMgr_submit_IOReq(dms_rq_t **dmsrqs,
        uint32_t index_s, uint32_t num_rqs,
        uint64_t sect_s, uint32_t sect_len);

extern int32_t create_volumeIOReq_queue(uint32_t volumeID);
extern void remove_volumeIOReq_queue(uint32_t volumeID);
extern int32_t clear_volumeIOReq_queue(uint32_t volumeID);

extern int32_t IOMgr_get_onfly_ovlpioReq(void);
extern int32_t IOMgr_get_onfly_ioSReq(void);
extern int32_t IOMgr_get_onfly_ioReq(void);
extern int32_t IOMgr_get_commit_qSize(void);
extern uint32_t IOMgr_get_last_ioReqID(void);
extern void IOMgr_rel_manager(void);
extern void IOMgr_init_manager(void);
#endif /* IO_MANAGER_DISCOC_IO_MANAGER_EXPORT_H_ */
