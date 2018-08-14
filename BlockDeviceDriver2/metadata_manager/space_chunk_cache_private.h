/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_cache_private.h
 *
 */
#ifndef METADATA_MANAGER_SPACE_CHUNK_CACHE_PRIVATE_H_
#define METADATA_MANAGER_SPACE_CHUNK_CACHE_PRIVATE_H_

typedef struct sm_cache_aschunk_item {
    void *user_data;         //pointer to user data
    uint64_t user_key;
    struct list_head entry_lru;
    uint32_t num_items;
} __attribute__((__packed__)) sm_cache_item_t;

#endif /* METADATA_MANAGER_SPACE_CHUNK_CACHE_PRIVATE_H_ */
