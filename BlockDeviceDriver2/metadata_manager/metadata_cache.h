/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * metadata_cache.h
 *
 */
#ifndef METADATA_MANAGER_METADATA_CACHE_H_
#define METADATA_MANAGER_METADATA_CACHE_H_

extern int32_t query_volume_MDCache(metadata_t *arr_MD_bk,
        uint32_t volid, uint64_t lbid_s, uint32_t lbid_len,
        int16_t req_priority, bool query_cache);
extern int32_t update_volume_MDCache(metadata_t *my_MData,
        uint32_t volid, uint64_t lbid_s, uint32_t lbid_len, bool enable_mdcache);
extern int32_t inval_volume_MDCache_LBID(uint64_t volid, uint64_t start_LB, uint32_t LB_cnt);
extern int32_t inval_volume_MDCache_volID(uint64_t volid);
extern int32_t inval_volume_MDCache_DN(int8_t *dnName, int32_t dnName_len, int32_t port);
extern int32_t inval_volume_MDCache_HBID(uint64_t hbid);

extern int32_t free_volume_MDCache(uint64_t volid);
extern int32_t create_volume_MDCache(uint64_t volid, uint64_t capacity);

extern void rel_metadata_cache(void);
extern int32_t init_metadata_cache(void);

#endif /* METADATA_MANAGER_METADATA_CACHE_H_ */
