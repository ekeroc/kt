#include "../inc/ccmakdbg_comm.h"
#include "../inc/dms_utest_main.h"
#include "../modules/inc/dms_utest_mod_main.h"

CCMAKDBG_ERR_CODE_T DMS_UTEST_MAIN_INIT(void)
{	
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

	return DMS_UTEST_MOD_MAIN_INIT();
}

CCMAKDBG_ERR_CODE_T DMS_UTEST_MAIN_EXIT(void)
{
    printk(" In function %s at line %d\n", __FUNCTION__, __LINE__);    

	return DMS_UTEST_MOD_MAIN_EXIT();	
}
