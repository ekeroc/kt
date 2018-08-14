/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_chrdev.c
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include "common/discoC_sys_def.h"
#include "common/common_util.h"
#include "common/dms_kernel_version.h"
#include "common/common.h"
#include "common/discoC_mem_manager.h"
#include "common/thread_manager.h"
#include "metadata_manager/metadata_manager.h"
#include "metadata_manager/metadata_cache.h"
#include "common/dms_client_mm.h"
#include "discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_manager/payload_manager_export.h"
#include "io_manager/discoC_IO_Manager.h"
#include "connection_manager/conn_manager_export.h"
#include "drv_fsm.h"
#include "drv_main.h"
#include "drv_main_api.h"
#include "config/dmsc_config.h"
#include "vdisk_manager/volume_manager.h"
#include "vdisk_manager/volume_operation_api.h"
#include "flowcontrol/FlowControl.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "metadata_manager/space_manager_export_api.h"
#endif

/*
 * long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
 * int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
 *
 * ioctl_num: number and param for ioctl
 */
static int32_t _do_ioctl_config (uint64_t ioctl_param)
{
    config_para_t config_para;
    int32_t ret;

    ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    ret = copy_from_user(&config_para, (void __user *)ioctl_param,
                                                         sizeof(config_para_t));
#else
    copy_from_user(&config_para, (void __user *)ioctl_param,
                                                         sizeof(config_para_t));
#endif

    if (ret) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERROR fail copy config from User daemon\n");
        DMS_WARN_ON(true);
        return ret;
    }

    set_config_item(config_para.item, citem_len);

    return ret;
}

static int32_t _do_ioctl_init (uint64_t ioctl_param)
{
    return init_drv_component();
}

bool is_drv_running (bool gain_ref)
{
    bool ret = false;

    mutex_lock(&discoC_drv.drv_lock);
    if (Drv_running == discoC_drv.drv_state) {
        ret = true;
        if (gain_ref) {
            atomic_inc(&discoC_drv.drv_busy_cnt);
        }
    }
    mutex_unlock(&discoC_drv.drv_lock);

    return ret;
}

static bool try_stop_drv (void)
{
    bool ret;
    drv_state_op_t old_stat;

    mutex_lock(&discoC_drv.drv_lock);

    /*
     * NOTE: When user issue a reset drv command, busy cnt already be inc
     * TODO: Should we increase the busy cnt when volume be open?
     */
    if (atomic_read(&discoC_drv.drv_busy_cnt) > 1) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail to shutdown drv, some volumes be used\n");
        ret = false;
    } else {
        old_stat = update_drv_state(&discoC_drv.drv_state, Drv_turn_off);
        switch (old_stat) {
        case Drv_running:
            ret = true;
            break;
        case Drv_init:
        case Drv_shuting_down:
        case Drv_shutdown:
            ret = false;
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO fail to shutdown drv, drv isn't running\n");
            break;
        default:
            ret = false;
            dms_printk(LOG_LVL_WARN, "DMSC WARN unknown drv state\n");
            DMS_WARN_ON(true);
            break;
        }
    }
    mutex_unlock(&discoC_drv.drv_lock);

    return ret;
}

static int32_t _do_ioctl_shutdown (uint64_t ioctl_param)
{
    int32_t ret, cnt_ligner_vol;

    /*
     * TODO: how to prevent
     * Lego: Preventing receive attach / detach commands at the same time
     * when we are trying to reset driver
     */

    if (try_stop_drv() == false) {
        return RESET_DRIVER_BUSY;
    }

    //TODO: Should notify nn that volume be detach
    cnt_ligner_vol = detach_all_volumes();
    if (cnt_ligner_vol != 0) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO reset driver fail, some volumes be usd = %d\n",
                cnt_ligner_vol);
        (void)update_drv_state_lock(&discoC_drv.drv_state, &discoC_drv.drv_lock,
                Drv_off_fail);
        ret = RESET_DRIVER_BUSY;
    } else {
        rel_drv_component();

        ret = RESET_DRIVER_OK;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO shutdown driver result = %d\n", ret);
    return ret;
}

