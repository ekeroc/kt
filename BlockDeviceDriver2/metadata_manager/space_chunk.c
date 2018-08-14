/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk.c
 *
 */
#include <linux/version.h>
#include <linux/kthread.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "metadata_manager.h"

#ifdef DISCO_PREALLOC_SUPPORT

#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager.h"

as_chunk_t *fschunk_alloc_space (fs_chunk_t *chunk, uint64_t lb_s, uint32_t lb_len)
{
    as_chunk_t *aschunk;
    fschunk_action_t action;
    int32_t aschunk_mpoolID, allocSize, i;

    aschunk_mpoolID = get_asChunkMemPoolID();
    aschunk = aschunk_alloc_chunk(aschunk_mpoolID);
    if (IS_ERR_OR_NULL(aschunk)) {
        return NULL;
    }

    gain_fschunk_state_lock(&chunk->chunkState);
    action = update_fschunk_state(&chunk->chunkState, fsChunk_E_AllocSpace);
    rel_fschunk_state_lock(&chunk->chunkState);

    if (fsChunk_A_DO_Alloc != action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail transit fschunk state action DO_ALLOC\n");
        return NULL;
    }

    allocSize = (chunk->chunkLen >= lb_len) ? lb_len : chunk->chunkLen;
    aschunk_init_chunk(aschunk, chunk->chunkID, lb_s, allocSize,
            chunk->HBID_s, chunk->num_replica);
    chunk->chunkLen -= allocSize;
    chunk->HBID_s += allocSize;

    for (i = 0; i < chunk->num_replica; i++) {
        aschunk_init_chunk_DNLoc(aschunk, i, chunk->chunk_DNLoc[i].ipaddr,
                chunk->chunk_DNLoc[i].port, chunk->chunk_DNLoc[i].rbid,
                chunk->chunk_DNLoc[i].offset);
        chunk->chunk_DNLoc[i].offset += allocSize;
    }

    gain_fschunk_state_lock(&chunk->chunkState);
    action = update_fschunk_state(&chunk->chunkState, fsChunk_E_AllocDONE);
    rel_fschunk_state_lock(&chunk->chunkState);

    //fschunk_show_chunk(chunk);

    //aschunk_show_chunk(aschunk);
    return aschunk;
}

int32_t fschunk_cleanup_chunk (fs_chunk_t *chunk)
{
    int32_t ret;
    fschunk_action_t action;

    gain_fschunk_state_lock(&chunk->chunkState);
    action = update_fschunk_state(&chunk->chunkState, fsChunk_E_CleanSpace);
    rel_fschunk_state_lock(&chunk->chunkState);

    ret = 0;

    if (fsChunk_A_DO_Clean != action) {
        ret = -EPERM;
    }

    return ret;
}

int32_t fschunk_report_unuse (fs_chunk_t *chunk, uint64_t *max_hbids)
{
    int32_t ret;
    fschunk_action_t action;

    gain_fschunk_state_lock(&chunk->chunkState);
    action = update_fschunk_state(&chunk->chunkState, fsChunk_E_ReportFree);
    rel_fschunk_state_lock(&chunk->chunkState);

    ret = 0;

    if (fsChunk_A_DO_Report == action) {
        *max_hbids = chunk->HBID_s;
    } else {
        ret = -EPERM;
        *max_hbids = 0;
    }

    return ret;
}

int32_t fschunk_invalidate_chunk (fs_chunk_t *chunk)
{
#define MAX_BUSY_WAIT_CNT   1000
    int32_t ret, count;
    bool doRetry;
    fschunk_action_t action;

    ret = 0;
    count = 0;

INVAL_FSCHUNK_AGAIN:
    doRetry = false;
    gain_fschunk_state_lock(&chunk->chunkState);

    action = update_fschunk_state(&chunk->chunkState, fsChunk_E_InvalidateSpace);

    rel_fschunk_state_lock(&chunk->chunkState);

    switch (action) {
    case fsChunk_A_DO_Inval:
        ret = 0;
        break;
    case fsChunk_A_DO_Retry:
        doRetry = true;
        break;
    case fsChunk_A_NoAction:
    case fsChunk_A_DO_Alloc:
    case fsChunk_A_DO_Report:
    case fsChunk_A_DO_Clean:
    case fsChunk_A_Unknown:
    default:
        ret = -EPERM;
        break;
    }

    if (doRetry) {
        count++;
        if (count == MAX_BUSY_WAIT_CNT) {
            yield();
            count = 0;
        }

        goto INVAL_FSCHUNK_AGAIN;
    }

    return ret;
}

