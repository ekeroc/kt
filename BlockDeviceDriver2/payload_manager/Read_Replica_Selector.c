/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Read_Replica_Selector.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../flowcontrol/FlowControl.h"

/*
 * _PLMgr_RoundRobin_selectDN: select a DN with round robin way
 * @loc: metadata item that contains all DN information
 * @dnSeltor: a data structure record how many data read from target DN index (not DN) in metadata item
 * @max_read_bytes: how many bytes can be read from same DN. this is a threshold to select other DN
 *
 * Return: selected DN index for read
 *
 */
static uint32_t _PLMgr_RoundRobin_selectDN (metaD_item_t *loc,
        struct Read_DN_selector *dnSeltor, uint32_t max_read_bytes)
{
    uint32_t ret_idx;

    ret_idx = 0;

    spin_lock(&dnSeltor->readDN_lock);

    dnSeltor->readDN_numLB += loc->num_hbids;

    if (dnSeltor->readDN_numLB >= max_read_bytes) {
        (dnSeltor->readDN_idx)++;
        dnSeltor->readDN_numLB = 0;
    }

    ret_idx = dnSeltor->readDN_idx % loc->num_dnLoc;

    spin_unlock(&dnSeltor->readDN_lock);

    return ret_idx;
}

/*
 * _PLMgr_checkDN_health: check and find better DN to read
 * @loc: metadata item that contains all DN information
 * @selectDN: which DN index in metadata item for checking healthy
 *
 * Return: new selected DN index for read
 *
 */
static uint32_t _PLMgr_checkDN_health (metaD_item_t *loc, uint32_t selectDN)
{
    metaD_DNLoc_t *dnLoc;
    uint32_t chk_idx;
    int32_t chk_ret;

    chk_idx = selectDN;
    do {
        dnLoc = &(loc->udata_dnLoc[chk_idx]);
        chk_ret = Check_DN_Resources_Is_Available(dnLoc->ipaddr, dnLoc->port);
        if (RESOURCES_IS_FREE_TO_USE == chk_ret) {
            break;
        }

        chk_idx++;
        chk_idx = chk_idx % loc->num_dnLoc;
        if (chk_idx == selectDN) {
            /*
             * Lego: 20130726 All resources has some problems (Maybe not yet
             * construct a connection , or all lose connection, or all have
             * DN LUN IO stuck. still use current one
             */
            break;
        }
    } while (RESOURCES_IS_FREE_TO_USE != chk_ret);

    return chk_idx;
}

/*
 * PLMgr_Read_selectDN: select a DN for read with round robin. check whether
 *     selected DN is good one - DN is connected and no timeout happens on this DN
 *     (timeout mean low responsiveness).
 * @loc: metadata item that contains all DN information
 * @dnSeltor: a data structure record how many data read from target DN index (not DN) in metadata item
 * @max_read_bytes: how many bytes can be read from same DN. this is a threshold to select other DN
 * @read_rr_on: whehter read round robin is on
 *
 * Return: selected DN index for read
 *
 */
int32_t PLMgr_Read_selectDN (metaD_item_t *loc, struct Read_DN_selector *dnSeltor,
        uint32_t max_read_bytes, bool read_rr_on)
{
    int32_t retIndex, chk_index, chk_ret;

    retIndex = 0;
    chk_ret = 0;
    if (read_rr_on) {
        retIndex = _PLMgr_RoundRobin_selectDN(loc, dnSeltor, max_read_bytes);
    }

    chk_index = _PLMgr_checkDN_health(loc, retIndex);
    if (chk_index != retIndex) {
        retIndex = chk_index;

        if (read_rr_on) {
            spin_lock(&dnSeltor->readDN_lock);
            dnSeltor->readDN_idx = retIndex;
            dnSeltor->readDN_numLB = 0;
            spin_unlock(&dnSeltor->readDN_lock);
        }
    }

    return retIndex;
}