static attach_response_t _do_ioctl_attvol (uint64_t ioctl_param)
{
    struct dms_volume_info vinfo;
    attach_response_t retcode;
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    int32_t cpret;
    cpret = copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#else
    copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#endif

    ccma_inet_aton(vinfo.nnSrv_IPV4addr, ipv4addr_str, IPV4ADDR_NM_LEN);
    dms_printk(LOG_LVL_INFO,
            "DMSC INFO attach NN %s %s volume %u capacity %llu replica_factor %u "
            "Share Volume %d\n", vinfo.nnSrv_nm, ipv4addr_str,
            vinfo.volumeID, vinfo.capacity,
            vinfo.replica_factor, vinfo.share_volume);

    retcode = attach_volume_default_gd(vinfo.nnSrv_nm, vinfo.nnSrv_IPV4addr,
            vinfo.volumeID, vinfo.capacity,
            vinfo.share_volume, dms_client_config->enable_metadata_cache,
            RW_PERMIT, vinfo.replica_factor);

    dms_printk(LOG_LVL_INFO, "DMSC INFO attach vol DONE NN %s %s volid %u ret %u\n",
            vinfo.nnSrv_nm, ipv4addr_str, vinfo.volumeID, retcode);

    return retcode;
}

static int32_t _do_ioctl_attvol_done (long ioctl_param)
{
    struct dms_volume_info vinfo;
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    int32_t cpret;
    cpret = copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#else
    copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#endif

    ccma_inet_aton(vinfo.nnSrv_IPV4addr, ipv4addr_str, IPV4ADDR_NM_LEN);
    set_volume_state_attach_done(vinfo.nnSrv_IPV4addr, (uint32_t)vinfo.volumeID);

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO attach done: receive attach complete "
            "set volume state as volume ready %s %s %d\n",
            vinfo.nnSrv_nm, ipv4addr_str, vinfo.volumeID);
    return 0;
}

static detach_response_t _do_ioctl_detvol (long ioctl_param)
{
    struct dms_volume_info vinfo;
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    detach_response_t ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    int32_t cpret;
    cpret = copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#else
    copy_from_user(&vinfo, (void __user *)ioctl_param, sizeof(struct dms_volume_info));
#endif

    ccma_inet_aton(vinfo.nnSrv_IPV4addr, ipv4addr_str, IPV4ADDR_NM_LEN);

    dms_printk(LOG_LVL_INFO, "DMSC INFO detach NN %s %s vol %u\n",
            vinfo.nnSrv_nm, ipv4addr_str, vinfo.volumeID);

    ret = detach_volume(vinfo.nnSrv_IPV4addr, (uint32_t)vinfo.volumeID);

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO detach NN %s %s vol %u DONE ret %u\n",
            vinfo.nnSrv_nm, ipv4addr_str, vinfo.volumeID, ret);
    return ret;
}

static int32_t _do_ioctl_cleanovw (long ioctl_param)
{
    uint32_t vol_id;
    detach_response_t ret;

    vol_id = (uint32_t)ioctl_param;
    dms_printk(LOG_LVL_INFO,
            "DMSC INFO reset ovw volume %u\n", vol_id);

    ret = (int32_t)clear_volume_ovw(vol_id);

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO reset ovw done volume %u ret = %d\n", vol_id, ret);
    return ret;
}

