/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_commit_worker.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_COMMIT_WORKER_H_
#define METADATA_MANAGER_SPACE_COMMIT_WORKER_H_
#ifdef DISCO_PREALLOC_SUPPORT
typedef enum {
    fstask_prealloc_fs,
    fstask_flush_LHO,
    fstask_report_unuse
} fs_task_type_t;

typedef enum {
    TaskFEA_IN_TaskWQ,
    TaskFEA_IN_TaskPQ,
} task_fea_bit_t;

typedef struct _freespace_commit_NN_worker {
    wait_queue_head_t flush_waitq;
    atomic_t num_flushReq_workq;
    atomic_t num_flushReq_pendq;
    struct list_head fs_task_workq;
    struct list_head fs_task_pendq;
    struct mutex fs_task_qlock;
    dmsc_thread_t *flush_wker;
    void *worker_data;
    //uint64_t flush_intvl; //unit HZ

    uint64_t last_flush_t;

    int32_t done_total_reqs;
    int32_t done_prealloc_reqs;
    int32_t done_flush_reqs;
    int32_t done_return_reqs;
} fs_ciNN_worker_t;

//TODO: task might need state
typedef struct _freespace_task {
    ReqCommon_t fstsk_comm;
    struct list_head entry_taskWQ;
    struct list_head entry_taskPQ;
    metadata_t MetaData;
    void *task_data;
    sm_task_endfn *fstask_end_task_fn;
    fs_ciNN_worker_t *tskWorker;
} fs_task_t;

extern fs_task_t *alloc_fs_task(int32_t mpoolID);
extern void init_fs_task(fs_task_t *fstsk, fs_task_type_t tskType, uint32_t tskID, sm_task_endfn *tsk_endfn, void *tsk_data);
extern void submit_fs_task(fs_ciNN_worker_t *mywker, fs_task_t *fstsk);

extern int32_t inif_fs_worker(fs_ciNN_worker_t *mywker, void *wker_data);
extern int32_t rel_fs_worker(fs_ciNN_worker_t *mywker);
#endif
#endif /* METADATA_MANAGER_SPACE_COMMIT_WORKER_H_ */
