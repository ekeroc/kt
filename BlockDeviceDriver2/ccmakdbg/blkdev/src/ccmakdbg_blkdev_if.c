#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include "../inc/ccmakdbg_blkdev_if.h"
#include "../h/ccmakdbg_blkdev_qm.h"
#include "../h/ccmakdbg_blkdev_knl.h"

#define MODULES_NAME               "CCMAKDBG_BLKDEV_IF:"

MODULE_LICENSE("Dual BSD/GPL");

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Init( void )
{
    CCMAKDBG_ERR_CODE_T ret;

    ret = CCMAKDBG_BLKDEV_QM_Init();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
        return ret;
    }

    ret = CCMAKDBG_BLKDEV_KNL_Init();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
        return ret;
    }

	return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Exit( void )
{
    CCMAKDBG_ERR_CODE_T ret;

    ret = CCMAKDBG_BLKDEV_QM_Exit();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
    }
    
    ret = CCMAKDBG_BLKDEV_KNL_Exit();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
    }


    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Attach_Volume( int index )
{
    CCMAKDBG_ERR_CODE_T ret;

    ret = CCMAKDBG_BLKDEV_KNL_Create_Disk( index );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
    }

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Deattach_Volume( int index )
{
    CCMAKDBG_ERR_CODE_T ret;

    ret = CCMAKDBG_BLKDEV_KNL_Destory_Disk( index );
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
    }

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Write_Full_Bio( int index )
{


    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_IF_Write_One_Bio_Mpl_Pages( int index )
{


    return CCMAKDBG_ERR_CODE_SUCCESS;
}

