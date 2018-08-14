/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_manager_private.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_MANAGER_PRIVATE_H_
#define METADATA_MANAGER_SPACE_MANAGER_PRIVATE_H_

#ifdef DISCO_PREALLOC_SUPPORT
#define MAX_NUM_FS_TASK  102400
#define MAX_NUM_FSCHUNK  DEFAULT_MAX_NUM_OF_VOLUMES*16
#define MAX_NUM_ASCHUNK  (2*1024*1024)
#define FLUSH_NUM_ASCHUNK   (MAX_NUM_ASCHUNK / 4)

typedef struct _freespace_manager {
    fspool_table_t fsPooltbl;
    fs_ciNN_worker_t fs_ci_wker;
    atomic_t fs_ReqID_generator;
    int32_t fstask_mpool_ID;
    int32_t fschunk_mpool_ID;
    int32_t aschunk_mpool_ID;
} fs_manager_t;

fs_manager_t discoC_fsMgr;
#endif

#endif /* METADATA_MANAGER_SPACE_MANAGER_PRIVATE_H_ */
