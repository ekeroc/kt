/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_main.c
 *
 */


#ifndef __KERNEL__
#define __KERNEL__
#endif
#ifndef MODULE
#define MODULE
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/swap.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "common/discoC_sys_def.h"
#include "common/common_util.h"
#include "common/discoC_mem_manager.h"
#include "common/discoC_comm_main.h"
#include "common/dms_kernel_version.h"
#include "common/common.h"
#include "common/thread_manager.h"
#include "common/dms_client_mm.h"
#include "connection_manager/conn_manager_export.h"
#include "metadata_manager/metadata_manager.h"
#include "discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_manager/payload_manager_export.h"
#include "discoNN_client/discoC_NNC_Manager_api.h"

#include "io_manager/discoC_IO_Manager.h"
#include "io_manager/discoC_IO_Manager_export.h"
#include "vdisk_manager/volume_manager.h"
#include "vdisk_manager/volume_operation_api.h"
#include "flowcontrol/FlowControl.h"
#include "housekeeping.h"
#include "info_monitor/dms_sysinfo.h"
#include "discoNN_client/discoC_NNC_ovw.h"
#include "drv_fsm.h"
#include "drv_main.h"
#include "drv_main_private.h"
#include "drv_blkdev.h"
#include "drv_chrdev.h"
#include "config/dmsc_config.h"
#include "discoDN_simulator/discoDNSrv_simulator_export.h"
#include "discoNN_simulator/discoNNSrv_simulator_export.h"
#include "discoNNOVW_simulator/discoNNOVWSrv_simulator_export.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "metadata_manager/space_manager_export_api.h"
#endif

int32_t init_drv_component (void)
{
    uint32_t ret;
    //allow_signal(SIGINT);
    //allow_signal(SIGTERM);

    do {
        if ((ret = init_discoC_common(dms_client_config->max_nr_io_reqs))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail to init common module\n");
            break;
        }

        init_flow_control();

#ifdef DISCO_PREALLOC_SUPPORT
        init_space_manager();
#endif

        MDMgr_init_manager();
#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
        payload_cache_init();
#endif
        init_NNSimulator_manager();
        init_NNOVWSimulator_manager();
        init_DNSimulator_manager();

        //NOTE : Don't try to change initialize order.
        init_conn_manager();
        init_housekeeping_res();
        PLMgr_init_manager();  //NOTE: if init fail, should stop dn worker

        IOMgr_init_manager(); //NOTE: if init fail, need stop all io_thread

        NNClientMgr_init_manager();  //NOTE: if init fail, should close conn

        init_sys_procfs();  //NOTE: if init fail, should close sysprocfs

        ret = init_blkdev(&discoC_drv);
        if (ret < 0) {
            dms_printk(LOG_LVL_ERR,
                    "DMSC ERROR, unable to get major number, ret %d\n", ret);
            DMS_WARN_ON(true);
            //TODO : should release all data
            ret = -ENOMEM;
            break;
        } else {
            discoC_drv.blkdev_major_num = ret;
        }

        init_vol_manager(discoC_drv.blkdev_major_num, DEFAULT_MAX_NUM_OF_VOLUMES);
        //TODO: handle response

        update_drv_state(&discoC_drv.drv_state, Drv_init_done);
        initial_cleancache_thread();

        ret = 0;
    } while (0);

    return ret;
}

//TODO : If drv is running , forbidden unload module
int32_t rel_drv_component (void)
{
    uint32_t ret;

    ret = 0;

#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
    payload_cache_release();
#endif

    dms_printk(LOG_LVL_INFO, "DMSC INFO remove all components\n");

#ifdef DISCO_PREALLOC_SUPPORT
    rel_space_manager();
#endif

    set_dmsc_mm_not_working();
    wake_up_mem_resource_wq();

    rel_housekeeping_res();

    dms_printk(LOG_LVL_INFO, "DMSC INFO stop all work q done\n");
    release_sys_procfs();
    rel_conn_manager();
    PLMgr_rel_manager();
    NNClientMgr_rel_manager();
    IOMgr_rel_manager();

    rel_DNSimulator_manager();
    rel_NNOVWSimulator_manager();
    rel_NNSimulator_manager();
    release_vol_manager();

    release_flow_control();
    MDMgr_rel_manager();

    release_discoC_common();

    update_drv_state(&discoC_drv.drv_state, Drv_all_stop);

    ret = 0;
    dms_printk(LOG_LVL_INFO, "DMSC INFO remove all components done\n");

    return ret;
}

static void _init_drv_data (discoC_drv_t *mydrv)
{
    int32_t len;

    memset(mydrv, 0, sizeof(discoC_drv_t));

    len = strlen(ChrDev_Name);
    strncpy(mydrv->chrdev_name, ChrDev_Name, len);
    mydrv->chrdev_name[len] = '\0';
    mydrv->chrdev_major_num = ChrDev_Major;

    len = strlen(BlkDev_Name_Prefix);
    strncpy(mydrv->blkdev_name_prefix, BlkDev_Name_Prefix, len);
    mydrv->blkdev_name_prefix[len] = '\0';

    mydrv->drv_state = Drv_init;
    atomic_set(&mydrv->drv_busy_cnt, 0);

    atomic_set(&discoC_err_cnt, 0);
    mutex_init(&mydrv->drv_lock);
}

int32_t init_drv (int32_t run_mode)
{
    discoC_drv_t *mydrv;
    int32_t ret;
    //allow_signal(SIGINT);
    //allow_signal(SIGTERM);

    ret = 0;

    do {
        mydrv = &discoC_drv;
        _init_drv_data(mydrv);
        if (init_dmsc_config(run_mode)) {
            ret = -ENOMEM;
            printk("DMSC ERROR fail init config when insmod kernel module\n");
            break;
        }

        if (init_chrdev(mydrv)) {
            ret = -ENOMEM;
            release_dmsc_config();
            printk("DMSC ERROR fail create charDev when insmod kernel module\n");
            break;
        }
    } while (0);

    return ret;
}

void release_drv (void)
{
    discoC_drv_t *mydrv;

    mydrv = &discoC_drv;
    rel_blkdev(mydrv);
    rel_chrdev(mydrv);
    release_dmsc_config();
    printk("DMSC INFO Driver module successfully unloaded: Major No. = %d\n",
            mydrv->blkdev_major_num);
}
