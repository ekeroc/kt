/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_cdev_cbf_tbl.h
 * DATE:        11/25/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of CDEV.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_CDEV_CBF_TBL_Init                - Init this module
 *      CCMAKDBG_CDEV_CBF_TBL_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef _DMS_CCMAKDBG_CDEV_CBF_TBL_H
#define _DMS_CCMAKDBG_CDEV_CBF_TBL_H
#include <linux/ioctl.h>
#include <asm/byteorder.h>
#include "../../inc/ccmakdbg_err.h"
#include "../inc/ccmakdbg_cdev_if.h"

#define CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD          CCMAKDBG_GLOBAL_MOD_ID_END

typedef struct
{
    CCMAKDBG_GLOBAL_MOD_ID_T     mod_id;
    int                          func_id;    
    CCMAKDBG_CDEV_IF_CB_FUNC     cb_func;
    int                          payload_size;
} __attribute__((__packed__)) CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T;

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Init( void );
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Exit( void );
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Add_Entry( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T *ent );
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Get_Entry( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, int func_id, CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T *ent );
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Del_All(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Show_Table(CCMAKDBG_GLOBAL_MOD_ID_T mod_id);

#endif  /* _DMS_CCMAKDBG_CDEV_CBF_TBL_H */
