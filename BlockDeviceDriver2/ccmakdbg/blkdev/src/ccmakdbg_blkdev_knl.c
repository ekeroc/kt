#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/hdreg.h>
#include <linux/bio.h>
#include <linux/kdev_t.h>
#include <linux/ide.h>
#include <linux/delay.h>


#include "../h/ccmakdbg_blkdev_knl.h"
#include "../h/ccmakdbg_blkdev_qm.h"

#define MODULES_NAME               "CCMAKDBG_BLKDEV_KNL:"

MODULE_LICENSE("Dual BSD/GPL");

static int ccmakdbg_blkdev_knl_create_blkdev( int index );
static int ccmakdbg_blkdev_knl_destory_blkdev( int index );
static int read_4KB_page( struct block_device *bdev, sector_t pos, int page_num_in_bioc );
static int write_4KB_page(struct block_device *bdev, sector_t pos, struct page *page,int page_num_in_bioc );

static atomic_t  _ccmakdbg_blkdev_knl_io_action;
#define    CCMAKDBG_BLKDEV_KNL_IO_ACTION_IDLE       0
#define    CCMAKDBG_BLKDEV_KNL_IO_ACTION_ONGOING    1
#define    CCMAKDBG_BLKDEV_KNL_IO_ACTION_RESPONE    3
#define    CCMAKDBG_BLKDEV_KNL_IO_ACTION_DONE       4

static struct block_device  *_ccmakdbg_blkdev_knl_blkdevs[CCMAKDBG_BLKDEV_KNL_BLKDEV_NUM];
static spinlock_t      _ccmakdbg_blkdev_knl_queue_lock[CCMAKDBG_BLKDEV_KNL_BLKDEV_NUM];
struct bio             _ccmakdbg_blkdev_bio_tbl[CCMAKDBG_BLKDEV_KNL_BLKDEV_NUM];
wait_queue_head_t      _ccmakdbg_io_queue;

static char            *_ccmakdbg_blkdev_knl_dev_info;
static int             _ccmakdbg_blkdev_knl_blkdev_index;

static void _ccmakdbg_blkdev_knl_print_bio_vect(struct bio_vec *tmp_bio_vec )
{
    printk("           ========struct bio_vec(%p) begin  ========  \n", tmp_bio_vec );  
    printk("           bv_page            = %p \n", tmp_bio_vec->bv_page);
    printk("           bv_len             = %llu \n", tmp_bio_vec->bv_len );
    printk("           bv_offset          = %llu \n", tmp_bio_vec->bv_offset);
    printk("           ========struct bio_vec end         ========\n");
    
    return;
}


static void _ccmakdbg_blkdev_knl_print_bio(struct bio *tm_bio )
{
    int i;
    
    if ( tm_bio == NULL )
    {
        return;
    }
    
    printk("    ========struct bio(%p) begin ========   \n", tm_bio );  
    printk("    bi_sector            = %llu \n", tm_bio->bi_sector);
    printk("    bi_rw                = %llu \n", tm_bio->bi_rw);
    printk("    bi_vcnt              = %llu \n", tm_bio->bi_vcnt);
    printk("    bi_idx               = %llu \n", tm_bio->bi_idx);    
    printk("    bi_size              = %llu \n", tm_bio->bi_size);
    printk("    bi_max_vecs          = %llu \n", tm_bio->bi_max_vecs);    
    printk("    bi_phys_segments     = %llu \n", tm_bio->bi_phys_segments);
    printk("    bi_flags             = %llu \n", tm_bio->bi_flags);
    printk("    bi_end_io            = %llu \n", tm_bio->bi_end_io);
    
    for ( i = 0 ; i < tm_bio->bi_max_vecs ; i++ )
    {
        printk("        ==bi_vec_idx(%d)== \n", i);
        _ccmakdbg_blkdev_knl_print_bio_vect( &tm_bio->bi_io_vec[i]);
    }
    printk("    ========struct bio end    ========  \n");
    return ;
}