//TODO call volume manager to perform invalidate cache
static int32_t _clean_md_cache (struct ResetCacheInfo *reset_cache_info)
{
    int32_t retCode, i;
    uint64_t *LBID_List = NULL;

    retCode = INVALIDATE_CACHE_ACK_OK;
    if (unlikely(IS_ERR_OR_NULL(reset_cache_info))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN clear metadata cache fail, param is NULL\n");
        DMS_WARN_ON(true);
        return INVALIDATE_CACHE_ACK_ERR;
    }

    switch (reset_cache_info->SubOpCode) {
    case SUBOP_INVALIDATECACHE_BYLBIDS:
        dms_printk(LOG_LVL_INFO,
                "DMSC DEBUG clear cache by LBIDs volume id = %llu\n",
                reset_cache_info->u.LBIDs.VolumeID);
        dms_printk(LOG_LVL_INFO,
                "DMSC DEBUG clear following LBIDs total num = %u\n",
                reset_cache_info->u.LBIDs.NumOfLBID);

        LBID_List = (uint64_t *)
                discoC_mem_alloc(
                        sizeof(uint64_t)*(reset_cache_info->u.LBIDs.NumOfLBID),
                        GFP_ATOMIC);
        if (LBID_List == NULL) {
            dms_printk(LOG_LVL_WARN,  "DMSC WARN cannot allocate memory for "
                    "Invalidate cache by lbids, memory size=%lu\n",
                    sizeof(uint64_t)*(reset_cache_info->u.LBIDs.NumOfLBID));
            retCode = INVALIDATE_CACHE_ACK_ERR;
            break;
        }
        
        dms_printk(LOG_LVL_INFO,
                "DMSC DEBUG before copy_from_user() for LBID list\n");
        retCode = copy_from_user(LBID_List, (void __user *)reset_cache_info->u.LBIDs.LBID_List,
                        sizeof(uint64_t)*(reset_cache_info->u.LBIDs.NumOfLBID));
        dms_printk(LOG_LVL_INFO,
                "DMSC DEBUG after copy_from_user() for LBID list, ret=%d\n", retCode);
        
        for (i = 0; i < reset_cache_info->u.LBIDs.NumOfLBID; i++) {
            dms_printk(LOG_LVL_INFO,  "DMSC DEBUG clear LBID: %llu\n",
                      LBID_List[i]);
            retCode = inval_volume_MDCache_LBID(
                                      reset_cache_info->u.LBIDs.VolumeID,
                                      LBID_List[i], 1);
            dms_printk(LOG_LVL_INFO,
                    "DMSC DEBUG clean result=%d for LBID=%llu\n",
                    retCode, LBID_List[i]);
        }
        retCode =  INVALIDATE_CACHE_ACK_OK;

        break;

    case SUBOP_INVALIDATECACHE_BYHBIDS:
        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG clear cache by HBIDs\n");
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG clear following HBIDs total num = %d\n",
                reset_cache_info->u.HBIDs.NumOfHBID);

        for (i = 0; i < reset_cache_info->u.HBIDs.NumOfHBID; i++) {
            dms_printk(LOG_LVL_DEBUG,  "DMSC DEBUG clear HBID: %llu\n",
                    reset_cache_info->u.HBIDs.HBID_List[i]);
            retCode =
                    inval_volume_MDCache_HBID(reset_cache_info->u.HBIDs.HBID_List[i]);
            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG clean result=%d for HBID=%llu\n",
                    retCode, reset_cache_info->u.HBIDs.HBID_List[i]);
        }
        retCode =  INVALIDATE_CACHE_ACK_OK;

        break;

    case SUBOP_INVALIDATECACHE_BYVOLID:
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG clear cache by HBIDs with volume id = %llu\n",
                reset_cache_info->u.LBIDs.VolumeID);
        retCode = inval_volume_MDCache_volID(reset_cache_info->u.LBIDs.VolumeID);
        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG  clean result=%d\n", retCode);

        if (retCode == -1) {
            retCode = INVALIDATE_CACHE_ACK_ERR;
        }

        break;
    case SUBOP_INVALIDATECACHE_BYDNNAME:
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG clear cache by DN Name %s with port = %d\n",
                reset_cache_info->u.DNName.Name,
                reset_cache_info->u.DNName.Port);
        retCode = inval_volume_MDCache_DN(reset_cache_info->u.DNName.Name,
                reset_cache_info->u.DNName.NameLen,
                reset_cache_info->u.DNName.Port);
        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG  clean result=%d\n", retCode);

        if (retCode == -1) {
            retCode = INVALIDATE_CACHE_ACK_ERR;
        }

        break;
    default:
        retCode = INVALIDATE_CACHE_ACK_ERR;
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknown reset cache subop %d\n",
                reset_cache_info->SubOpCode);
        DMS_WARN_ON(true);
        break;
    }

    return retCode;
}

