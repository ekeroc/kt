/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_fsm_private.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_CHUNK_FSM_PRIVATE_H_
#define METADATA_MANAGER_SPACE_CHUNK_FSM_PRIVATE_H_

#ifdef DISCO_PREALLOC_SUPPORT

int8_t *fsChunkStat_str[] = {
        "init", "free", "Allocating", "invalidated",  "reporting",
        "removable", "end", "Unknown"
};

int8_t *fsChunkAction_str[] = {
        "No action", "Do Allocation", "Do Invalidation", "Do Report", "Do Clean",
        "Retry TR Event", "Unknown"
};

int8_t *asChunkStat_str[] = {
        "init", "allocated", "committed", "flushing",  "removable",
        "end", "Unknown"
};

int8_t *asChunkAction_str[] = {
        "No action", "Do Commit", "Do Flush", "Do Clean",
        "Retry TR Event", "Unknown"
};

#endif
#endif /* METADATA_MANAGER_SPACE_CHUNK_FSM_PRIVATE_H_ */