fs_chunk_t *fschunk_alloc_chunk (int32_t mpoolID)
{
    return (fs_chunk_t *)discoC_malloc_pool(mpoolID);
}

void fschunk_free_chunk (int32_t mpoolID, fs_chunk_t *chunk)
{
    discoC_free_pool(mpoolID, chunk);
}

void fschunk_init_chunk (fs_chunk_t *fsChunk, uint64_t chunkID, uint32_t len,
        uint64_t hbid_s, uint16_t numReplica)
{
    fsChunk->chunkID = chunkID;
    fsChunk->chunkLen = len;
    fsChunk->HBID_s = hbid_s;
    fsChunk->num_replica = numReplica;

    init_fschunk_state_fsm(&fsChunk->chunkState);
    INIT_LIST_HEAD(&fsChunk->entry_spacePool);
}

void fschunk_init_chunk_DNLoc (fs_chunk_t *fsChunk, uint16_t bkIdx,
        uint32_t ipaddr, uint32_t port, uint32_t rbid, uint32_t offset)
{
    struct meta_cache_dn_loc *dnLoc;

    dnLoc = (struct meta_cache_dn_loc *)&(fsChunk->chunk_DNLoc[bkIdx]);
    dnLoc->ipaddr = ipaddr;
    dnLoc->port = port;
    dnLoc->rbid = rbid;
    dnLoc->offset = offset;
}

void fschunk_show_chunk (fs_chunk_t *fsChunk)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct meta_cache_dn_loc *dnLoc;
    int32_t i;

    dms_printk(LOG_LVL_INFO, "DMSC INFO fs chunk "
            "ID %llu len %u hbid_s %llu numReplic %u state %s\n",
            fsChunk->chunkID, fsChunk->chunkLen, fsChunk->HBID_s,
            fsChunk->num_replica, get_fschunk_state_str(&fsChunk->chunkState));
    for (i = 0; i < fsChunk->num_replica; i++) {
        dnLoc = &(fsChunk->chunk_DNLoc[i]);
        ccma_inet_aton(dnLoc->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_INFO, "DMSC INFO DN[%d] %s %u %u %u\n",
                i, ipv4addr_str, dnLoc->port, dnLoc->rbid, dnLoc->offset);
    }
}

int32_t fschunk_dump_chunk (fs_chunk_t *fsChunk, int8_t *buff, int32_t buff_size)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct meta_cache_dn_loc *dnLoc;
    int32_t i, len;

    len = 0;
    len += sprintf(buff + len, "  fschunk ID %llu len %u hbid_s %llu replica %u state %s\n",
            fsChunk->chunkID, fsChunk->chunkLen, fsChunk->HBID_s,
            fsChunk->num_replica, get_fschunk_state_str(&fsChunk->chunkState));
    if (len >= buff_size) {
        return len;
    }

    for (i = 0; i < fsChunk->num_replica; i++) {
        dnLoc = &(fsChunk->chunk_DNLoc[i]);
        ccma_inet_aton(dnLoc->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        len += sprintf(buff + len, "    DN[%d] %s %u %u %u\n",
                i, ipv4addr_str, dnLoc->port, dnLoc->rbid, dnLoc->offset);
        if (len >= buff_size) {
            break;
        }
    }

    return len;
}

/************* allocated chunk state *************/
int32_t aschunk_flush_chunk (as_chunk_t *asChunk)
{
    int32_t ret;
    aschunk_action_t action;

    gain_aschunk_state_lock(&asChunk->chunkState);
    action = update_aschunk_state(&asChunk->chunkState, asChunk_E_FlushSpace);
    rel_aschunk_state_lock(&asChunk->chunkState);

    ret = 0;

    if (asChunk_A_DO_Flush != action) {
        ret = -EPERM;
    }

    return ret;
}

int32_t aschunk_commit_chunk (as_chunk_t *asChunk)
{
    int32_t ret;
    aschunk_action_t action;

    gain_aschunk_state_lock(&asChunk->chunkState);
    action = update_aschunk_state(&asChunk->chunkState, asChunk_E_Commit);
    rel_aschunk_state_lock(&asChunk->chunkState);

    ret = 0;

    if (asChunk_A_DO_Commit != action) {
        ret = -EPERM;
    }

    return ret;
}

