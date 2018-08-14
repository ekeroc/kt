#include "../../../inc/ccmakdbg_comm.h"
#include "../inc/dms_utest_sim_ow.h"

typedef struct {
    char					        a;
    char					        b;
    char					        c;
    char					        d;
} __attribute__((__packed__)) DMS_UTEST_OW_CMD_INFO;

#if 0
static void dms_utest_overwritten_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{
		DMS_UTEST_OW_CMD_INFO   *cmd_info;
		
		if ( ioctl_data.mod_id != CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN )
		{
			printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
		}
		
		cmd_info  =  (DMS_UTEST_OW_CMD_INFO *) ioctl_data.payload;
		printk("a=%x \n",cmd_info->a );
		printk("b=%x \n",cmd_info->b );
		printk("c=%x \n",cmd_info->c );	
		printk("d=%x \n",cmd_info->d );	
		

}
#endif
void DMS_UTEST_SIM_OW_INIT(void)
{	

	//( void ) CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_OVERWRITTEN, dms_utest_overwritten_cb, sizeof(DMS_UTEST_OW_CMD_INFO) );
		
	//OW_QM_Show_Table();
    return ;
}

void DMS_UTEST_SIM_OW_EXIT(void)
{
    return;
}
