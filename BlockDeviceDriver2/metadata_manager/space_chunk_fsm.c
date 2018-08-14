/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_fsm.c
 *
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/common.h"

#ifdef DISCO_PREALLOC_SUPPORT
#include "space_chunk_fsm_private.h"
#include "space_chunk_fsm.h"

static fschunk_action_t _update_fschunkInit (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
    case fsChunk_E_AllocDONE:
    case fsChunk_E_InvalidateSpace:
    case fsChunk_E_ReportFree:
    case fsChunk_E_CleanSpace:
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkFree (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
        chunkStat->chunkStat = fsChunk_S_Allocating;
        atomic_inc(&chunkStat->num_allocating);
        action = fsChunk_A_DO_Alloc;
        break;
    case fsChunk_E_AllocDONE:
        break;
    case fsChunk_E_InvalidateSpace:
        chunkStat->chunkStat = fsChunk_S_Invalidated;
        action = fsChunk_A_DO_Inval;
        break;
    case fsChunk_E_ReportFree:
    case fsChunk_E_CleanSpace:
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkAllocating (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
        chunkStat->chunkStat = fsChunk_S_Allocating;
        atomic_inc(&chunkStat->num_allocating);
        action = fsChunk_A_DO_Alloc;
        break;
    case fsChunk_E_AllocDONE:
        if (atomic_sub_return(1, &chunkStat->num_allocating) == 0) {
            chunkStat->chunkStat = fsChunk_S_Free;
        }
        break;
    case fsChunk_E_InvalidateSpace:
        action = fsChunk_A_DO_Retry;
        break;
    case fsChunk_E_ReportFree:
    case fsChunk_E_CleanSpace:
        break;
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkInval (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
    case fsChunk_E_AllocDONE:
    case fsChunk_E_InvalidateSpace:
        break;
    case fsChunk_E_ReportFree:
        chunkStat->chunkStat = fsChunk_S_Reporting;
        action = fsChunk_A_DO_Report;
        break;
    case fsChunk_E_CleanSpace:
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkReport (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
    case fsChunk_E_AllocDONE:
    case fsChunk_E_InvalidateSpace:
    case fsChunk_E_ReportFree:
        break;
    case fsChunk_E_CleanSpace:
        chunkStat->chunkStat = fsChunk_S_Removable;
        action = fsChunk_A_DO_Clean;
        break;
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkRemovable (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event) {

    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (event) {
    case fsChunk_E_AllocSpace:
    case fsChunk_E_AllocDONE:
    case fsChunk_E_InvalidateSpace:
    case fsChunk_E_ReportFree:
    case fsChunk_E_CleanSpace:
    case fsChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static fschunk_action_t _update_fschunkStat (fs_chunkStat_fsm_t *chunkStat,
        fschunk_TRevent_t event)
{
    fschunk_action_t action;

    action = fsChunk_A_NoAction;
    switch (chunkStat->chunkStat) {
    case fsChunk_S_INIT:
        action = _update_fschunkInit(chunkStat, event);
        break;
    case fsChunk_S_Free:
        action = _update_fschunkFree(chunkStat, event);
        break;
    case fsChunk_S_Allocating:
        action = _update_fschunkAllocating(chunkStat, event);
        break;
    case fsChunk_S_Invalidated:
        action = _update_fschunkInval(chunkStat, event);
        break;
    case fsChunk_S_Reporting:
        action = _update_fschunkReport(chunkStat, event);
        break;
    case fsChunk_S_Removable:
        action = _update_fschunkRemovable(chunkStat, event);
        break;
    case fsChunk_S_End:
    case fsChunk_S_Unknown:
    default:
        break;
    }

    return action;
}

fschunk_action_t update_fschunk_state (fs_chunkStat_fsm_t *chunkStat, fschunk_TRevent_t event)
{
    fschunk_action_t ret;

    ret = _update_fschunkStat(chunkStat, event);

    return ret;
}

int8_t *get_fschunkStat_action_str (fschunk_action_t action)
{
    int8_t *str;

    str = NULL;

    if (action >= fsChunk_A_NoAction && action < fsChunk_A_Unknown) {
        return fsChunkAction_str[action];
    }

    return fsChunkAction_str[fsChunk_A_Unknown];
}

fschunk_stat_t get_fschunk_state (fs_chunkStat_fsm_t *chunkStat)
{
    fschunk_stat_t stat;

    gain_fschunk_state_lock(chunkStat);
    stat = chunkStat->chunkStat;
    rel_fschunk_state_lock(chunkStat);

    return stat;
}

int8_t *get_fschunk_state_str (fs_chunkStat_fsm_t *chunkStat)
{
    int8_t *str;
    fschunk_stat_t stat;

    str = NULL;
    stat = get_fschunk_state(chunkStat);

    if (stat >= fsChunk_S_INIT && stat < fsChunk_S_Unknown) {
        return fsChunkStat_str[stat];
    }

    return fsChunkStat_str[fsChunk_S_Unknown];
}

void gain_fschunk_state_lock (fs_chunkStat_fsm_t *chunkStat)
{
    spin_lock(&chunkStat->chunk_slock);
}

void rel_fschunk_state_lock (fs_chunkStat_fsm_t *chunkStat)
{
    spin_unlock(&chunkStat->chunk_slock);
}

void init_fschunk_state_fsm (fs_chunkStat_fsm_t *chunkStat)
{
    chunkStat->chunkStat = fsChunk_S_INIT;
    atomic_set(&chunkStat->num_allocating, 0);
    spin_lock_init(&chunkStat->chunk_slock);

    gain_fschunk_state_lock(chunkStat);
    chunkStat->chunkStat = fsChunk_S_Free;
    rel_fschunk_state_lock(chunkStat);
}

/*************** aschunk state machine *******************/

static aschunk_action_t _update_aschunkInit (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event) {

    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (event) {
    case asChunk_E_Commit:
    case asChunk_E_FlushSpace:
    case asChunk_E_CleanSpace:
    case asChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static aschunk_action_t _update_aschunkAlloc (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event) {

    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (event) {
    case asChunk_E_Commit:
        chunkStat->chunkStat = asChunk_S_Committed;
        action = asChunk_A_DO_Commit;
        break;
    case asChunk_E_FlushSpace:
    case asChunk_E_CleanSpace:
    case asChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static aschunk_action_t _update_aschunkCommit (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event) {

    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (event) {
    case asChunk_E_Commit:
        break;
    case asChunk_E_FlushSpace:
        chunkStat->chunkStat = asChunk_S_Flushing;
        action = asChunk_A_DO_Flush;
        break;
    case asChunk_E_CleanSpace:
    case asChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static aschunk_action_t _update_aschunkFlushing (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event) {

    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (event) {
    case asChunk_E_Commit:
    case asChunk_E_FlushSpace:
        break;
    case asChunk_E_CleanSpace:
        chunkStat->chunkStat = asChunk_S_Removable;
        action = asChunk_A_DO_Clean;
        break;
    case asChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static aschunk_action_t _update_aschunkRemovable (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event) {

    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (event) {
    case asChunk_E_Commit:
    case asChunk_E_FlushSpace:
    case asChunk_E_CleanSpace:
    case asChunk_E_Unknown:
    default:
        break;
    }

    return action;
}

static aschunk_action_t _update_aschunkStat (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event)
{
    aschunk_action_t action;

    action = asChunk_A_NoAction;
    switch (chunkStat->chunkStat) {
    case asChunk_S_INIT:
        action = _update_aschunkInit(chunkStat, event);
        break;
    case asChunk_S_Allocated:
        action = _update_aschunkAlloc(chunkStat, event);
        break;
    case asChunk_S_Committed:
        action = _update_aschunkCommit(chunkStat, event);
        break;
    case asChunk_S_Flushing:
        action = _update_aschunkFlushing(chunkStat, event);
        break;
    case asChunk_S_Removable:
        action = _update_aschunkRemovable(chunkStat, event);
        break;
    case asChunk_S_End:
    case asChunk_S_Unknown:
    default:
        break;
    }

    return action;
}

aschunk_action_t update_aschunk_state (as_chunkStat_fsm_t *chunkStat,
        aschunk_TRevent_t event)
{
    aschunk_action_t ret;

    ret = _update_aschunkStat(chunkStat, event);

    return ret;
}

int8_t *get_aschunkStat_action_str (aschunk_action_t action)
{
    int8_t *str;

    str = NULL;

    if (action >= asChunk_A_NoAction && action < asChunk_A_Unknown) {
        return asChunkAction_str[action];
    }

    return asChunkAction_str[asChunk_A_Unknown];
}

aschunk_stat_t get_aschunk_state (as_chunkStat_fsm_t *chunkStat)
{
    aschunk_stat_t stat;

    gain_aschunk_state_lock(chunkStat);
    stat = chunkStat->chunkStat;
    rel_aschunk_state_lock(chunkStat);

    return stat;
}

int8_t *get_aschunk_state_str (as_chunkStat_fsm_t *chunkStat)
{
    int8_t *str;
    aschunk_stat_t stat;

    str = NULL;
    stat = get_aschunk_state(chunkStat);

    if (stat >= asChunk_S_INIT && stat < asChunk_S_Unknown) {
        return asChunkStat_str[stat];
    }

    return asChunkStat_str[asChunk_S_Unknown];
}

void gain_aschunk_state_lock (as_chunkStat_fsm_t *chunkStat)
{
    spin_lock(&chunkStat->chunk_slock);
}

void rel_aschunk_state_lock (as_chunkStat_fsm_t *chunkStat)
{
    spin_unlock(&chunkStat->chunk_slock);
}

void init_aschunk_state_fsm (as_chunkStat_fsm_t *chunkStat)
{
    chunkStat->chunkStat = asChunk_S_INIT;
    spin_lock_init(&chunkStat->chunk_slock);

    gain_aschunk_state_lock(chunkStat);
    chunkStat->chunkStat = asChunk_S_Allocated;
    rel_aschunk_state_lock(chunkStat);
}

#endif
