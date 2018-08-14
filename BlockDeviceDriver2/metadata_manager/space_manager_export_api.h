/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_manager_export_api.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_MANAGER_EXPORT_API_H_
#define METADATA_MANAGER_SPACE_MANAGER_EXPORT_API_H_

#ifdef DISCO_PREALLOC_SUPPORT
typedef enum {
    SM_INVAL_CHUNK_OK = 0,
    SM_INVAL_CHUNK_NoVolume,
    SM_INVAL_CHUNK_NoChunk,
    SM_INVAL_CHUNK_Fail,
} sm_invalChunk_resp_t;

typedef enum {
    SM_ALLOC_SPACE_OK = 0,
    SM_ALLOC_SPACE_NoVolume,
    SM_ALLOC_SPACE_WAIT,
    SM_ALLOC_SPACE_NOSPACE,
    SM_ALLOC_SPACE_Fail,
} sm_allocSpace_resp_t;

typedef int32_t (sm_task_endfn) (void *cb_arg, int32_t result);

extern int32_t sm_add_volume_fschunk(uint32_t volumeID, uint64_t chunkID, uint32_t len, uint64_t hbid_s,
        uint16_t numReplica, struct meta_cache_dn_loc *arr_dnLoc);
extern sm_invalChunk_resp_t sm_invalidate_volume_fschunk(uint32_t volumeID, uint64_t chunkID, uint64_t *maxUnuseHBID); //Called when NN request
extern void sm_invalidate_volume(uint32_t volumeID); //Called when detach volume

extern int32_t sm_report_volume_unusespace(uint32_t volumeID); //Called when detach volume
extern int32_t sm_report_volume_spaceMData(uint32_t volumeID);
extern int32_t sm_prealloc_volume_freespace(uint32_t volumeID, uint32_t minSize, sm_task_endfn *cbfn, void *cb_data);
extern int32_t sm_report_allvolume_spaceMData(void);

extern void sm_cleanup_volume(uint32_t volumeID);
extern int32_t sm_remove_allocspace(uint32_t volumeID, uint64_t lbid_s, uint32_t lb_len);
extern sm_allocSpace_resp_t sm_alloc_volume_space(uint32_t volumeID, uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD, ReqCommon_t *callerReq);

extern int32_t sm_query_volume_MDCache(metadata_t *arr_MD_bk, uint32_t volid, uint64_t lbid_s, uint32_t lbid_len);

extern void sm_show_storage_pool(void);
//extern int32_t sm_report_fschunk(uint32_t volumeID, uint64_t chunkID);
//extern int32_t sm_cleanup_fschunk(uint32_t volumeID, uint64_t chunkID);
extern int32_t sm_dump_info(int8_t *buffer, uint32_t buff_size);

extern int32_t free_volume_fsPool(uint32_t volumeID);
extern int32_t create_volume_fsPool(uint32_t volumeID);
extern int32_t init_space_manager(void);
extern int32_t rel_space_manager(void);

/*
 * TODO: support
 * 1. flush specific volume's metadata with blocking API - ioctl / detach
 * 2. flush all volume's metadata with blocking API - stop cn
 * 3. flush all volume's metadata with non blocking API - flush worker
 * 4. report specific volume's free space with blocking API - ioctl / detach
 * 5. report all volume's free space with blocking API - stop CN
 * 6. pre-alloc
 */
#endif
 
#endif /* METADATA_MANAGER_SPACE_MANAGER_EXPORT_API_H_ */
