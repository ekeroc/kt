/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_blkdev_if.h
 * DATE:        11/25/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces of BLKDEV in the CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_BLKDEV_IF_Init                - Init this module
 *      CCMAKDBG_BLKDEV_IF_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_BLKDEV_IF_H
#define __CCMAKDBG_BLKDEV_IF_H

#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"

typedef enum {
    CCMAKDBG_BLKDEV_IF_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_IF_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_IF_MOD_ID_CODE_END      
} CCMAKDBG_BLKDEV_IF_MOD_ID_CODE_T;

typedef enum {
    CCMAKDBG_BLKDEV_IF_SUB_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_IF_SUB_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_IF_SUB_MOD_ID_CODE_END      
} CCMAKDBG_BLKDEV_IF_SUB_MOD_ID_CODE_T;


typedef enum {
    CCMAKDBG_BLKDEV_IF_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_IF_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_IF_CMD_TYPE_CODE_END      
} CCMAKDBG_BLKDEV_IF_CMD_TYPE_CODE_T;


typedef enum {
    CCMAKDBG_BLKDEV_IF_SUB_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_BLKDEV_IF_SUB_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_BLKDEV_IF_SUB_CMD_TYPE_CODE_END      
} CCMAKDBG_BLKDEV_IF_SUB_CMD_TYPE_CODE_T;

typedef struct {
    CCMAKDBG_BLKDEV_IF_MOD_ID_CODE_T         mod_id;
    CCMAKDBG_BLKDEV_IF_SUB_MOD_ID_CODE_T     sub_mod_id;
    CCMAKDBG_BLKDEV_IF_CMD_TYPE_CODE_T       cmd_type;
    CCMAKDBG_BLKDEV_IF_SUB_CMD_TYPE_CODE_T   sub_cmd_type;
} CCMAKDBG_BLKDEV_IF_IOCTL_CMD;

/**
 * STRUCT NAME:
 *      CCMAKDBG_BLKDEV_IF_EVENT_INFO_T     
 *
 * DESCRIPTION:
 *      Response Information structure
 *
 * MEMBERS:
 */
typedef struct
{
    CCMAKDBG_BLKDEV_IF_IOCTL_CMD      ioctl_cmd;
} CCMAKDBG_BLKDEV_IF_EVENT_INFO_T;

/*
 * Callback function type
 */
typedef void ( *CCMAKDBG_BLKDEV_IF_CB_FUNC )( CCMAKDBG_BLKDEV_IF_EVENT_INFO_T );

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Register_Callback( CCMAKDBG_BLKDEV_IF_CB_FUNC callback );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Attach_Volume( int index );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Deattach_Volume( int index );


#endif
