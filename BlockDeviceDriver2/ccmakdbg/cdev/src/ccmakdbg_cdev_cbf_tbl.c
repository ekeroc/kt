#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "../h/ccmakdbg_cdev_cbf_tbl.h"

typedef struct
{
    struct list_head  list;
    struct mutex      queue_lock;
} CCMAKDBG_CDEV_CBF_TBL_TBL_T;

typedef struct{
    struct list_head                      list;
    CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T    entry;
} __attribute__((__packed__)) CCMAKDBG_CDEV_CBF_TBL_ITEM_T;

static CCMAKDBG_CDEV_CBF_TBL_TBL_T _ccmakdbg_cdev_cbf_tbl_func_tbl[CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD];

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Init( void )
{
    int i;

    for ( i = 0 ; i < CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD ; i++ )
    {
        INIT_LIST_HEAD(&_ccmakdbg_cdev_cbf_tbl_func_tbl[i].list);
    }        
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}


CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Exit( void )
{
    CCMAKDBG_CDEV_CBF_TBL_ITEM_T      *item, *next;
    int i;

    for ( i = 0 ; i < CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD ; i++ )
    {
        
        if ( list_empty(&_ccmakdbg_cdev_cbf_tbl_func_tbl[i].list))
        {
            continue;
        }
    
        list_for_each_entry_safe(item, next, &_ccmakdbg_cdev_cbf_tbl_func_tbl[i].list, list)
        {        
            list_del(&item->list);
            kfree(item);
        }
    }        
    
    return CCMAKDBG_ERR_CODE_SUCCESS;    
}


CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Add_Entry( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T *ent )
{
    CCMAKDBG_CDEV_CBF_TBL_ITEM_T      *new_item;

    if ( mod_id >= CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD )
    {
       return CCMAKDBG_ERR_CODE_PARAM_INVALID;
    }     
    
    new_item = kmalloc( sizeof(CCMAKDBG_CDEV_CBF_TBL_ITEM_T) , GFP_ATOMIC);  
    
    if ( new_item == NULL )
    {
        printk("In function %s at line %d\n", __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_TBL_FULL;
    }
    
    memcpy( &new_item->entry, ent, sizeof( CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T ) );    
    list_add_tail(&new_item->list, &_ccmakdbg_cdev_cbf_tbl_func_tbl[mod_id].list);
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Get_Entry( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, int func_id, CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T *ent )
{
    CCMAKDBG_CDEV_CBF_TBL_ITEM_T      *item, *next;

    if ( mod_id >= CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD )
    {
       return CCMAKDBG_ERR_CODE_PARAM_INVALID;
    }     

    if ( ent == NULL )
    {
        return CCMAKDBG_ERR_CODE_NULLPOINT;
    }
    
    if ( list_empty(&_ccmakdbg_cdev_cbf_tbl_func_tbl[mod_id].list))
    {
        return CCMAKDBG_ERR_CODE_NOT_EXIST;
    }    

    list_for_each_entry_safe(item, next, &_ccmakdbg_cdev_cbf_tbl_func_tbl[mod_id].list, list)
    {        
        if ( item->entry.func_id == func_id )
        {
            memcpy( ent, &item->entry, sizeof( CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T ) );   
            return CCMAKDBG_ERR_CODE_SUCCESS;            
        }
    }
        
    return CCMAKDBG_ERR_CODE_NOT_EXIST;            
}



CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Del_All( void )
{
    CCMAKDBG_CDEV_CBF_TBL_ITEM_T      *item, *next;


    int i;

    for ( i = 0 ; i < CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD ; i++ )
    {
    
        if ( list_empty(&_ccmakdbg_cdev_cbf_tbl_func_tbl[i].list))
        {
            continue;
        }    
        
        list_for_each_entry_safe(item, next, &_ccmakdbg_cdev_cbf_tbl_func_tbl[i].list, list)
        {        
            list_del(&item->list);
            kfree(item);
        }
    }
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_CBF_TBL_Show_Table( CCMAKDBG_GLOBAL_MOD_ID_T mod_id )
{
    CCMAKDBG_CDEV_CBF_TBL_ITEM_T      *item, *next;

    if ( mod_id >= CCMAKDBG_CDEV_CBF_TBL_MAX_NUM_OF_MOD )
    {
       return CCMAKDBG_ERR_CODE_PARAM_INVALID;
    }     

    if ( list_empty(&_ccmakdbg_cdev_cbf_tbl_func_tbl[mod_id].list))
    {
        return CCMAKDBG_ERR_CODE_SUCCESS;
    }    

    printk("Mod_id  func_id  py_size  cb_func \n");
    printk("------  -------  -------  --------\n");

    list_for_each_entry_safe(item, next, &_ccmakdbg_cdev_cbf_tbl_func_tbl[mod_id].list, list)
    {        
        printk("%-6d  %-7d  %-7d  %-8p  \n", item->entry.mod_id, item->entry.func_id, item->entry.payload_size, item->entry.cb_func );
    }
        
    return CCMAKDBG_ERR_CODE_SUCCESS;            
}


