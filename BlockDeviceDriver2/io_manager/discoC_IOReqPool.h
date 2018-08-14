/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReqPool.h
 */

#ifndef IO_MANAGER_IOREQPOOL_H_
#define IO_MANAGER_IOREQPOOL_H_

//TODO: a better protection way is each volume io request pending queue has a reference count
//      to prevent a queue be free when someone reference it
#define IOMGR_VOLIOQ_HKEY   401
typedef struct volume_IOReqPool_manager {
    struct list_head volIOQ_pool[IOMGR_VOLIOQ_HKEY];
    struct rw_semaphore volIOQ_plock[IOMGR_VOLIOQ_HKEY];
    atomic_t num_volIOQ;
} volIOReqPool_Mgr_t;

extern void IOReqPool_gain_lock(volume_IOReqPool_t *volIOq);
extern void IOReqPool_rel_lock(volume_IOReqPool_t *volIOq);

extern void IOReqPool_addOvlpReq_nolock(io_request_t *ioReq, volume_IOReqPool_t *volIOq);
extern void IOReqPool_rmOvlpReq_nolock(io_request_t *ioReq);
extern void IOReqPool_addReq_nolock(io_request_t *ioReq, volume_IOReqPool_t *volIOq);
extern void IOReqPool_rmReq_nolock(io_request_t *ioReq);

#ifdef DISCO_PREALLOC_SUPPORT
extern void volIOReqQMgr_gain_fwwaitQ_lock(volume_IOReqPool_t *myPool);
extern void volIOReqQMgr_rel_fwwaitQ_lock(volume_IOReqPool_t *myPool);
extern void volIOReqQMgr_add_fwwaitQ_nolock(volume_IOReqPool_t *myPool, struct list_head *entry);
extern struct list_head *volIOReqQMgr_peek_fwwaitQ_nolock(volume_IOReqPool_t *myPool);
#endif

extern int32_t volIOReqQMgr_clear_pool(uint32_t volumeID, volIOReqPool_Mgr_t *mymgr, struct list_head *buff_list);
extern int32_t volIOReqQMgr_add_pool(uint32_t volumeID, volIOReqPool_Mgr_t *mymgr);
extern void volIOReqQMgr_rm_pool(uint32_t volumeID, volIOReqPool_Mgr_t *mymgr);
extern volume_IOReqPool_t *volIOReqQMgr_find_pool(uint32_t volumeID, volIOReqPool_Mgr_t *mymgr);
extern void volIOReqQMgr_init_manager(volIOReqPool_Mgr_t *mymgr);
extern volIOReqPool_Mgr_t *volIOReqQMgr_get_manager(void);
extern int32_t volIOReqQMgr_rel_manager(volIOReqPool_Mgr_t *mymgr);

#endif /* IO_MANAGER_IOREQPOOL_H_ */
