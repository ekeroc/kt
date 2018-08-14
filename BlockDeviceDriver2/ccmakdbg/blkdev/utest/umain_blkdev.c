#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include "../../inc/ccmakdbg_err.h"
#include "../inc/ccmakdbg_blkdev_if.h"
#include "../h/ccmakdbg_blkdev_knl.h"

MODULE_LICENSE("Dual BSD/GPL");

#define MODULES_NAME               "CCMAKDBG_BLKDEV_UMAIN:"

static int umain_blkdev_init(void)
{
    printk("%s In function %s at line %d: Installed Successfully,before.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
 
    CCMAKDBG_BLKDEV_IF_Init();

    CCMAKDBG_BLKDEV_IF_Attach_Volume(1);

    CCMAKDBG_BLKDEV_KNL_Submit_Bio(1);
    
    printk("%s In function %s at line %d: Installed Successfully, after.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
    
    return 0;
}

static void umain_blkdev_exit(void)
{
	
    printk("%s In function %s at line %d: Removed Successfully, before.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
    CCMAKDBG_BLKDEV_IF_Deattach_Volume(1);  
    CCMAKDBG_BLKDEV_IF_Exit();
    printk("%s In function %s at line %d: Removed Successfully, after\n",MODULES_NAME, __FUNCTION__, __LINE__);    

    return;
}

module_init(umain_blkdev_init);
module_exit(umain_blkdev_exit);

MODULE_DESCRIPTION("CCMA DBG Module");
MODULE_ALIAS("CCMA DBG Module");
