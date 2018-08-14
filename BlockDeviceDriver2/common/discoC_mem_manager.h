/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_mem_manager.h
 *
 */

#ifndef COMMON_DISCOC_MEM_MANAGER_H_
#define COMMON_DISCOC_MEM_MANAGER_H_

typedef struct data_mem_item {
    //uint32_t index;
    struct list_head list; //An entry for add memory chunk to memory pool's free list or used list
    int8_t *data_ptr;      //memory pointer for user
} data_mem_item_t;

typedef struct data_mem_chunks {
    struct data_mem_item *chunks[MAX_NUM_MEM_CHUNK_BUFF]; //memory chunk array
    uint32_t chunk_item_size[MAX_NUM_MEM_CHUNK_BUFF]; //unit bytes, how many user data be stored in each chunk
    uint16_t num_chunks;  //number of chunk used by user
} data_mem_chunks_t;

extern void *mem_pool_alloc(int32_t pool_id);
extern void *mem_pool_alloc_nowait(int32_t pool_id);
extern void mem_pool_free(int32_t pool_id, void *mem);

extern void *discoC_mem_alloc(size_t size, gfp_t flags);
extern void discoC_mem_free(void *mem);

#define discoC_malloc_pool(pool_id) mem_pool_alloc(pool_id)
#define discoC_malloc_pool_nowait(pool_id) mem_pool_alloc_nowait(pool_id)
#define discoC_free_pool(pool_id, buffer) mem_pool_free(pool_id, buffer)

extern int32_t register_mem_pool(int8_t *pool_nm, int32_t pool_size, size_t item_size, bool is_mutex);
extern int32_t deregister_mem_pool(int32_t pool_id);

extern int32_t init_mempool_manager(void);
extern int32_t rel_mempool_manager(void);

#endif /* COMMON_DISCOC_MEM_MANAGER_H_ */
