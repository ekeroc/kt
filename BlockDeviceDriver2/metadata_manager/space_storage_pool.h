/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_storage_pool.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_STORAGE_POOL_H_
#define METADATA_MANAGER_SPACE_STORAGE_POOL_H_

#ifdef DISCO_PREALLOC_SUPPORT

#define FREESPACE_POOL_HSIZE    257

typedef enum {
    SPOOL_INVAL_CHUNK_OK = 0,
    SPOOL_INVAL_CHUNK_NoVolume,
    SPOOL_INVAL_CHUNK_NoChunk,
    SPOOL_INVAL_CHUNK_Fail,
} spool_invalChunk_resp_t;

typedef enum {
    SPOOL_ALLOC_SPACE_OK = 0,
    SPOOL_ALLOC_SPACE_NoVolume,
    SPOOL_ALLOC_SPACE_WAIT,
    SPOOL_ALLOC_SPACE_NOSPACE,
    SPOOL_ALLOC_SPACE_Fail,
} spool_allocSpace_resp_t;

typedef struct volume_space_pool {
    uint64_t volumeID;

    spinlock_t fsSizeInfo_lock;
    uint32_t fsSize_remaining;
    uint32_t fsSize_requesting;
    uint32_t fsSize_userWant;

    struct rw_semaphore space_plock;
    struct list_head space_chunk_pool;
    struct list_head alloc_chunk_pool; //TODO: this should be a radix tree
    struct list_head mdataReq_wait_pool;

    uint32_t num_space_chunk;
    uint32_t num_alloc_unit;
    uint32_t num_waitReq;

    struct list_head entry_fspooltbl;
    sm_cache_pool_t cpool;
} freespace_pool_t;

typedef struct volume_freespace_pool_table {
    struct list_head fspool_tbl[FREESPACE_POOL_HSIZE];
    struct rw_semaphore fspool_tbl_lock[FREESPACE_POOL_HSIZE];
    atomic_t num_fsPool;
} fspool_table_t;

extern void fspool_add_fschunk(fspool_table_t *fstbl, uint32_t volumeID, fs_chunk_t *fsChunk);
extern spool_invalChunk_resp_t fspool_inval_fschunk(fspool_table_t *fstbl, uint32_t volumeID, uint64_t chunkID, uint64_t *maxUnuseHBID);

extern void fspool_inval_pool(fspool_table_t *fstbl, uint32_t volumeID);
extern int32_t fspool_report_pool_unusespace(fspool_table_t *fstbl, uint32_t volumeID,
        struct list_head *tsk_lhead);
extern int32_t fspool_report_pool_allocspace(fspool_table_t *fstbl, uint32_t volumeID,
        struct list_head *tsk_lhead);
extern void fspool_cleanup_pool(fspool_table_t *fstbl, uint32_t volumeID);
extern int32_t fspool_report_allpool_allocspace(fspool_table_t *fstbl, struct list_head *tsk_lhead);
extern int32_t fspool_remove_allocspace(fspool_table_t *fstbl, uint32_t volumeID, uint64_t lbid_s, uint32_t lb_len);

extern int32_t fspool_update_fsSizeRequesting(fspool_table_t *fstbl, uint32_t volumeID, bool do_inc);
extern int32_t fspool_alloc_space(fspool_table_t *fstbl, uint32_t volumeID,
        uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD, ReqCommon_t *callerReq);
extern int32_t fspool_query_volume_MDCache(fspool_table_t *fstbl, metadata_t *arr_MD_bk, uint32_t volid, uint64_t lbid_s, uint32_t lbid_len);

extern freespace_pool_t *fspool_create_pool(fspool_table_t *fstbl);
extern void fspool_init_pool(fspool_table_t *fstbl, freespace_pool_t *fspool,
        uint32_t volumeID);

extern int32_t fspool_free_pool(fspool_table_t *fstbl, uint32_t volumeID);

extern void fspool_init_table(fspool_table_t *fstbl);
extern int32_t fspool_rel_table(fspool_table_t *fstbl);
extern void fspool_show_table(fspool_table_t *fstbl);
extern int32_t fspool_dump_table(fspool_table_t *fstbl, int8_t *buff, int32_t buff_size);

#endif

#endif /* METADATA_MANAGER_SPACE_STORAGE_POOL_H_ */