void aschunk_fill_metadata (as_chunk_t *asChunk, metaD_item_t *myMDItem)
{
    struct meta_cache_dn_loc *dnloc_src;
    int32_t i;

    memset(myMDItem, 0, sizeof(metaD_item_t));
    MData_init_MDItem(myMDItem, asChunk->lbid_s, asChunk->lb_len, asChunk->num_replica,
            MData_VALID, false, false, true);

    for (i = 0; i < asChunk->lb_len; i++) {
        myMDItem->arr_HBID[i] = asChunk->hbid_s + i;
    }

    //TODO: should check DNState when query
    for (i = 0; i < asChunk->num_replica; i++) {
        dnloc_src = &(asChunk->chunk_DNLoc[i]);

        MData_init_DNLoc(&myMDItem->udata_dnLoc[i],
                dnloc_src->ipaddr, dnloc_src->port, 1);
        myMDItem->udata_dnLoc[i].rbid_len_offset[0] =
                compose_triple(dnloc_src->rbid, asChunk->lb_len, dnloc_src->offset);
    }
}

void aschunk_show_chunk (as_chunk_t *asChunk)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct meta_cache_dn_loc *dnLoc;
    int32_t i;

    dms_printk(LOG_LVL_INFO, "DMSC INFO allocated chunk "
            "ID %llu lb info (%llu %u) hbid_s %llu numReplic %u state %s\n",
            asChunk->fs_chunkID, asChunk->lbid_s, asChunk->lb_len,
            asChunk->hbid_s, asChunk->num_replica,
            get_aschunk_state_str(&asChunk->chunkState));
    for (i = 0; i < asChunk->num_replica; i++) {
        dnLoc = &(asChunk->chunk_DNLoc[i]);
        ccma_inet_aton(dnLoc->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_INFO, "DMSC INFO DN[%d] %s %u %u %u state %d\n",
                i, ipv4addr_str, dnLoc->port, dnLoc->rbid, dnLoc->offset,
                asChunk->chunk_DNLoc_state[i]);
    }
}

void aschunk_free_chunk (int32_t mpoolID, as_chunk_t *chunk)
{
    discoC_free_pool(mpoolID, chunk);
}

void aschunk_init_chunk_DNLoc (as_chunk_t *asChunk, uint16_t bkIdx,
        uint32_t ipaddr, uint32_t port, uint32_t rbid, uint32_t offset)
{
    struct meta_cache_dn_loc *dnLoc;

    dnLoc = (struct meta_cache_dn_loc *)&(asChunk->chunk_DNLoc[bkIdx]);
    dnLoc->ipaddr = ipaddr;
    dnLoc->port = port;
    dnLoc->rbid = rbid;
    dnLoc->offset = offset;
    asChunk->chunk_DNLoc_state[bkIdx] = true;
}

void aschunk_init_chunk (as_chunk_t *asChunk, uint64_t chunkID,
        uint64_t lb_s, uint32_t lb_len, uint64_t hbid_s, uint16_t numReplica)
{
    asChunk->fs_chunkID = chunkID;
    asChunk->lbid_s = lb_s;
    asChunk->lb_len = lb_len;
    asChunk->hbid_s = hbid_s;
    asChunk->num_replica = numReplica;

    memset((int8_t *)(asChunk->chunk_DNLoc_state), 0, sizeof(bool)*MAX_NUM_REPLICA);
    init_aschunk_state_fsm(&asChunk->chunkState);
    INIT_LIST_HEAD(&asChunk->entry_allocPool);
}

as_chunk_t *aschunk_alloc_chunk (int32_t mpoolID)
{
    return (as_chunk_t *)discoC_malloc_pool_nowait(mpoolID);
}

int32_t aschunk_dump_chunk (as_chunk_t *asChunk, int8_t *buff, int32_t buff_size)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    struct meta_cache_dn_loc *dnLoc;
    int32_t i, len;

    len = 0;
    len += sprintf(buff + len,
            "  aschunk ID %llu hbid_s %llu lb %llu %u replica %u state %s\n",
            asChunk->fs_chunkID, asChunk->hbid_s,
            asChunk->lbid_s, asChunk->lb_len, asChunk->num_replica,
            get_aschunk_state_str(&asChunk->chunkState));
    for (i = 0; i < asChunk->num_replica; i++) {
        dnLoc = &(asChunk->chunk_DNLoc[i]);
        ccma_inet_aton(dnLoc->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        len += sprintf(buff + len, "    DN[%d] %s %u %u %u state %d\n",
                i, ipv4addr_str, dnLoc->port, dnLoc->rbid, dnLoc->offset,
                asChunk->chunk_DNLoc_state[i]);
        if (len >= buff_size) {
            break;
        }
    }

    return len;
}

#endif
