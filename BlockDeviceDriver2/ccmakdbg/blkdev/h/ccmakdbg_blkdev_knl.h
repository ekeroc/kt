/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_blkdev_knl.h
 * DATE:        11/25/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of BLKDEV in the CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_BLKDEV_KNL_Init                - Init this module
 *      CCMAKDBG_BLKDEV_KNL_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_BLKDEV_KNL_H
#define __CCMAKDBG_BLKDEV_KNL_H

#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"

#define CCMAKDBG_BLKDEV_REG_CLASS 1

#define CCMAKDBG_BLKDEV_KNL_BLKDEV_NUM 	32

// BLKDEV_MINORS defined how many followed number from first_minor this disk can use for partition.
#define CCMAKDBG_BLKDEV_KNL_MINORS	16

/* CCMA_MINOR separates every 16 numbers for partition of the disk drive use. */
#define CCMAKDBG_BLKDEV_KNL_GET_MINOR(m)									(m * CCMAKDBG_BLKDEV_KNL_MINORS)
#define CCMAKDBG_BLKDEV_KNL_GET_INDEX_BY_MINOR(m)			        		(m / CCMAKDBG_BLKDEV_KNL_MINORS)
#define CCMAKDBG_BLKDEV_KNL_DEVICE_NAME_PREFIX                            "ccmakdbg_disk"
#define CCMAKDBG_BLKDEV_KNL_SECTORS_PER_PAGE_ORDER                        (9)
#define CCMAKDBG_BLKDEV_KNL_MAX_SECTORS                                   (128)





typedef enum {
    CCMAKDBG_BLKDEV_KNL_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_KNL_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_KNL_MOD_ID_CODE_END      
} CCMAKDBG_BLKDEV_KNL_MOD_ID_CODE_T;

typedef enum {
    CCMAKDBG_BLKDEV_KNL_SUB_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_KNL_SUB_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_KNL_SUB_MOD_ID_CODE_END      
} CCMAKDBG_BLKDEV_KNL_SUB_MOD_ID_CODE_T;


typedef enum {
    CCMAKDBG_BLKDEV_KNL_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_KNL_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_KNL_CMD_TYPE_CODE_END      
} CCMAKDBG_BLKDEV_KNL_CMD_TYPE_CODE_T;


typedef enum {
    CCMAKDBG_BLKDEV_KNL_SUB_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_KNL_SUB_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_KNL_SUB_CMD_TYPE_CODE_END      
} CCMAKDBG_BLKDEV_KNL_SUB_CMD_TYPE_CODE_T;

typedef struct {
    CCMAKDBG_BLKDEV_KNL_MOD_ID_CODE_T         mod_id;
    CCMAKDBG_BLKDEV_KNL_SUB_MOD_ID_CODE_T     sub_mod_id;
    CCMAKDBG_BLKDEV_KNL_CMD_TYPE_CODE_T       cmd_type;
    CCMAKDBG_BLKDEV_KNL_SUB_CMD_TYPE_CODE_T   sub_cmd_type;
} CCMAKDBG_BLKDEV_KNL_IOCTL_CMD;

/**
 * STRUCT NAME:
 *      CCMAKDBG_BLKDEV_KNL_EVENT_INFO_T     
 *
 * DESCRIPTION:
 *      Response Information structure
 *
 * MEMBERS:
 */
typedef struct
{
    CCMAKDBG_BLKDEV_KNL_IOCTL_CMD      ioctl_cmd;
} CCMAKDBG_BLKDEV_KNL_EVENT_INFO_T;

/*
 * Callback function type
 */
typedef void ( *CCMAKDBG_BLKDEV_KNL_CB_FUNC )( CCMAKDBG_BLKDEV_KNL_EVENT_INFO_T );

#define CCMAKDBG_BLKDEV_KNL_DRIVER_NAME 	    	  "ccmakdbg_disk"
#define CCMAKDBG_BLKDEV_KNL_REG_CLASS     1
#define CCMAKDBG_BLKDEV_KNL_MAGIC 'd'
#define CCMAKDBG_BLKDEV_KNL_CCMAKDBG_CDEV_IF_IOCTL_VALSET _IOW(CCMAKDBG_BLKDEV_KNL_MAGIC, 1, CCMAKDBG_BLKDEV_KNL_IOCTL_CMD)
#define CCMAKDBG_BLKDEV_KNL_CCMAKDBG_CDEV_IF_IOCTL_VALGET _IOR(CCMAKDBG_BLKDEV_KNL_MAGIC, 2, CCMAKDBG_BLKDEV_KNL_IOCTL_CMD)

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Create_Disk( int index );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Destory_Disk( int index );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Submit_Bio( int index );

#endif
