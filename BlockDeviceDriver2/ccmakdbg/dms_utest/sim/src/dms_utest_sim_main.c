#include "../../inc/ccmakdbg_comm.h"
#include "../inc/dms_utest_sim_main.h"
#include "../overwritten/inc/dms_utest_sim_ow.h"
void DMS_UTEST_SIM_MAIN_INIT(void)
{	
	( void ) DMS_UTEST_SIM_OW_INIT();
    return ;
}

void DMS_UTEST_SIM_MAIN_EXIT(void)
{
	( void ) DMS_UTEST_SIM_OW_EXIT();
    return;
}