static int32_t _do_ioctl_cleanmdcache (long ioctl_param)
{
    struct ResetCacheInfo *reset_cache_info;
    uint64_t *arr_hbids, *ptr_old;
    int32_t ret, numHBIDs;

    dms_printk(LOG_LVL_INFO, "DMSC INFO clean md cache start\n");

    reset_cache_info = (struct ResetCacheInfo *)
        discoC_mem_alloc(sizeof(struct ResetCacheInfo), GFP_ATOMIC);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    ret = copy_from_user(reset_cache_info, (void __user *)ioctl_param,
            sizeof(struct ResetCacheInfo));
#else
    copy_from_user(reset_cache_info, (void __user *)ioctl_param,
            sizeof(struct ResetCacheInfo));
#endif

    numHBIDs = reset_cache_info->u.HBIDs.NumOfHBID;
    arr_hbids = discoC_mem_alloc(sizeof(uint64_t)*numHBIDs, GFP_KERNEL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    ret = copy_from_user(arr_hbids, (void __user *)ioctl_param,
            sizeof(uint64_t)*numHBIDs);
#else
    copy_from_user(arr_hbids, (void __user *)ioctl_param,
            sizeof(uint64_t)*numHBIDs);
#endif

    ptr_old = reset_cache_info->u.HBIDs.HBID_List;
    reset_cache_info->u.HBIDs.HBID_List = arr_hbids;

    reset_cache_info->u.HBIDs.HBID_List = ptr_old;
    ret = _clean_md_cache(reset_cache_info);
    discoC_mem_free(reset_cache_info);
    discoC_mem_free(arr_hbids);
    reset_cache_info = NULL;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO clean md cache done ret = %d\n", ret);
    return ret;
}

static int32_t _do_ioctl_blockIO (long ioctl_param, bool blockIO)
{
    uint32_t vol_id;
    int32_t ret;

    vol_id = (uint32_t)ioctl_param;
    if (blockIO) {
        /*
         * TODO: Lego: Should we using blk_stop_queue blk_sync_queue and
         * blk_start_queue to replace the IO blocking action?
         */
        dms_printk(LOG_LVL_INFO, "DMSC INFO blok IO volume %u\n", vol_id);
        ret = (int32_t)set_volume_state_block_io(vol_id);
        dms_printk(LOG_LVL_INFO, "DMSC INFO blok IO done volume %u ret = %d\n",
                vol_id, ret);
    } else {
        dms_printk(LOG_LVL_INFO, "DMSC INFO unblok IO volume %u\n", vol_id);
        ret = (int32_t)set_volume_state_unblock_io(vol_id);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO unblok IO done volume %u ret = %d\n", vol_id, ret);
    }

    return ret;
}

static set_fea_resp_t _do_ioctl_setvolshare (long ioctl_param)
{
    struct dms_volume_info vinfo;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    int32_t cpret;
    cpret = copy_from_user(&vinfo, (void __user *)ioctl_param,
            sizeof(struct dms_volume_info));
#else
    copy_from_user(&vinfo, (void __user *)ioctl_param,
            sizeof(struct dms_volume_info));
#endif

    return set_volume_state_share(vinfo.volumeID, vinfo.share_volume);
}

static int32_t _do_ioctl_disable_rw_perm (long ioctl_param)
{
    uint32_t vol_id;

    vol_id = (long)ioctl_param;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO disable volume %u RW\n", vol_id);

    set_volume_rw_permission(vol_id, RW_NO_PERMIT);

    return 0;
}

static int32_t _do_ioctl_stopvol (long ioctl_param)
{
    uint32_t vol_id;
    int32_t ret;

    vol_id = (long)ioctl_param;
    dms_printk(LOG_LVL_INFO, "DMSC INFO stop volume %u\n", vol_id);
    ret = (uint32_t)stop_volume(vol_id);

    return ret;
}

static int32_t _do_ioctl_startvol (long ioctl_param)
{
    uint32_t vol_id;
    int32_t ret;

    vol_id = (long)ioctl_param;

    dms_printk(LOG_LVL_INFO, "DMSC INFO start volume %u\n", vol_id);
    ret = (uint32_t)restart_volume(vol_id);

    return ret;
}

static int32_t _get_CNDN_latencies (int8_t *buff)
{
    int32_t ret, cnt;
    int8_t *ptr;

    ret = 0;
    ptr = buff;

    *((int16_t *)ptr) = 0;
    ptr += sizeof(int16_t);

    cnt = get_dn_latency_info(ptr);
    *((int16_t *)buff) = cnt;

    if (cnt == 0) {
        ret = 1; //no any DN latency information
    }

    return ret;
}

static int32_t _get_CNDN_qlen (int8_t *buff)
{
    int32_t ret;

    ret = 0;

    *((int32_t *)buff) = get_num_DNUDataReq_waitSend();

    return ret;
}

static int32_t _get_vol_activeDN (int64_t volid, int8_t *buff)
{
    volume_device_t *vol;
    int8_t *ptr;
    int32_t ret, cnt;

    ret = 0;
    ptr = buff;

    *((int16_t *)ptr) = 0;
    ptr += sizeof(int16_t);

    vol = find_volume(volid);

    if (IS_ERR_OR_NULL(vol)) {
        return 2; //volume not exist
    }

    cnt = get_vol_active_dn(ptr, vol->vol_fc_group);

    if (cnt == 0) {
        return 1; //no active DN
    }

    *((int16_t *)buff) = cnt;
    return ret;
}

static int32_t _get_all_volumes (int8_t *buff)
{
    volume_device_t *vol;
    int8_t *ptr;
    int32_t numVols;

    numVols = 0;

    ptr = buff;
    vol = NULL;
    ptr += sizeof(int16_t);
    while (!IS_ERR_OR_NULL(vol = iterate_all_volumes(vol))) {
        *((int32_t *)ptr) = vol->vol_id;
        numVols++;
        ptr += sizeof(int32_t);
    }

    if (numVols == 0) {
        return 1; //no volumes
    }

    *((int16_t *)buff) = (int16_t)numVols;

    return 0;
}

static int32_t _do_ioctl_getcninfo (long ioctl_param)
{
    int8_t *databuff, *ptr;
    int16_t subopcode;
    int32_t ret, cpret;
    int64_t volid;

    databuff = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    cpret = copy_from_user(databuff, (void __user *)ioctl_param,
            sizeof(int8_t)*PAGE_SIZE);
#else
    copy_from_user(databuff, (void __user *)ioctl_param,
            sizeof(int8_t)*PAGE_SIZE);
#endif

    ptr = databuff;
    subopcode = *((int16_t *)ptr);
    ptr += sizeof(int16_t);

    switch (subopcode) {
    case CN_INFO_SUBOP_GetAllCNDNLatencies:
        ret = _get_CNDN_latencies(databuff);
        break;
    case CN_INFO_SUBOP_GetAllCNDNQLen:
        ret = _get_CNDN_qlen(databuff);
        break;
    case CN_INFO_SUBOP_GETVolActiveDN:
        volid = *((int64_t *)ptr);
        ret = _get_vol_activeDN(volid, databuff);
        break;
    case CN_INFO_SUBOP_GETAllVols:
        ret = _get_all_volumes(databuff);
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN wrong subopcode %u\n", subopcode);
        ret = 1002;
        break;
    }

    cpret = copy_to_user((void __user *)ioctl_param, databuff, sizeof(int8_t)*PAGE_SIZE);
    discoC_mem_free(databuff);

    return ret;
}

static int32_t _do_ioctl_inval_volfs (long ioctl_param)
{
    userD_prealloc_ioctl_cmd_t cmd;
    int32_t ret, cpret;

    ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    cpret = copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#else
    copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#endif

#ifdef DISCO_PREALLOC_SUPPORT
    cmd.max_unuse_hbid = 0;
    ret = Inval_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_inval_volfs not support now\n");
#else
    cmd.max_unuse_hbid = 0;
    ret = Inval_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_inval_volfs not support now\n");
#endif

    cpret = copy_to_user((void __user *)ioctl_param, &cmd,
            sizeof(userD_prealloc_ioctl_cmd_t));

    return ret;
}

static int32_t _do_ioctl_clean_volfs (long ioctl_param)
{
    userD_prealloc_ioctl_cmd_t cmd;
    int32_t ret, cpret;

    ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    cpret = copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#else
    copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#endif

#ifdef DISCO_PREALLOC_SUPPORT
    ret = Clean_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_clean_volfs not support now\n");
#else
    ret = Clean_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_clean_volfs not support now\n");
#endif

    cpret = copy_to_user((void __user *)ioctl_param, &cmd,
            sizeof(userD_prealloc_ioctl_cmd_t));

    return ret;
}

static int32_t _do_ioctl_flush_volfs (long ioctl_param)
{
    userD_prealloc_ioctl_cmd_t cmd;
    int32_t ret, cpret;

    ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    cpret = copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#else
    copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#endif

#ifdef DISCO_PREALLOC_SUPPORT
    ret = Flush_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_flush_volfs not support now\n");
#else
    ret = Flush_chunk_FAIL;
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_flush_volfs not support now\n");
#endif

    cpret = copy_to_user((void __user *)ioctl_param, &cmd,
            sizeof(userD_prealloc_ioctl_cmd_t));

    return ret;
}

static int32_t _do_ioctl_prealloc_cmd (long ioctl_param)
{
    userD_prealloc_ioctl_cmd_t cmd;
    int32_t ret, cpret;

    ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    cpret = copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#else
    copy_from_user(&cmd, (void __user *)ioctl_param,
            sizeof(userD_prealloc_ioctl_cmd_t));
#endif

#ifdef DISCO_PREALLOC_SUPPORT
    switch (cmd.subopcode) {
    case SUBOP_PREALLOC_AllocFreeSpace:
        sm_prealloc_volume_freespace(cmd.volumeID, 32, NULL, NULL);
        break;
    case SUBOP_PREALLOC_FlushMetadata:
        sm_report_volume_spaceMData(cmd.volumeID);
        break;
    case SUBOP_PREALLOC_ReturnFreeSpace:
        sm_invalidate_volume(cmd.volumeID);
        sm_report_volume_unusespace(cmd.volumeID);
        sm_cleanup_volume(cmd.volumeID);
        break;
    default:
        ret = Prealloc_cmd_FAIL;
        break;
    }
#else
    dms_printk(LOG_LVL_INFO, "DMSC INFO _do_ioctl_prealloc_cmd not support now\n");
    ret = Prealloc_cmd_FAIL;
#endif

    cpret = copy_to_user((void __user *)ioctl_param, &cmd,
            sizeof(userD_prealloc_ioctl_cmd_t));

    return ret;
}

/*
 * long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
 * int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
 *
 * ioctl_num: number and param for ioctl
 */
static int32_t _do_normal_ioctl (uint32_t ioctl_num, uint64_t ioctl_param)
{
    int32_t ret;

    if (is_drv_running(true) == false) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN driver isn't running\n");
        return DRIVER_ISNOT_WORKING;
    }

    switch (ioctl_num) {
    case IOCTL_RESET_DRIVER:
        dms_printk(LOG_LVL_INFO, "DMSC INFO IOCTL_shutdown_driver called\n");
        ret = _do_ioctl_shutdown(ioctl_param);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO shutdown driver called with result=%d\n", ret);

        break;

    case IOCTL_ATTACH_VOL:
        dms_printk(LOG_LVL_INFO, "DMSC INFO attach volume\n");
        ret = _do_ioctl_attvol(ioctl_param);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO attach volume done with result=%d\n", ret);
        break;

    case IOCTL_ATTACH_VOL_FINISH:
        ret = _do_ioctl_attvol_done(ioctl_param);
        break;

    case IOCTL_DETATCH_VOL:
        dms_printk(LOG_LVL_INFO, "DMSC INFO detach volume\n");
        ret = _do_ioctl_detvol(ioctl_param);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO detach volume done with result=%d\n", ret);
        break;

    case IOCTL_RESETOVERWRITTEN:
        ret = _do_ioctl_cleanovw(ioctl_param);
        break;

    case IOCTL_INVALIDCACHE:
        ret = _do_ioctl_cleanmdcache(ioctl_param);
        break;

    case IOCTL_BLOCK_VOLUME_IO:
        ret = _do_ioctl_blockIO(ioctl_param, true);
        break;

    case IOCTL_UNBLOCK_VOLUME_IO:
        ret = _do_ioctl_blockIO(ioctl_param, false);
        break;

    case IOCTL_SET_VOLUME_SHARE_FEATURE:
        ret = _do_ioctl_setvolshare(ioctl_param);
        break;

    case IOCTL_DISABLE_VOL_RW:
        ret = _do_ioctl_disable_rw_perm(ioctl_param);
        break;

    case IOCTL_STOP_VOL:
        ret = _do_ioctl_stopvol(ioctl_param);
        break;

    case IOCTL_RESTART_VOL:
        ret = _do_ioctl_startvol(ioctl_param);
        break;
    case IOCTL_GET_CN_INFO:
        ret = _do_ioctl_getcninfo(ioctl_param);
        break;
    case IOCTL_INVAL_VOL_FS:
        ret = _do_ioctl_inval_volfs(ioctl_param);
        break;
    case IOCTL_CLEAN_VOL_FS:
        ret = _do_ioctl_clean_volfs(ioctl_param);
        break;
    case IOCTL_FLUSH_VOL_FS:
        ret = _do_ioctl_flush_volfs(ioctl_param);
        break;
    case IOCTL_PREALLOC_CMD:
        ret = _do_ioctl_prealloc_cmd(ioctl_param);
        break;
    default:
        dms_printk(LOG_LVL_INFO, "DMC WANR unknown ioctl cmds %u\n",
                ioctl_num);
        DMS_WARN_ON(true);
        ret = -ENOTTY;
        break;
    }

    atomic_dec(&discoC_drv.drv_busy_cnt);

    return ret;
}

