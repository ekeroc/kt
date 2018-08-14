/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_fsm.h
 *
 */

#ifndef METADATA_MANAGER_SPACE_CHUNK_FSM_H_
#define METADATA_MANAGER_SPACE_CHUNK_FSM_H_
#ifdef DISCO_PREALLOC_SUPPORT
typedef enum {
    fsChunk_E_AllocSpace,
    fsChunk_E_AllocDONE,
    fsChunk_E_InvalidateSpace,
    fsChunk_E_ReportFree,
    fsChunk_E_CleanSpace,
    fsChunk_E_Unknown
} fschunk_TRevent_t;

typedef enum {
    fsChunk_A_NoAction,
    fsChunk_A_DO_Alloc,
    fsChunk_A_DO_Inval,
    fsChunk_A_DO_Report,
    fsChunk_A_DO_Clean,
    fsChunk_A_DO_Retry,
    fsChunk_A_Unknown,
} fschunk_action_t;

typedef enum {
    fsChunk_S_INIT,
    fsChunk_S_Free,
    fsChunk_S_Allocating,
    fsChunk_S_Invalidated,
    fsChunk_S_Reporting,
    fsChunk_S_Removable,
    fsChunk_S_End,
    fsChunk_S_Unknown,
} fschunk_stat_t;

typedef struct free_space_chunk_state_fsm {
    fschunk_stat_t chunkStat;
    spinlock_t chunk_slock;
    atomic_t num_allocating;
} fs_chunkStat_fsm_t;

typedef enum {
    asChunk_E_Commit,
    asChunk_E_FlushSpace,
    asChunk_E_CleanSpace,
    asChunk_E_Unknown
} aschunk_TRevent_t;

typedef enum {
    asChunk_A_NoAction,
    asChunk_A_DO_Commit,
    asChunk_A_DO_Flush,
    asChunk_A_DO_Clean,
    asChunk_A_DO_Retry,
    asChunk_A_Unknown,
} aschunk_action_t;

typedef enum {
    asChunk_S_INIT,
    asChunk_S_Allocated,
    asChunk_S_Committed,
    asChunk_S_Flushing,
    asChunk_S_Removable,
    asChunk_S_End,
    asChunk_S_Unknown,
} aschunk_stat_t;

typedef struct allocated_space_chunk_state_fsm {
    aschunk_stat_t chunkStat;
    spinlock_t chunk_slock;
} as_chunkStat_fsm_t;

extern fschunk_action_t update_fschunk_state(fs_chunkStat_fsm_t *chunkStat, fschunk_TRevent_t event);
extern int8_t *get_fschunkStat_action_str(fschunk_action_t action);
extern fschunk_stat_t get_fschunk_state(fs_chunkStat_fsm_t *chunkStat);
extern int8_t *get_fschunk_state_str(fs_chunkStat_fsm_t *chunkStat);
extern void init_fschunk_state_fsm(fs_chunkStat_fsm_t *chunkStat);
extern void gain_fschunk_state_lock(fs_chunkStat_fsm_t *chunkStat);
extern void rel_fschunk_state_lock(fs_chunkStat_fsm_t *chunkStat);

extern aschunk_action_t update_aschunk_state(as_chunkStat_fsm_t *chunkStat, aschunk_TRevent_t event);
extern int8_t *get_aschunkStat_action_str(aschunk_action_t action);
extern aschunk_stat_t get_aschunk_state(as_chunkStat_fsm_t *chunkStat);
extern int8_t *get_aschunk_state_str(as_chunkStat_fsm_t *chunkStat);
extern void init_aschunk_state_fsm(as_chunkStat_fsm_t *chunkStat);
extern void gain_aschunk_state_lock(as_chunkStat_fsm_t *chunkStat);
extern void rel_aschunk_state_lock(as_chunkStat_fsm_t *chunkStat);
#endif
#endif /* METADATA_MANAGER_SPACE_CHUNK_FSM_H_ */
