/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_CHUNK_H_
#define METADATA_MANAGER_SPACE_CHUNK_H_

#ifdef DISCO_PREALLOC_SUPPORT

typedef struct fs_chunk { //free space chunk
    uint64_t chunkID;
    uint32_t chunkLen;  //free space length
    uint64_t HBID_s;
    uint16_t num_replica;
    struct meta_cache_dn_loc chunk_DNLoc[MAX_NUM_REPLICA];

    fs_chunkStat_fsm_t chunkState;
    //struct rw_semaphore state_lock;
    struct list_head entry_spacePool;
} fs_chunk_t;

typedef struct allocated_space_chunk {
    uint64_t fs_chunkID;
    uint64_t lbid_s;
    uint64_t hbid_s;
    uint32_t lb_len;
    uint16_t num_replica;
    struct meta_cache_dn_loc chunk_DNLoc[MAX_NUM_REPLICA];
    bool chunk_DNLoc_state[MAX_NUM_REPLICA];

    as_chunkStat_fsm_t chunkState;
    struct list_head entry_allocPool;
} as_chunk_t;

extern void fschunk_show_chunk(fs_chunk_t *fsChunk);
extern int32_t fschunk_dump_chunk(fs_chunk_t *fsChunk, int8_t *buff, int32_t buff_size);

extern int32_t fschunk_invalidate_chunk(fs_chunk_t *chunk);
extern int32_t fschunk_report_unuse(fs_chunk_t *chunk, uint64_t *max_hbids);
extern int32_t fschunk_cleanup_chunk(fs_chunk_t *chunk);

extern fs_chunk_t *fschunk_alloc_chunk(int32_t mpoolID);
extern void fschunk_free_chunk(int32_t mpoolID, fs_chunk_t *chunk);
extern void fschunk_init_chunk(fs_chunk_t *fsChunk, uint64_t chunkID, uint32_t len, uint64_t hbid_s,
        uint16_t numReplica);
extern void fschunk_init_chunk_DNLoc(fs_chunk_t *fsChunk, uint16_t bkIdx,
        uint32_t ipaddr, uint32_t port, uint32_t rbid, uint32_t offset);

extern as_chunk_t *fschunk_alloc_space(fs_chunk_t *chunk, uint64_t lb_s, uint32_t lb_len);

extern int32_t aschunk_flush_chunk(as_chunk_t *asChunk);
extern int32_t aschunk_commit_chunk(as_chunk_t *asChunk);
extern void aschunk_fill_metadata(as_chunk_t *asChunk, metaD_item_t *myMDItem);
extern void aschunk_free_chunk(int32_t mpoolID, as_chunk_t *chunk);
extern void aschunk_init_chunk_DNLoc(as_chunk_t *asChunk, uint16_t bkIdx,
        uint32_t ipaddr, uint32_t port, uint32_t rbid, uint32_t offset);
extern void aschunk_init_chunk(as_chunk_t *asChunk, uint64_t chunkID,
        uint64_t lb_s, uint32_t lb_len, uint64_t hbid_s, uint16_t numReplica);
extern as_chunk_t *aschunk_alloc_chunk(int32_t mpoolID);

extern void aschunk_show_chunk(as_chunk_t *asChunk);
extern int32_t aschunk_dump_chunk(as_chunk_t *asChunk, int8_t *buff, int32_t buff_size);

#endif
#endif /* METADATA_MANAGER_SPACE_CHUNK_H_ */
