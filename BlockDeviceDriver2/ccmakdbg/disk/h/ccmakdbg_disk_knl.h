/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_if.h
 * DATE:        12/1/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of the disk in the CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_DISK_KNL_Init                - Init this module
 *      CCMAKDBG_DISK_KNL_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_DISK_KNL_H
#define __CCMAKDBG_DISK_KNL_H

#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"

#define CCMAKDBG_DISK_REG_CLASS 1

#define CCMAKDBG_DISK_KNL_DISK_NUM 	32

// DISK_MINORS defined how many followed number from first_minor this disk can use for partition.
#define CCMAKDBG_DISK_KNL_MINORS	16

/* CCMA_MINOR separates every 16 numbers for partition of the disk drive use. */
#define CCMAKDBG_DISK_KNL_GET_MINOR(m)									(m * CCMAKDBG_DISK_KNL_MINORS)
#define CCMAKDBG_DISK_KNL_GET_INDEX_BY_MINOR(m)			        		(m / CCMAKDBG_DISK_KNL_MINORS)
#define CCMAKDBG_DISK_KNL_DEVICE_NAME_PREFIX                            "ccmakdbg_disk"
#define CCMAKDBG_DISK_KNL_SECTORS_PER_PAGE_ORDER                        (9)
#define CCMAKDBG_DISK_KNL_MAX_SECTORS                                   (128)





typedef enum {
    CCMAKDBG_DISK_KNL_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_KNL_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_DISK_KNL_MOD_ID_CODE_END      
} CCMAKDBG_DISK_KNL_MOD_ID_CODE_T;

typedef enum {
    CCMAKDBG_DISK_KNL_SUB_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_KNL_SUB_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_DISK_KNL_SUB_MOD_ID_CODE_END      
} CCMAKDBG_DISK_KNL_SUB_MOD_ID_CODE_T;


typedef enum {
    CCMAKDBG_DISK_KNL_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_KNL_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_DISK_KNL_CMD_TYPE_CODE_END      
} CCMAKDBG_DISK_KNL_CMD_TYPE_CODE_T;


typedef enum {
    CCMAKDBG_DISK_KNL_SUB_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_KNL_SUB_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_DISK_KNL_SUB_CMD_TYPE_CODE_END      
} CCMAKDBG_DISK_KNL_SUB_CMD_TYPE_CODE_T;

typedef struct {
    CCMAKDBG_DISK_KNL_MOD_ID_CODE_T         mod_id;
    CCMAKDBG_DISK_KNL_SUB_MOD_ID_CODE_T     sub_mod_id;
    CCMAKDBG_DISK_KNL_CMD_TYPE_CODE_T       cmd_type;
    CCMAKDBG_DISK_KNL_SUB_CMD_TYPE_CODE_T   sub_cmd_type;
} CCMAKDBG_DISK_KNL_IOCTL_CMD;

/**
 * STRUCT NAME:
 *      CCMAKDBG_DISK_KNL_EVENT_INFO_T     
 *
 * DESCRIPTION:
 *      Response Information structure
 *
 * MEMBERS:
 */
typedef struct
{
    CCMAKDBG_DISK_KNL_IOCTL_CMD      ioctl_cmd;
} CCMAKDBG_DISK_KNL_EVENT_INFO_T;

/*
 * Callback function type
 */
typedef void ( *CCMAKDBG_DISK_KNL_CB_FUNC )( CCMAKDBG_DISK_KNL_EVENT_INFO_T );

#define CCMAKDBG_DISK_KNL_DRIVER_NAME 	    	  "ccmakdbg_disk"
#define CCMAKDBG_DISK_KNL_REG_CLASS     1
#define CCMAKDBG_DISK_KNL_MAGIC 'd'
#define CCMAKDBG_DISK_KNL_CCMAKDBG_CDEV_IF_IOCTL_VALSET _IOW(CCMAKDBG_DISK_KNL_MAGIC, 1, CCMAKDBG_DISK_KNL_IOCTL_CMD)
#define CCMAKDBG_DISK_KNL_CCMAKDBG_CDEV_IF_IOCTL_VALGET _IOR(CCMAKDBG_DISK_KNL_MAGIC, 2, CCMAKDBG_DISK_KNL_IOCTL_CMD)

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Create_Disk( int index );
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Destory_Disk( int index );

#endif
