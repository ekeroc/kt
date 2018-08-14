#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include "../../inc/ccmakdbg_err.h"
#include "../inc/ccmakdbg_disk_if.h"
MODULE_LICENSE("Dual BSD/GPL");

#define MODULES_NAME               "CCMAKDBG_DISK_UMAIN:"

static int umain_disk_init(void)
{
    printk("%s In function %s at line %d: Installed Successfully,before.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
 
    CCMAKDBG_DISK_IF_Init();

    CCMAKDBG_DISK_IF_Attach_Volume(1);
    
    printk("%s In function %s at line %d: Installed Successfully, after.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
    
    return 0;
}

static void umain_disk_exit(void)
{
	
    printk("%s In function %s at line %d: Removed Successfully, before.\n",MODULES_NAME, __FUNCTION__, __LINE__);    
    CCMAKDBG_DISK_IF_Deattach_Volume(1);  
    CCMAKDBG_DISK_IF_Exit();
    printk("%s In function %s at line %d: Removed Successfully, after\n",MODULES_NAME, __FUNCTION__, __LINE__);    

    return;
}

module_init(umain_disk_init);
module_exit(umain_disk_exit);

MODULE_DESCRIPTION("CCMA DBG Module");
MODULE_ALIAS("CCMA DBG Module");
