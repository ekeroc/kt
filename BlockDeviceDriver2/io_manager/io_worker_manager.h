/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * io_worker_manager.h
 */

#ifndef IO_MANAGER_IO_WORKER_MANAGER_H_
#define IO_MANAGER_IO_WORKER_MANAGER_H_

#define MAX_NUM_IO_WORKERS  32

typedef struct discoC_io_worker_manager {
    atomic_t active_vol_cnt;
    struct list_head active_vol_list;
    struct mutex active_vol_lock;
    wait_queue_head_t dms_io_wq;
    int32_t num_io_wker;
    dmsc_thread_t *io_workers[MAX_NUM_IO_WORKERS];
} iowker_manager_t;

extern iowker_manager_t iowker_Mgr;

extern void enq_vol_active_list(volume_device_t *vol);
extern void init_ioworker_manager(iowker_manager_t *iowker_mgr, int32_t num_wker);
#endif /* IO_MANAGER_IO_WORKER_MANAGER_H_ */
