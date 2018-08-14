/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_err.h
 * DATE:        12/1/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide defined error codes.
 *
 *
 * FUNCTIONS    
 *      None.
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_ERR_H
#define __CCMAKDBG_ERR_H
#include "ccmakdbg_global.h"

#define CCMAKDBG_BOOL_TRUE         1
#define CCMAKDBG_BOOL_FALSE        0 
typedef enum {
    CCMAKDBG_ERR_CODE_SUCCESS = 0,
    CCMAKDBG_ERR_CODE_FAIL    = 1,
    CCMAKDBG_ERR_CODE_PARAM_INVALID,
    CCMAKDBG_ERR_CODE_NOT_EXIST,    
    CCMAKDBG_ERR_CODE_NO_MEMORY,
    CCMAKDBG_ERR_CODE_TBL_FULL,
    CCMAKDBG_ERR_CODE_QUEUE_EMPTY,
    CCMAKDBG_ERR_CODE_TBL_OVERFLOW,
    CCMAKDBG_ERR_CODE_NULLPOINT,    
    CCMAKDBG_ERR_CODE_END      
} CCMAKDBG_ERR_CODE_T;

#endif