static void _ccmakdbg_blkdev_knl_print_request(struct request * rq )
{
    int i;
    struct bio *tmp_bio;
    
    if ( rq == NULL )
    {
        return;
    }

    printk("========struct request(%p) begin   ======== \n", rq );
    printk("sector                   = %lu \n", rq->sector);
    printk("nr_sectors               = %lu \n", rq->nr_sectors);
    printk("current_nr_sectors       = %lu \n", rq->current_nr_sectors);
    printk("hard_sector              = %lu \n", rq->hard_sector);
    printk("hard_nr_sectors          = %lu \n", rq->hard_nr_sectors);
    printk("hard_cur_sectors         = %lu \n", rq->hard_cur_sectors);
    printk("data_len                 = %lu \n", rq->data_len);
    printk("retries                  = %lu \n", rq->retries);   

    tmp_bio = rq->bio;
    i = 0 ;
    while(1)
    {
        if ( tmp_bio == NULL )
        {
            break;
        }
        i++;
        printk("   ==bio_idx(%d)== \n", i);        
        _ccmakdbg_blkdev_knl_print_bio(tmp_bio);        
        tmp_bio  = tmp_bio->bi_next;        
    }
    printk("========struct request end   ======== \n");
    return ;
}

static void _ccmakdbg_blkdev_knl_print_request_queue(struct request_queue * q )
{
    if ( q == NULL )
    {
        return;
    }
    
    printk(" struct request_queue(%p) begin   \n", q );
    printk("nr_requests         = %lu \n", q->nr_requests);
    printk("nr_batching         = %lu \n", q->nr_batching);
    printk("max_sectors         = %lu \n", q->max_sectors);
    printk("max_hw_sectors      = %lu \n", q->max_hw_sectors);
    printk("max_phys_segments   = %lu \n", q->max_phys_segments);
    printk("max_hw_segments     = %lu \n", q->max_hw_segments);
    printk("max_segment_size    = %lu \n", q->max_segment_size);
    printk("request end_sector=%llu, bi_size=%d \n", q->end_sector, q->bi_size );
    printk("request_fn          = %p  \n", q->request_fn);
    printk("merge_requests_fn   = %p  \n", q->merge_requests_fn);
    printk(" struct request_queue end   \n");
    
    return ;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Init( void )
{

    printk("%s In function %s at line %d\n",MODULES_NAME, __FUNCTION__, __LINE__);    

    init_waitqueue_head(&_ccmakdbg_io_queue);

    _ccmakdbg_blkdev_knl_dev_info = kmalloc( 32, GFP_NOWAIT );

	printk(KERN_ALERT "%s driver installed.\n", CCMAKDBG_BLKDEV_KNL_DRIVER_NAME);

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Exit( void )
{
    kfree(_ccmakdbg_blkdev_knl_dev_info);
	printk(KERN_ALERT "%s driver removed.\n", MODULES_NAME);


	return CCMAKDBG_ERR_CODE_SUCCESS;
}
int my_make_request(struct bio *bio)
{
		struct request_queue *subq = NULL;

		subq = bio->bi_bdev->bd_disk->queue;

		if(subq && subq->make_request_fn){
            subq->make_request_fn(subq, bio);
		}
		if(subq && subq->unplug_fn){
			subq->unplug_fn(subq);
		}
	return 0;
}

void end_bio_page_read(struct bio *bio, unsigned int bytes_done,  int err)
{  
    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_RESPONE );
    wake_up(&_ccmakdbg_io_queue);
	
	return;
}

static int read_4KB_page( struct block_device *bdev, sector_t pos, int page_num_in_bioc )
{
    struct page *page;
	struct bio* page_bio;
	struct bio_vec* bv;
	int ret=0,try=0;
	int i = 0;
	//Compose a page bio for read

    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_IDLE);
    
	while (!(page_bio = bio_alloc(GFP_ATOMIC| __GFP_ZERO, 1))) {
	    printk("allocate header_bio fails in read_4KB_page\n");
	    schedule();
	}
	
	page_bio->bi_sector        = pos; 
	page_bio->bi_bdev          = bdev;  
	page_bio->bi_idx           = 0;
	page_bio->bi_end_io        = end_bio_page_read;
	page_bio->bi_private       = _ccmakdbg_blkdev_knl_dev_info;
	page_bio->bi_vcnt          = page_num_in_bioc;
	page_bio->bi_size          = PAGE_SIZE*page_num_in_bioc;
	page_bio->bi_max_vecs      = page_num_in_bioc;
	page_bio->bi_rw            = READ;
	page_bio->bi_flags         = 1 << BIO_UPTODATE;
	page_bio->bi_phys_segments = 0;
	page_bio->bi_next = NULL;	

    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_ONGOING);

    for ( i =0 ; i < page_num_in_bioc ; i++ )
    {   
        while (!(page = alloc_page(__GFP_ZERO))) {
            printk("allocate header_page fails in compose_bio\n");
            schedule();
        }
    
        bv = bio_iovec_idx(page_bio, i );
        bv->bv_page = page;
        bv->bv_offset = i;
        bv->bv_len = PAGE_SIZE;        
        my_make_request(page_bio);  
        
    }	

    wait_event_interruptible(_ccmakdbg_io_queue, 
                             atomic_read(&_ccmakdbg_blkdev_knl_io_action) != CCMAKDBG_BLKDEV_KNL_IO_ACTION_ONGOING );
                             
	// free bio don't free page becasue we use write_io's page
	bio_put(page_bio);
    for ( i =0 ; i < page_num_in_bioc ; i++ )
    {
    	__free_page(page_bio->bi_io_vec[i].bv_page); 
    }
    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_DONE );
	
	return ret;
}

