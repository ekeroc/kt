#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../h/ccmakdbg_blkdev_qm.h"

#define MODULES_NAME               "CCMAKDBG_BLKDEV_QM:"

typedef struct
{
    struct list_head  list;
    struct mutex      queue_lock;
} CCMAKDBG_BLKDEV_QM_NN_REQ_QUEUE_T;

typedef struct
{
    struct list_head     list;
    CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T req_info;
} CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T;

static CCMAKDBG_BLKDEV_QM_NN_REQ_QUEUE_T  _ccmakdbg_blkdev_qm_req_queue;

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Init(void)
{   
    mutex_init(&_ccmakdbg_blkdev_qm_req_queue.queue_lock);
    INIT_LIST_HEAD(&_ccmakdbg_blkdev_qm_req_queue.list); 
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Exit(void)
{
    if ( !list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
      ( void ) CCMAKDBG_BLKDEV_QM_Del_All();
    }
    
    mutex_destroy(&_ccmakdbg_blkdev_qm_req_queue.queue_lock);
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_NN_Req_Lock(  )
{
    mutex_lock(&_ccmakdbg_blkdev_qm_req_queue.queue_lock);
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_NN_Req_Unlock(  )
{
    mutex_unlock(&_ccmakdbg_blkdev_qm_req_queue.queue_lock);
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Enqueue_Req_Msg( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T ReqMsg )
{
    CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T      *new_ent;

    new_ent = kmalloc( sizeof(CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T) , GFP_ATOMIC);  

    if ( new_ent == NULL )
    {
        printk("%s In function %s at line %d\n", MODULES_NAME, __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_NO_MEMORY;    
    }
    
    memcpy( &new_ent->req_info, &ReqMsg, sizeof( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T ) );    
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Lock();
    list_add_tail(&new_ent->list, &_ccmakdbg_blkdev_qm_req_queue.list);
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Unlock();

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Queue_Is_Empty( void )
{
    if ( list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}


CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Dequeue_Req_Msg( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T *pReqMsg )
{
    CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T    *item, *next;
    

    if ( pReqMsg == NULL )
    {
        printk("In function %s at line %d\n", __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_PARAM_INVALID;
    }

    if ( list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Lock();
    list_for_each_entry_safe(item, next, &_ccmakdbg_blkdev_qm_req_queue.list, list)
    {        
        memcpy( pReqMsg, &item->req_info, sizeof( CCMAKDBG_BLKDEV_QM_NN_REQ_ENTRY_T ) );    
        list_del(&item->list);
        kfree(item);
        break;
    }
    
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Unlock();
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_Del_All(void)
{
    CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T      *item, *next;

    if ( list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }    
    
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Lock();    
    list_for_each_entry_safe(item, next, &_ccmakdbg_blkdev_qm_req_queue.list, list)
    {        
        list_del(&item->list);
        kfree(item);
    }
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Unlock();
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_ShCCMAKDBG_BLKDEV_Table(void)
{
    CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T      *item, *next;

    printk("Vol Id   Req Type \n");
    printk("-------- ---------------\n");
   
    if ( list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Lock();
    list_for_each_entry_safe(item, next, &_ccmakdbg_blkdev_qm_req_queue.list, list)
    {
        printk("%8llu", item->req_info.Vol_id );
        printk("%8d \n", item->req_info.req_type );        
    }
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Unlock();
                
    return CCMAKDBG_ERR_CODE_SUCCESS;            
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_QM_ShCCMAKDBG_BLKDEV_Table_REV(void)
{
    CCMAKDBG_BLKDEV_QM_NN_REQ_ITEM_T      *item;

    printk("Vol Id   Req Type \n");
    printk("-------- ---------------\n");
    
    if ( list_empty(&_ccmakdbg_blkdev_qm_req_queue.list))
    {
        return CCMAKDBG_ERR_CODE_QUEUE_EMPTY;
    }

    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Lock();
    list_for_each_entry_reverse(item, &_ccmakdbg_blkdev_qm_req_queue.list, list)
    {
        printk("%8llu", item->req_info.Vol_id );
        printk("%8d \n", item->req_info.req_type );        
    }
    ( void ) CCMAKDBG_BLKDEV_QM_NN_Req_Unlock();
                
    return CCMAKDBG_ERR_CODE_SUCCESS;            
}


