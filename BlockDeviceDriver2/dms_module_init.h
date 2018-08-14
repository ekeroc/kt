/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_module_init.h
 *
 */

#ifndef _DMS_MODULE_INIT_H_
#define _DMS_MODULE_INIT_H_

//----CCMA BLOCK DEVICE DRIVER INFO start----------------//
#define VERSION_STR          "1.0.1"
#define COPYRIGHT            "Copyright 2010 CCMA/ITRI"
#define DRIVER_AUTHOR        "CCMA Storage team"
#define DRIVER_DESC          "Block device driver for CCMA Storage System"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#define PARAM_PERMISSION 0644
#else
#define PARAM_PERMISSION ( S_ISUID | S_ISGID | S_IRUSR | S_IRGRP )
#endif

#endif /* _DMS_MODULE_INIT_H_ */
