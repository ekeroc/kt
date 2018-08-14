#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include "../../inc/ccmakdbg_err.h"
#include "../h/ccmakdbg_cdev_cbf_tbl.h"

MODULE_LICENSE("Dual BSD/GPL");


static void umain_cdev_cb( CCMAKDBG_CDEV_IF_IOCTL_CMD  ioctl_data )
{		
	printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);
}


static int umain_cdev_init(void)
{
    int i, j;
    CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T ent;
    
    printk( "umain_cdev_init: Kernel Module Installed Successfully.\n" );
 
    CCMAKDBG_CDEV_CBF_TBL_Init();

    for ( i = 0 ; i < 3 ; i++ )
    {
        for ( j = 0 ; j < 10 ; j++ )
        {    
            ent.mod_id        = i;
            ent.func_id       = j;
            ent.payload_size  = j*10;
            ent.cb_func       = umain_cdev_cb;
            CCMAKDBG_CDEV_CBF_TBL_Add_Entry( i, &ent);
        }
    }

    for ( i = 0 ; i < 3 ; i++ )
    {
        ( void ) CCMAKDBG_CDEV_CBF_TBL_Show_Table_Info( i );
    }
    
    for ( i = 0 ; i < 3 ; i++ )
    {
        for ( j = 0 ; j < 10 ; j++ )
        {    
            CCMAKDBG_CDEV_CBF_TBL_Get_Entry( i, j, &ent);
            printk("%-6d  %-7d  %-7d  %-8p  \n", ent.mod_id, ent.func_id, ent.payload_size, ent.cb_func );
            
        }
    }

    
    return 0;
}

static void umain_cdev_exit(void)
{
	
    CCMAKDBG_CDEV_CBF_TBL_Exit();
	printk( "umain_cdev_exit: Kernel Module removed Successfully.\n");

    return;
}

module_init(umain_cdev_init);
module_exit(umain_cdev_exit);

MODULE_DESCRIPTION("CCMA DBG Module");
MODULE_ALIAS("CCMA DBG Module");
