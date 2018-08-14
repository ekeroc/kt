/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_manager.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_MANAGER_H_
#define METADATA_MANAGER_SPACE_MANAGER_H_

#ifdef DISCO_PREALLOC_SUPPORT

typedef struct sm_task_completion_wrapper {
    struct completion comp_work;
    atomic_t counter;
} sm_completion_t;

uint32_t generate_fsReq_ID(void);
int32_t get_tskMemPoolID(void);
int32_t get_fsChunkMemPoolID(void);
int32_t get_asChunkMemPoolID(void);

extern int32_t sm_preallocReq_endfn(uint32_t volumeID);
extern int32_t sm_get_flush_criteria(void);
#endif

#endif /* METADATA_MANAGER_SPACE_MANAGER_H_ */
