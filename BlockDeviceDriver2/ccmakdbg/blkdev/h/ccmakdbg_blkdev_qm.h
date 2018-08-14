/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_blkdev_qm.h
 * DATE:        11/25/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces only for sub-modules of BLKDEV in the CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_BLKDEV_QM_Init                - Init this module
 *      CCMAKDBG_BLKDEV_QM_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_BLKDEV_QM_H
#define __CCMAKDBG_BLKDEV_QM_H

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../../inc/ccmakdbg_err.h"

typedef enum {
    CCMAKDBG_BLKDEV_REQUEST_TYPE_OVERWRITTEN = 0,
    CCMAKDBG_BLKDEV_REQUEST_TYPE_END      
} CCMAKDBG_BLKDEV_REQUEST_TYPE_T;

typedef struct
{
    unsigned long long             Vol_id;
    CCMAKDBG_BLKDEV_REQUEST_TYPE_T     req_type;
} CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T;

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Exit(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Enqueue_Req_Msg( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T ReqMsg );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Dequeue_Req_Msg( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T *pReqMsg );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Queue_Is_Empty( void );
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Del_All(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_ShCCMAKDBG_BLKDEV_Table(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_ShCCMAKDBG_BLKDEV_Table_REV(void);

/* 
 * Don't use lock & unlock.
 * Had implemented in .c file already.
 */
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_NN_Req_Lock(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_NN_Req_Unlock(void);
#endif

