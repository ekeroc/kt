/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * sys_mem_monitor.h
 *
 * monitor whether current available memory in OS is low
 *
 */
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include "../common/common_util.h"
#include "../config/dmsc_config.h"

/*
 * is_sys_free_mem_low: check system memory is lower than setting
 *
 * Return: true: available system memory < congestion setting
 *         false: has enough memory
 */
bool is_sys_free_mem_low (void)
{
    struct sysinfo i;
    uint64_t free_mem;

    si_meminfo(&i);

    free_mem = KBytes(i.freeram);

    if (free_mem >= dms_client_config->freeMem_cong) {
        return false;
    }

    return true;
}



