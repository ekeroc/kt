/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_module_init.c
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "dms_module_init.h"
#include "drv_main_api.h"

int32_t running_mode = 0;

static int
__init init_discoC_module (void)
{
    return init_drv(running_mode);
}

static void
__exit exit_discoC_module (void) {
    release_drv();
}

module_init(init_discoC_module);
module_exit(exit_discoC_module);

//setup module parameters. user can pass it down when insmod
module_param(running_mode, int, PARAM_PERMISSION);
MODULE_PARM_DESC(running_mode, "DMSClient test flag");
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, COPYRIGHT);


