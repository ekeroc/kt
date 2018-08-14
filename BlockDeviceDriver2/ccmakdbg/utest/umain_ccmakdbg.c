#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include "../inc/ccmakdbg_if.h"

MODULE_LICENSE("Dual BSD/GPL");
static int umain_ccmakdbg_init(void)
{
    printk( "umain_ktest_init: Kernel Module Installed Successfully.\n" );

    CCMAKDBG_IF_Init();

    
	//( void ) OW_IF_Init();    
    return 0;
}

static void umain_ccmakdbg_exit(void)
{
	
    CCMAKDBG_IF_Exit();
//	( void ) OW_IF_Exit();
	printk( "umain_ktest_exit: Kernel Module removed Successfully.\n");

    return;
}

module_init(umain_ccmakdbg_init);
module_exit(umain_ccmakdbg_exit);

MODULE_DESCRIPTION("CCMA DBG Module");
MODULE_ALIAS("CCMA DBG Module");
