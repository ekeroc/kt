#include <linux/init.h>
#include <linux/module.h>
#include "../inc/ccmakdbg_if.h"
#include "../cdev/inc/ccmakdbg_cdev_if.h"
#include "../../overwritten/inc/ow_if.h"
#include "../dms_utest/inc/dms_utest_main.h"

CCMAKDBG_ERR_CODE_T CCMAKDBG_IF_Init( void )
{      
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

    if ( CCMAKDBG_CDEV_IF_Init() != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d\n, failer", __FUNCTION__, __LINE__);
        return CCMAKDBG_ERR_CODE_FAIL;
    }
       
    if ( DMS_UTEST_MAIN_INIT() != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d\n, failer",__FUNCTION__, __LINE__);
        return CCMAKDBG_ERR_CODE_FAIL;        
    }
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_IF_Exit( void )
{    
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    
    
    if ( CCMAKDBG_CDEV_IF_Exit() != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d\n, failer", __FUNCTION__, __LINE__);
        return CCMAKDBG_ERR_CODE_FAIL;        
    }
    if ( DMS_UTEST_MAIN_EXIT() != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d\n, failer", __FUNCTION__, __LINE__);
        return CCMAKDBG_ERR_CODE_FAIL;       
    }
   
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

