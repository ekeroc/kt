/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_disk_if.h
 * DATE:        12/1/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_DISK_IF_Init                - Init this module
 *      CCMAKDBG_DISK_IF_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_DISK_IF_IF_H
#define __CCMAKDBG_DISK_IF_IF_H

#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"

typedef enum {
    CCMAKDBG_DISK_IF_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_IF_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_DISK_IF_MOD_ID_CODE_END      
} CCMAKDBG_DISK_IF_MOD_ID_CODE_T;

typedef enum {
    CCMAKDBG_DISK_IF_SUB_MOD_ID_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_IF_SUB_MOD_ID_CODE_FAIL        = -1,
    CCMAKDBG_DISK_IF_SUB_MOD_ID_CODE_END      
} CCMAKDBG_DISK_IF_SUB_MOD_ID_CODE_T;


typedef enum {
    CCMAKDBG_DISK_IF_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_IF_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_DISK_IF_CMD_TYPE_CODE_END      
} CCMAKDBG_DISK_IF_CMD_TYPE_CODE_T;


typedef enum {
    CCMAKDBG_DISK_IF_SUB_CMD_TYPE_CODE_SUCCESS     = 0,
    CCMAKDBG_DISK_IF_SUB_CMD_TYPE_CODE_FAIL        = -1,
    CCMAKDBG_DISK_IF_SUB_CMD_TYPE_CODE_END      
} CCMAKDBG_DISK_IF_SUB_CMD_TYPE_CODE_T;

typedef struct {
    CCMAKDBG_DISK_IF_MOD_ID_CODE_T         mod_id;
    CCMAKDBG_DISK_IF_SUB_MOD_ID_CODE_T     sub_mod_id;
    CCMAKDBG_DISK_IF_CMD_TYPE_CODE_T       cmd_type;
    CCMAKDBG_DISK_IF_SUB_CMD_TYPE_CODE_T   sub_cmd_type;
} CCMAKDBG_DISK_IF_IOCTL_CMD;

/**
 * STRUCT NAME:
 *      CCMAKDBG_DISK_IF_EVENT_INFO_T     
 *
 * DESCRIPTION:
 *      Response Information structure
 *
 * MEMBERS:
 */
typedef struct
{
    CCMAKDBG_DISK_IF_IOCTL_CMD      ioctl_cmd;
} CCMAKDBG_DISK_IF_EVENT_INFO_T;

/*
 * Callback function type
 */
typedef void ( *CCMAKDBG_DISK_IF_CB_FUNC )( CCMAKDBG_DISK_IF_EVENT_INFO_T );

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_IF_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_IF_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_IF_Register_Callback( CCMAKDBG_DISK_IF_CB_FUNC callback );
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_IF_Attach_Volume( int index );
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_IF_Deattach_Volume( int index );


#endif
