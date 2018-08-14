#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../h/ccmakdbg_disk_qm.h"
#include "../../overwritten/h/ow_porting.h"

#define MODULES_NAME               "CCMAKDBG_DISK_QM:"

typedef struct
{
    struct list_head  list;
    struct mutex      queue_lock;
} CCMAKDBG_DISK_QM_NN_REQ_QUEUE_T;

typedef struct
{
    struct list_head     list;
    CCMAKDBG_DISK_QM_NN_REQ_ENTRY_T req_info;
} CCMAKDBG_DISK_QM_NN_REQ_ITEM_T;

static CCMAKDBG_DISK_QM_NN_REQ_QUEUE_T  _ccmakdbg_disk_qm_req_queue;

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Init(void)
{   
    mutex_init(&_ccmakdbg_disk_qm_req_queue.queue_lock);
    INIT_LIST_HEAD(&_ccmakdbg_disk_qm_req_queue.list); 
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Exit(void)
{
    if ( !list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
      ( void ) CCMAKDBG_DISK_QM_Del_All();
    }
    
    mutex_destroy(&_ccmakdbg_disk_qm_req_queue.queue_lock);
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_NN_Req_Lock(  )
{
    mutex_lock(&_ccmakdbg_disk_qm_req_queue.queue_lock);
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_NN_Req_Unlock(  )
{
    mutex_unlock(&_ccmakdbg_disk_qm_req_queue.queue_lock);
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Enqueue_Req_Msg( CCMAKDBG_DISK_QM_NN_REQ_ENTRY_T ReqMsg )
{
    CCMAKDBG_DISK_QM_NN_REQ_ITEM_T      *new_ent;

    new_ent = kmalloc( sizeof(CCMAKDBG_DISK_QM_NN_REQ_ITEM_T) , GFP_ATOMIC);  

    if ( new_ent == NULL )
    {
        printk("%s In function %s at line %d\n", MODULES_NAME, __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_NO_MEMORY;    
    }
    
    memcpy( &new_ent->req_info, &ReqMsg, sizeof( CCMAKDBG_DISK_QM_NN_REQ_ENTRY_T ) );    
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Lock();
    list_add_tail(&new_ent->list, &_ccmakdbg_disk_qm_req_queue.list);
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Unlock();

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Queue_Is_Empty( void )
{
    if ( list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}


CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Dequeue_Req_Msg( CCMAKDBG_DISK_QM_NN_REQ_ENTRY_T *pReqMsg )
{
    CCMAKDBG_DISK_QM_NN_REQ_ITEM_T    *item, *next;
    

    if ( pReqMsg == NULL )
    {
        printk("In function %s at line %d\n", __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_PARAM_INVALID;
    }

    if ( list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_DISK_QM_NN_Req_Lock();
    list_for_each_entry_safe(item, next, &_ccmakdbg_disk_qm_req_queue.list, list)
    {        
        memcpy( pReqMsg, &item->req_info, sizeof( CCMAKDBG_DISK_QM_NN_REQ_ENTRY_T ) );    
        list_del(&item->list);
        kfree(item);
        break;
    }
    
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Unlock();
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_Del_All(void)
{
    CCMAKDBG_DISK_QM_NN_REQ_ITEM_T      *item, *next;

    if ( list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }    
    
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Lock();    
    list_for_each_entry_safe(item, next, &_ccmakdbg_disk_qm_req_queue.list, list)
    {        
        list_del(&item->list);
        kfree(item);
    }
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Unlock();
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_ShCCMAKDBG_DISK_Table(void)
{
    CCMAKDBG_DISK_QM_NN_REQ_ITEM_T      *item, *next;

    printk("Vol Id   Req Type \n");
    printk("-------- ---------------\n");
   
    if ( list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_DISK_QM_NN_Req_Lock();
    list_for_each_entry_safe(item, next, &_ccmakdbg_disk_qm_req_queue.list, list)
    {
        printk("%8llu", item->req_info.Vol_id );
        printk("%8d \n", item->req_info.req_type );        
    }
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Unlock();
                
    return CCMAKDBG_ERR_CODE_SUCCESS;            
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_QM_ShCCMAKDBG_DISK_Table_REV(void)
{
    CCMAKDBG_DISK_QM_NN_REQ_ITEM_T      *item;

    printk("Vol Id   Req Type \n");
    printk("-------- ---------------\n");
    
    if ( list_empty(&_ccmakdbg_disk_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_DISK_QM_NN_Req_Lock();
    list_for_each_entry_reverse(item, &_ccmakdbg_disk_qm_req_queue.list, list)
    {
        printk("%8llu", item->req_info.Vol_id );
        printk("%8d \n", item->req_info.req_type );        
    }
    ( void ) CCMAKDBG_DISK_QM_NN_Req_Unlock();
                
    return CCMAKDBG_ERR_CODE_SUCCESS;            
}


