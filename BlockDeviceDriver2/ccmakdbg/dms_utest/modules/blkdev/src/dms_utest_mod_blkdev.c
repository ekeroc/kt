#include <linux/time.h>
#include "../../../inc/ccmakdbg_comm.h"
#include "../inc/dms_utest_mod_blkdev.h"
#include "../../../../../overwritten/inc/dms_err.h"

extern CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Show_Table( CCMAKDBG_GLOBAL_MOD_ID_T mod_id );

static struct timeval   _Start_Tick , _End_Tick, _Diff_Tick;

#define  START()       (do_gettimeofday(&_Start_Tick));
#define  END()         (do_gettimeofday(&_End_Tick));
#define  DIFF()        { _Diff_Tick.tv_usec = _End_Tick.tv_usec - _Start_Tick.tv_usec;  \
                         _Diff_Tick.tv_sec  = _End_Tick.tv_sec - _Start_Tick.tv_sec;    \
                       };
                       
static void dms_utest_blkdev_show_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T   ret = DMS_ERR_CODE_SUCCESS;
	
	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_BLKDEV )
	{
		printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
		return;
	}
	
    printk("\n\n" );
    printk("PRE:\n" );
    printk("mod_id  :%d\n", ioctl_data.mod_id  );
    printk("func_id :%d\n", ioctl_data.func_id );

    START();
    /*
     * Call Kernel function.
     */
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
#if 0
static void DMS_UTEST_MOD_BLKDEV_if_query_ow_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T   ret = DMS_ERR_CODE_SUCCESS;
    unsigned long long      *vol_id;
    
	vol_id  =  (long long*) ioctl_data.payload;

	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_BLKDEV )
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
    /*
     * Call Kernel function.
     */
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

static void dms_utest_blkdev_if_set_dbg_flag( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
    DMS_ERR_CODE_T   ret = DMS_ERR_CODE_SUCCESS;
    unsigned int     *flag;
    
	flag  =  (unsigned int*) ioctl_data.payload;

	if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_BLKDEV )
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
    /*
     * Call Kernel function.
     */
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
#endif
CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_BLKDEV_INIT(void)
{	
    CCMAKDBG_ERR_CODE_T  ret;

    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);
    
	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_BLKDEV,
	                                          CCMAKDBG_GLOBAL_BLKDEV_Showall,
	                                          dms_utest_blkdev_show_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_BLKDEV: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_BLKDEV,
	                                          CCMAKDBG_GLOBAL_BLKDEV_Write_Full_Req,
	                                          dms_utest_blkdev_show_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_BLKDEV: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_BLKDEV,
	                                          CCMAKDBG_GLOBAL_BLKDEV_Read_Full_Req,
	                                          dms_utest_blkdev_show_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_BLKDEV: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_BLKDEV,
	                                          CCMAKDBG_GLOBAL_BLKDEV_Write_Partial_Req,
	                                          dms_utest_blkdev_show_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_BLKDEV: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	ret = CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_BLKDEV,
	                                          CCMAKDBG_GLOBAL_BLKDEV_Read_Partial_Req,
	                                          dms_utest_blkdev_show_cb,
	                                          sizeof(long long));
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD_BLKDEV: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	CCMAKDBG_CDEV_CBF_TBL_Show_Table(CCMAKDBG_GLOBAL_MOD_ID_BLKDEV);
    return ret;
}

CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_BLKDEV_EXIT(void)
{
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

    return CCMAKDBG_ERR_CODE_SUCCESS;
}
