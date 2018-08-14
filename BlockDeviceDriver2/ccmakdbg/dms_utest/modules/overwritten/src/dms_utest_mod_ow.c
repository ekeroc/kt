/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_utest_mod_ow.c
 *
 * Author: Jared Liang, Vis, Lego Lin
 *
 */

#include <linux/time.h>
#include <linux/types.h>
#include "../../../inc/ccmakdbg_comm.h"
#include "../../../../../overwritten/inc/dms_err.h"
#include "../../../../inc/ccmakdbg_global.h"
#include "../inc/dms_utest_mod_ow.h"
#include "../../../../../overwritten/inc/ow_if.h"
#include "../../../../../overwritten/h/ow_qm.h"
//#include "../../../../../overwritten/h/ow_tbl.h"
//#include "../../../../../overwritten/h/ow_skt.h"

extern CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Show_Table( CCMAKDBG_GLOBAL_MOD_ID_T mod_id );

static struct timeval   _Start_Tick , _End_Tick, _Diff_Tick;

#define  START()       (do_gettimeofday(&_Start_Tick));
#define  END()         (do_gettimeofday(&_End_Tick));
#define  DIFF()        { _Diff_Tick.tv_usec = _End_Tick.tv_usec - _Start_Tick.tv_usec;  \
                         _Diff_Tick.tv_sec  = _End_Tick.tv_sec - _Start_Tick.tv_sec;    \
                       };
                       
static void dms_utest_overwritten_show_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T   ret = DMS_ERR_CODE_SUCCESS;
	
	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN )
	{
		printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
		return;
	}
	
    printk("\n\n" );
    printk("PRE:\n" );
    printk("mod_id  :%d\n", ioctl_data.mod_id  );
    printk("func_id :%d\n", ioctl_data.func_id );

    START();
#if CCMAKDBG_MODULES_OVERWRITTEN
    switch (ioctl_data.func_id)
    {
        case CCMAKDBG_GLOBAL_OW_QM_Show_Table:
            ret = OW_QM_Show_Table();
            break;
        case CCMAKDBG_GLOBAL_OW_QM_Show_Table_REV:
             ret = OW_QM_Show_Table_REV();            
            break;
        case CCMAKDBG_GLOBAL_OW_TBL_Del_All:
             ret = OW_TBL_Del_All();                            
            break;
        case CCMAKDBG_GLOBAL_OW_TBL_Show_Table_Info:
            ret = OW_TBL_Show_Table_Info();
            break;
        case CCMAKDBG_GLOBAL_OW_TBL_Show_Table:
            ret = OW_TBL_Show_Table();
            break;
        case CCMAKDBG_GLOBAL_OW_SKT_Show_State_Info:
            ret = ow_skt_show_state_info();
            break;                
        default:
            printk("DMS_UTEST_MOD_OW: In function %s at line %d failer func_id =%d \n", __FUNCTION__, __LINE__, ioctl_data.func_id );
            return ;
	}
#endif
	END();
	DIFF();
	
    printk("\n\n" );
    printk("POST:\n" );
    printk("mod_id          :%d\n", ioctl_data.mod_id  );
    printk("func_id         :%d\n", ioctl_data.func_id );
    printk("ret             :%d\n", ret );
    printk("Elapsed Time    :%-lu sec %-lu usec\n", _Diff_Tick.tv_sec, _Diff_Tick.tv_usec );
    printk("\n" );
    
    return ;
}

static void dms_utest_ow_if_query_ow_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T          ret = DMS_ERR_CODE_SUCCESS;
    uint64_t      *vol_id;
    
	vol_id  =  (long long*) ioctl_data.payload;

	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN )
	{
		printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
		return;
	}

    printk("\n\n" );
    printk("PRE:\n" );
    printk("mod_id  :%d\n", ioctl_data.mod_id  );
    printk("func_id :%d\n", ioctl_data.func_id );
    printk("vol_id  :%llu\n", *vol_id );
    
    START();
#if CCMAKDBG_MODULES_OVERWRITTEN    
	ret = ow_if_query_overwritten_flag(*vol_id);
#endif	
	END();
	DIFF();
	
    printk("\n\n" );
    printk("POST:\n" );
    printk("mod_id          :%d\n", ioctl_data.mod_id  );
    printk("func_id         :%d\n", ioctl_data.func_id );
    printk("vol_id          :%llu\n", *vol_id );        
    printk("ret             :%d\n", ret );
    printk("Elapsed Time    :%-lu sec %-lu usec\n", _Diff_Tick.tv_sec, _Diff_Tick.tv_usec );
    printk("\n" );
    
    return ;		
}

static void dms_utest_ow_if_set_dbg_flag( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T   ret = DMS_ERR_CODE_SUCCESS;
    unsigned int     *flag;
    
	flag  =  (unsigned int*) ioctl_data.payload;

	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN )
	{
		printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
		return;
	}
	
    printk("\n\n" );
    printk("PRE:\n" );
    printk("mod_id  :%d\n", ioctl_data.mod_id  );
    printk("func_id :%d\n", ioctl_data.func_id );
    printk("flag    :%d\n", *flag );
    
    START();
	END();
	DIFF();
	
    printk("\n\n" );
    printk("POST:\n" );
    printk("mod_id          :%d\n", ioctl_data.mod_id  );
    printk("func_id         :%d\n", ioctl_data.func_id );
    printk("flag            :%d\n", *flag );        
    printk("ret             :%d\n", ret );
    printk("Elapsed Time    :%-lu sec %-lu usec\n", _Diff_Tick.tv_sec, _Diff_Tick.tv_usec );
    printk("\n" );
    
    return ;		
}

CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_OW_INIT(void)
{	
    CCMAKDBG_ERR_CODE_T  ret;

    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);
    
	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                          CCMAKDBG_GLOBAL_OW_IF_Query_OW_Flag,
	                                          dms_utest_ow_if_query_ow_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                          CCMAKDBG_GLOBAL_OW_Set_Dbg_Flag,
	                                          dms_utest_ow_if_set_dbg_flag,
	                                          sizeof(int) );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    
	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                          CCMAKDBG_GLOBAL_OW_TBL_Del_All,
	                                          dms_utest_overwritten_show_cb,
	                                          0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                          CCMAKDBG_GLOBAL_OW_SKT_Show_State_Info,
	                                          dms_utest_overwritten_show_cb,
	                                          0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    
	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                          CCMAKDBG_GLOBAL_OW_TBL_Show_Table_Info,
	                                          dms_utest_overwritten_show_cb,
	                                          0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                      CCMAKDBG_GLOBAL_OW_TBL_Show_Table,
	                                      dms_utest_overwritten_show_cb,
	                                      0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }
    
	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                      CCMAKDBG_GLOBAL_OW_QM_Show_Table_REV,
	                                      dms_utest_overwritten_show_cb,
	                                      0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }



	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN,
	                                      CCMAKDBG_GLOBAL_OW_QM_Show_Table,
	                                      dms_utest_overwritten_show_cb,
	                                      0 );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_OW: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }
		
	CCMAKDBG_CDEV_CBF_TBL_Show_Table(CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN);
    return ret;
}

CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_OW_EXIT(void)
{
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

    return CCMAKDBG_ERR_CODE_SUCCESS;
}