static int32_t chrdev_ioctl_gut (uint32_t ioctl_num, uint64_t ioctl_param)
{
    int32_t ret;

    ret = 0;

    switch (ioctl_num) {
    case IOCTL_CONFIG_DRV:
        ret = _do_ioctl_config(ioctl_param);
        break;

    case IOCTL_INIT_DRV:
        ret = _do_ioctl_init(ioctl_param);
        break;

//    case IOCTL_GET_DRV_STATE:
//        ret = _ioctl_get_drv_stat(ioctl_param);
//        break;

//    case IOCTL_REGISTER_DRV:
//        ret = _do_ioctl_reg(ioctl_param);
//        break;

//    case IOCTL_REMOVE_ALL_COMP:
//        ret = _do_ioctl_rel_all_comps(ioctl_param);
//        break;
    default:
        ret = _do_normal_ioctl(ioctl_num, ioctl_param);
        break;
    }

    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
long discoC_chrdev_ioctl (struct file *file, /* ditto */
                        unsigned int ioctl_num, /* number and param for ioctl */
                        unsigned long ioctl_param) {
    return (long)chrdev_ioctl_gut(ioctl_num, ioctl_param);
}
#else
int discoC_chrdev_ioctl (struct inode *inode, /* see include/linux/fs.h */
                       struct file *file, /* ditto */
                       unsigned int ioctl_num, /* number and param for ioctl */
                       unsigned long ioctl_param)
{
    return (long)chrdev_ioctl_gut(ioctl_num, ioctl_param);
}
#endif

struct file_operations chrdev_fops = {
owner:
    THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
unlocked_ioctl :
    discoC_chrdev_ioctl
#else
ioctl :
    discoC_chrdev_ioctl
#endif
};

int32_t init_chrdev (discoC_drv_t *mydrv)
{
    int32_t ret;

    ret = 0;

    if (register_chrdev(mydrv->chrdev_major_num, mydrv->chrdev_name,
            &chrdev_fops)) {
        dms_printk(LOG_LVL_ERR, "DMSC ERROR unable to reg char dev\n");
        DMS_WARN_ON(true);
        ret = -ENOMEM;
    }

    return ret;
}

void rel_chrdev (discoC_drv_t *mydrv)
{
    unregister_chrdev(mydrv->chrdev_major_num, mydrv->chrdev_name);
}


