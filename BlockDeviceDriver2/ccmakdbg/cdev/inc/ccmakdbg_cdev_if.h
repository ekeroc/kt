/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_cdev_if.h
 * DATE:        11/25/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_CDEV_IF_Init                - Init this module
 *      CCMAKDBG_CDEV_IF_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_CDEV_IF_H
#define __CCMAKDBG_CDEV_IF_H

#include <linux/version.h>
#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"

#define       CCMAKDBG_CDEV_MAX_PAYLOAD_LEN      256

typedef struct {
    CCMAKDBG_GLOBAL_MOD_ID_T   mod_id;
    int                        func_id;
    char                       payload[CCMAKDBG_CDEV_MAX_PAYLOAD_LEN];
} __attribute__((__packed__)) CCMAKDBG_CDEV_IF_IOCTL_CMD;

typedef struct
{
    CCMAKDBG_CDEV_IF_IOCTL_CMD      ioctl_cmd;
} CCMAKDBG_CDEV_EVENT_INFO_T;

/*
 * Callback function type
 */
typedef void ( *CCMAKDBG_CDEV_IF_CB_FUNC )( CCMAKDBG_CDEV_IF_IOCTL_CMD );

#define CCMAKDBG_DRIVER_NAME 	    	  "CCMACCMAKDBG"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
#define CCMAKDBG_CDEV_REG_CLASS     1
#else
#define CCMAKDBG_CDEV_REG_CLASS     0
#endif
#define CCMAKDBG_CDEV_IF_IOC_MAGIC 'd'
#define CCMAKDBG_CDEV_IF_IOCTL_VALSET _IOW(CCMAKDBG_CDEV_IF_IOC_MAGIC, 1, CCMAKDBG_CDEV_IF_IOCTL_CMD)
#define CCMAKDBG_CDEV_IF_IOCTL_VALGET _IOR(CCMAKDBG_CDEV_IF_IOC_MAGIC, 2, CCMAKDBG_CDEV_IF_IOCTL_CMD)

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, int func_id,  CCMAKDBG_CDEV_IF_CB_FUNC callback, int payload_size );

#endif
