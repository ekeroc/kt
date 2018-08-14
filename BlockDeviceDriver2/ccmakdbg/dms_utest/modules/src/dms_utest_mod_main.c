#include "../../inc/ccmakdbg_comm.h"
#include "../inc/dms_utest_mod_main.h"
#include "../ccmakdbg/inc/dms_utest_mod_ccmakdbg.h"
#include "../overwritten/inc/dms_utest_mod_ow.h"
#include "../vol_mgm/inc/dms_utest_mod_vol_mgm.h"
#include "../blkdev/inc/dms_utest_mod_blkdev.h"
#include "../cache/inc/dms_utest_mod_cache.h"
#include "../dms/inc/dms_utest_mod_dms.h"

CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_MAIN_INIT(void)
{	
    CCMAKDBG_ERR_CODE_T ret;
    
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

    ret = DMS_UTEST_MOD_CCMAKDBG_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_OW_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_VOL_MGM_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_BLKDEV_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_CACHE_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_DMS_INIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	return ret;
}

CCMAKDBG_ERR_CODE_T DMS_UTEST_MOD_MAIN_EXIT(void)
{
    CCMAKDBG_ERR_CODE_T ret;
    
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

    ret = DMS_UTEST_MOD_CCMAKDBG_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_OW_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_VOL_MGM_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_BLKDEV_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_CACHE_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

    ret = DMS_UTEST_MOD_DMS_EXIT();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS_UTEST_MOD: In function %s at line %d failer ret=%d \n", __FUNCTION__, __LINE__, ret );
        return ret;
    }

	return ret;
}