static int write_4KB_page(struct block_device *bdev, sector_t pos, struct page *page, int page_num_in_bioc )
{
	struct bio* page_bio;
	struct bio_vec* bv;
	int ret=0,try=0;

    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_IDLE);

	//Compose a page bio for read
	while (!(page_bio = bio_alloc(GFP_ATOMIC| __GFP_ZERO, 1))) {
	    printk("allocate header_bio fails in read_4KB_page\n");
	    schedule();
	};
	
	page_bio->bi_sector  = pos; 
	page_bio->bi_bdev    = bdev;  
	page_bio->bi_idx     = 0;
	page_bio->bi_end_io  = end_bio_page_read;
	page_bio->bi_private = _ccmakdbg_blkdev_knl_dev_info;
	page_bio->bi_vcnt    = 16;   // 1 page
	page_bio->bi_size    = PAGE_SIZE; // 1 pages
	page_bio->bi_rw      = WRITE;
	page_bio->bi_flags   = 1 << BIO_UPTODATE;
	page_bio->bi_phys_segments = 0;
	page_bio->bi_next    = NULL;	

	bv = bio_iovec_idx(page_bio, 0);
	bv->bv_page = page;
	bv->bv_offset = 0;
	bv->bv_len = PAGE_SIZE;
	
	while(try++ < 1)
	{
        atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_ONGOING);
        my_make_request(page_bio);  
        wait_event_interruptible(_ccmakdbg_io_queue, 
                                 atomic_read(&_ccmakdbg_blkdev_knl_io_action) != CCMAKDBG_BLKDEV_KNL_IO_ACTION_ONGOING );
	}

	printk("pid#thread:%d#%d Decompress bio pbno:%llu bi_size: %d\n",current->pid,current->tgid,pos,page_bio->bi_size);

	// free bio don't free page becasue we use write_io's page
	bio_put(page_bio);
	__free_page(page_bio->bi_io_vec[0].bv_page); 

	
    atomic_set(&_ccmakdbg_blkdev_knl_io_action, CCMAKDBG_BLKDEV_KNL_IO_ACTION_DONE );
	
	return 0;

}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Submit_Bio( int index )
{
    int i;

    for ( i = 0 ; i <= 5 ; i++ )
    {
        read_4KB_page( _ccmakdbg_blkdev_knl_blkdevs[index], 32+(i*64)   , 16 );
    }
#if 0
    while (!(bio_page = alloc_page(__GFP_ZERO))) {
        printk("allocate header_page fails in compose_bio\n");
        schedule();
    }
    
    write_4KB_page( _ccmakdbg_blkdev_knl_blkdevs[index], 0,  bio_page, 1);
#endif
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

/*
 * create a blkdev drive and link to sysfs. ex: /dev/hda1
 */
CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Create_Disk( int index )
{
	int retcode = -EFAULT;
    
    retcode = ccmakdbg_blkdev_knl_create_blkdev(index);

    if ( retcode < 0 )
    {
    	printk(KERN_INFO "%s%s, before alloc blkdev\n", MODULES_NAME, __func__);

    	return CCMAKDBG_ERR_CODE_FAIL;    	
    }
    
	return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_BLKDEV_KNL_Destory_Disk( int index )
{
	int retcode = -EFAULT;
    
    retcode = ccmakdbg_blkdev_knl_destory_blkdev(index);

    if ( retcode < 0 )
    {
    	printk(KERN_INFO "%s%s, before alloc blkdev\n", MODULES_NAME, __func__);

    	return CCMAKDBG_ERR_CODE_FAIL;    	
    }
    
	return CCMAKDBG_ERR_CODE_SUCCESS;
}

int compose_bio(struct bio ** biop, struct block_device * bdev, bio_end_io_t *bi_end_io, void * bi_private, int bi_size, int bi_vec_size)
{
    struct page * bio_page;
    struct bio * bio;
    int i = 0;
    int org_bi_size = bi_size;
		
    while (!(bio = bio_alloc(__GFP_WAIT|__GFP_ZERO, bi_vec_size))) {
	    
		printk("allocate header_bio fails in compose_bio\n");
	    schedule();
    };

    for(i = 0; i < bi_vec_size; i++){
	    /* Grab a free page and free bio to hold the log record header */
	    while (!(bio_page = alloc_page(__GFP_ZERO))) {
		    printk("allocate header_page fails in compose_bio\n");
		    schedule();
	    }

	    printk("In %s at line %d, bio_page: %p\n", __FUNCTION__, __LINE__, bio_page);
	    bio->bi_io_vec[i].bv_page = bio_page;
	    bio->bi_io_vec[i].bv_offset = 0;

	    if(org_bi_size < PAGE_SIZE)
	    	bio->bi_io_vec[i].bv_len = org_bi_size;
	    else
			bio->bi_io_vec[i].bv_len = PAGE_SIZE;

	    org_bi_size -= PAGE_SIZE;
    
    }

    bio->bi_sector  = -1; /* we do not know the dest_LBA yet */
    bio->bi_bdev    = bdev;  /* set header_bio with same value as bio */
    bio->bi_vcnt    = bi_vec_size;
    bio->bi_idx     = 0;
    bio->bi_size    = bi_size;
    bio->bi_end_io  = bi_end_io;
    bio->bi_private = bi_private;
    *biop = bio;

    return 0;
}

/*
 * create a blkdev drive and link to sysfs. ex: /dev/hda1
 */
static int ccmakdbg_blkdev_knl_create_blkdev( int index ){

	int retcode = -EFAULT;
	struct block_dev *blkdev = NULL;

	printk(KERN_INFO "%s%s, before alloc disk\n", MODULES_NAME, __func__);


    blkdev = open_bdev_excl("/dev/ccmakdbg_disk1", O_RDWR, NULL);
    if ( IS_ERR(blkdev) )
    {
        printk(KERN_INFO "%s%s, kmalloc error!\n", MODULES_NAME, __func__);
    }
    
    memset(_ccmakdbg_blkdev_knl_dev_info, 0x0,32);
#if 0
	( void ) compose_bio(&_ccmakdbg_blkdev_bio_tbl[index], blkdev, end_bio_metalog, _ccmakdbg_blkdev_knl_dev_info, PAGE_SIZE*16, 16 );
#endif
	_ccmakdbg_blkdev_knl_blkdevs[index] = blkdev;
    _ccmakdbg_blkdev_knl_blkdev_index   = index;

	printk(KERN_INFO "%s%s, after\n", MODULES_NAME, __func__);

	return 0;
}

static int ccmakdbg_blkdev_knl_destory_blkdev( int index )
{
	printk(KERN_INFO "%s%s, before\n", MODULES_NAME, __func__);
    close_bdev_excl(_ccmakdbg_blkdev_knl_blkdevs[index]);
    printk(KERN_INFO "%s%s, after\n", MODULES_NAME, __func__);
    
    return 0;
}
