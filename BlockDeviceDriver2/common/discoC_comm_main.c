/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_comm_main.c
 *
 * initialize all common components
 * release all common components
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "discoC_comm_main_private.h"
#include "discoC_comm_main.h"
#include "dms_client_mm.h"
#include "thread_manager.h"
#include "discoC_mem_manager.h"

/*
 * set_common_loglvl: set whole system log level
 * @mylevel: log level to set
 */
void set_common_loglvl (log_lvl_t mylevel)
{
    discoC_log_lvl = mylevel;
}

/*
 * init_discoC_common: An API for system initialization. Initialize memory manager,
 *                     cache system, thread manager
 * @num_of_ioreq: number of pre-alloc io requests
 *
 * Return: initialization result
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t init_discoC_common (int32_t num_of_ioreq)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO init discoC common module\n");

    do {
        if ((ret = client_mm_init(num_of_ioreq))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail to init mem buffer\n");
            break;
        }

        if ((ret = init_mempool_manager())) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail to init mem pool manager\n");
            break;
        }

        init_thread_manager();
    } while (0);

    dms_printk(LOG_LVL_INFO, "DMSC INFO init discoC common module DONE ret %d\n",
            ret);

    return ret;
}

/*
 * release_discoC_common: An API for system release. Release memory manager,
 *                        cache system, thread manager
 *
 */
void release_discoC_common (void)
{
    dms_printk(LOG_LVL_INFO, "DMSC INFO release discoC common module\n");

    release_thread_manager();
    client_mm_release();
    rel_mempool_manager();

    dms_printk(LOG_LVL_INFO, "DMSC INFO release discoC common module DONE\n");
}

