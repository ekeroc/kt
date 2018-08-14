/*=============================================================================
 * MODULE NAME: CCMAKDBG
 * FILE NAME:   ccmakdbg_if.h
 * DATE:        12/1/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide global variable between CLI and Kernel modules in CCMAKDBG modules.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_IF_Init                - Init this module
 *      CCMAKDBG_IF_Exit                - Exit this module
 * 
 *=============================================================================
 */
#ifndef __CCMAKDBG_GLOBAL_LIST_H
#define __CCMAKDBG_GLOBAL_LIST_H

#define  CCMAKDBG_MODULES_OVERWRITTEN        1


#define CCMAKDBG_GLOBAL_DEVFILE "/dev/ccmakdbg_d0"
#define CCMAKDBG_GLOBAL_PASSWORD    "ccma_dms"


/*
 * Modules ID List.
 */
typedef enum {
    CCMAKDBG_GLOBAL_MOD_ID_CCMAKDBG        = 0,
    CCMAKDBG_GLOBAL_MOD_ID_BLKDEV          = 1,
    CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN        ,    
    CCMAKDBG_GLOBAL_MOD_ID_VOLUME_MGM         ,    
    CCMAKDBG_GLOBAL_MOD_ID_CACHE              ,    
    CCMAKDBG_GLOBAL_MOD_ID_DMS                ,
    CCMAKDBG_GLOBAL_MOD_ID_END      
} CCMAKDBG_GLOBAL_MOD_ID_T;

/*****************************************************************************
 *                  The list of function ID per module.                      *
 *****************************************************************************/

/*
 * CCMA KDBG module.
 */
typedef enum {
    CCMAKDBG_GLOBAL_KDBG_Showall_Func            = 1,
    CCMAKDBG_GLOBAL_KDBG_Show_Perf_TBL_Func      = 2,   
    CCMAKDBG_GLOBAL_KDBG_Show_Perf_QM_TBL_Func,
    CCMAKDBG_GLOBAL_KDBG_END                 
    
} CCMAKDBG_GLOBAL_KDBG_KFUNC_ID_T;

/*
 * BLKDEV  module for testing.
 */
typedef enum {
    CCMAKDBG_GLOBAL_BLKDEV_Showall            = 1,
    CCMAKDBG_GLOBAL_BLKDEV_Write_Full_Req     = 2,    
    CCMAKDBG_GLOBAL_BLKDEV_Read_Full_Req         ,
    CCMAKDBG_GLOBAL_BLKDEV_Write_Partial_Req     ,    
    CCMAKDBG_GLOBAL_BLKDEV_Read_Partial_Req      ,
    CCMAKDBG_GLOBAL_BLKDEV_END    
} CCMAKDBG_GLOBAL_BLKDEV_KFUNC_ID_T;
 
/*
 * Overwritten Flag module.
 */
typedef enum {
    CCMAKDBG_GLOBAL_OW_IF_Query_OW_Flag         = 1,
    CCMAKDBG_GLOBAL_OW_QM_Show_Table            = 2,
    CCMAKDBG_GLOBAL_OW_QM_Show_Table_REV           ,    
    CCMAKDBG_GLOBAL_OW_TBL_Del_All                 ,    
    CCMAKDBG_GLOBAL_OW_TBL_Show_Table_Info         ,    
    CCMAKDBG_GLOBAL_OW_TBL_Show_Table              ,
    CCMAKDBG_GLOBAL_OW_SKT_Show_State_Info         ,
    CCMAKDBG_GLOBAL_OW_Set_Dbg_Flag                ,    
    CCMAKDBG_GLOBAL_OW_END      
} CCMAKDBG_GLOBAL_OW_KFUNC_ID_T;


/*
 * Volume Management module.
 */
typedef enum {
    CCMAKDBG_GLOBAL_VOL_MGM_Create_Vol          = 1,
    CCMAKDBG_GLOBAL_VOL_MGM_Atta_Vol            = 2,    
    CCMAKDBG_GLOBAL_VOL_MGM_Deatta_Vol             ,
    CCMAKDBG_GLOBAL_VOL_MGM_Showall                ,    
    CCMAKDBG_GLOBAL_VOL_MGM_Del_Vol                ,    
    CCMAKDBG_GLOBAL_VOL_MGM_END    
} CCMAKDBG_GLOBAL_VOL_MGM_KFUNC_ID_T;

/*
 * Cache module.
 */
typedef enum {
    CCMAKDBG_GLOBAL_CACHE_Clear                 = 1,
    CCMAKDBG_GLOBAL_CACHE_Showall               = 2,    
    CCMAKDBG_GLOBAL_CACHE_END    
} CCMAKDBG_GLOBAL_CACHE_KFUNC_ID_T;

/*
 * DMS module.
 */
typedef enum {
    CCMAKDBG_GLOBAL_DMS_PASSWORD                 = 1,
    CCMAKDBG_GLOBAL_DMS_SHOW_VER                    ,    
    CCMAKDBG_GLOBAL_DMS_END    
} CCMAKDBG_GLOBAL_DMS_KFUNC_ID_T;

#endif
