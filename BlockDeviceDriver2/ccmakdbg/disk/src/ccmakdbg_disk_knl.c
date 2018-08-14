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
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/kdev_t.h>
#include <linux/ide.h>
#include <linux/delay.h>


#include "../h/ccmakdbg_disk_knl.h"
#include "../h/ccmakdbg_disk_qm.h"

#define MODULES_NAME               "CCMAKDBG_DISK_KNL:"

MODULE_LICENSE("Dual BSD/GPL");
#if 0
static int _ccmakdbg_disk_knl_bvec_merge_fn(struct request_queue *q, struct bvec_merge_data *bvm, struct bio_vec *biovec);
#endif
static long rx_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int ccma_volume_open(struct inode * inode, struct file* flip);
static int ccma_volume_release(struct inode * inode, struct file* flip);
static int ccmakdbg_disk_knl_create_disk( int index );
static int ccmakdbg_disk_knl_destory_disk( int index );
static void _ccmakdbg_disk_knl_process_io_request(struct request_queue * q );




static struct gendisk  *_ccmakdbg_disk_knl_disks[CCMAKDBG_DISK_KNL_DISK_NUM];
static spinlock_t      _ccmakdbg_disk_knl_queue_lock[CCMAKDBG_DISK_KNL_DISK_NUM];
static int             _ccmakdbg_disk_knl_major=0;
static char            *_ccmakdbg_disk_knl_dev_info;
static int             _ccmakdbg_disk_knl_disk_index;

struct block_device_operations ccmakdbg_disk_knl_fops = { 
	.owner = THIS_MODULE,
    .unlocked_ioctl = rx_unlocked_ioctl,
#if 0    
    .open = ccma_volume_open,
    .release = ccma_volume_release,
#endif    
};


/**
 * function: rx_unlocked_ioctl()
 * description: ioctl routine.
 */
static long rx_unlocked_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg) {
	unsigned long long size;
//	struct hd_geometry geo;
	struct inode    *inode ;
	struct gendisk *disk;

	
	printk(KERN_INFO "rxd: debug: entering %s\n", __func__);
	printk(KERN_INFO "rxd: debug: cmd=%d (BLKFLSBUF=%d BLKGETSIZE=%d BLKSSZGET=%d HDIO_GETGEO=%d)\n", cmd,
			BLKFLSBUF, BLKGETSIZE, BLKSSZGET, HDIO_GETGEO);

	switch (cmd) {
	case BLKFLSBUF:
		printk("rx_unlocked_ioctl BLKFLSBUF: flush buffer\n");
		return 0;
	case BLKGETSIZE:
		printk("rx_unlocked_ioctl BLKGETSIZE: get size\n");
		if (!arg) { 
			printk("rx_unlocked_ioctl BLKGETSIZE: null argument\n");
			return -EINVAL; /* NULL pointer: not valid */
		}
		inode = filp->f_dentry->d_inode;
		disk= inode->i_bdev->bd_disk;
        size = disk->capacity>>CCMAKDBG_DISK_KNL_SECTORS_PER_PAGE_ORDER;
		
		printk("The size of is %llu\n", size);
		return 0;
	case BLKSSZGET:
		printk(KERN_INFO "rxd: debug: request = BLKSSZGET\n");
		size = (1 << CCMAKDBG_DISK_KNL_SECTORS_PER_PAGE_ORDER);
		return 0;		
	case HDIO_GETGEO:
		printk(KERN_INFO "rxd: debug: request = HDIO_GETGEO\n");
		return 0;		
	default:
		printk(KERN_INFO "JERR: default, unknown ioctl %u, (the 21264 cmd is a cdrom ioctl command, don't care!)\n", cmd);
		return -ENOTTY;
	}

	printk(KERN_INFO "JERR:debug: unsupported command: return -ENOTTY\n");
	return -ENOTTY; /* unknown command */
}

static int ccma_volume_open(struct inode * inode, struct file* flip) {

	int retcode = 0;
	unsigned unit = iminor(inode);
	int drive_index = CCMAKDBG_DISK_KNL_GET_INDEX_BY_MINOR(unit);

	printk("%s%s, flip = %p, unit = %d, CCMA_MINOR_NUM = %d \n", MODULES_NAME, __func__, flip, unit, drive_index);

	return retcode;
}

static int ccma_volume_release(struct inode * inode, struct file* flip) {

	int retcode = 0;
	unsigned unit = iminor(inode);
	int drive_index = CCMAKDBG_DISK_KNL_GET_INDEX_BY_MINOR(unit);

	printk("%s%s, unit = %d, CCMA_MINOR_NUM(unit) = %d \n", MODULES_NAME, __func__, unit, drive_index);

	return retcode;

}

void ccmakdbg_disk_knl_unplug_device(struct request_queue *q)
{
    printk("%s In function %s at line %d,  \n",MODULES_NAME, __FUNCTION__, __LINE__ );
#if 0
// 	lfsm_dev_t *td = REQUEST_QUEUE_TO_PITDEV(q);
	struct request_queue * subq = NULL;
	int i;

	for(i = 0; i < LFSM_DISK_NUM; i++){
		
		subq = 	subdisks[i].bdev->bd_disk->queue;
		if(subq && subq->unplug_fn){
			subq->unplug_fn(subq);
		}

	}
#endif	
}

static char check_error_case(struct request *rq, struct request_queue *q) 
{

	if (q==NULL|| IS_ERR(q)){
		printk("%s%s, request_queue ptr = %p\n", MODULES_NAME, __func__, q);
		return -1;
	}
#if 0
	if (blk_queue_plugged(q)){
		DMS_PRINTK("JERR: blk_queue_plugged\n");
		return -1;
	}
#endif

    if (rq == NULL||IS_ERR(rq)) {
    	printk("JERR: rq is null\n");
    	return -1;
	}

	if (!blk_fs_request (rq)) {
		printk(KERN_INFO "JERR: debug: Unsupported fs request: skipping.\n");
		end_request(rq, 0);
		BUG();
	}

	if(elv_queue_empty(q)){
		printk("JERR: elv_queue_empty\n");
		return 1;
	}

	if(list_empty(&rq->queuelist)){
		printk("JERR: queuelist is empty\n");
		return -1;
	}

    return 0;
}

static void _ccmakdbg_disk_knl_print_bio_vect(struct bio_vec *tmp_bio_vec )
{
    printk("           ========struct bio_vec(%p) begin  ========  \n", tmp_bio_vec );  
    printk("           bv_page            = %p \n", tmp_bio_vec->bv_page);
    printk("           bv_len             = %llu \n", tmp_bio_vec->bv_len );
    printk("           bv_offset          = %llu \n", tmp_bio_vec->bv_offset);
    printk("           ========struct bio_vec end         ========\n");
    
    return;
}


static void _ccmakdbg_disk_knl_print_bio(struct bio *tm_bio )
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
    printk("    bi_end_io            = %p \n", tm_bio->bi_end_io);

    for ( i = 0 ; i < tm_bio->bi_max_vecs ; i++ )
    {
        printk("        ==bi_vec_idx(%d)== \n", i);
        _ccmakdbg_disk_knl_print_bio_vect( &tm_bio->bi_io_vec[i]);
    }
    printk("    ========struct bio end    ========  \n");
    return ;
}

static void _ccmakdbg_disk_knl_print_request(struct request * rq )
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
        _ccmakdbg_disk_knl_print_bio(tmp_bio);        
        tmp_bio  = tmp_bio->bi_next;        
    }
    printk("========struct request end   ======== \n");
    return ;
}

static void _ccmakdbg_disk_knl_print_request_queue(struct request_queue * q )
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

static void _ccmakdbg_disk_knl_process_io_request(struct request_queue * q )
{
    struct request * rq;
    struct request * last_rq;    
    unsigned long flags;    
    int    result =1;
    int    v;

    printk("%s In function %s at line %d,  \n",MODULES_NAME, __FUNCTION__, __LINE__ );

    printk("--------------------Begin----------------------------------\n");

   // _ccmakdbg_disk_knl_print_request_queue(q);
	while(!blk_queue_plugged(q))
	{	
	
#if 0	
        spin_unlock_irq(q->queue_lock);
        
		if (current->flags & PF_FROZEN) {
			printk("PF_FREEZE\n");
			refrigerator();
		}
        spin_lock_irq(q->queue_lock);
#endif
		rq = elv_next_request(q);
        v=check_error_case(rq, q);
        if (v>0)
        {
            continue;
        }
        else if(v<0)
        {
            break;
        }
        else
        {
        }
        
        _ccmakdbg_disk_knl_print_request(rq);
        if (blk_fs_request(rq))
        {
        }        
        
		blkdev_dequeue_request(rq);
		
		if (rq_data_dir (rq)) {
            if( !end_that_request_first(rq, result, rq->nr_sectors))
            {                                
                add_disk_randomness(rq->rq_disk);
                end_that_request_last(rq, result);
            }
		} else {		
            if( !end_that_request_first(rq, result, rq->nr_sectors))
            {               
                add_disk_randomness(rq->rq_disk);
                end_that_request_last(rq, result);
            }
		}

		
    }
    printk("--------------------End----------------------------------\n");
    
    
    return;
}
#if 0
static int _ccmakdbg_disk_knl_bvec_merge_fn(struct request_queue *q, struct bvec_merge_data *bvm, struct bio_vec *biovec)
{	
    printk("before,In function %s at line %d, bv_offset=%d, bvec->bv_len=%d \n",__FUNCTION__, __LINE__ ,biovec->bv_offset, biovec->bv_len );
    
    
	return 0;
}
#endif

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Init( void )
{
    int i;

    printk("%s In function %s at line %d\n",MODULES_NAME, __FUNCTION__, __LINE__);    
   
	_ccmakdbg_disk_knl_major = register_blkdev(_ccmakdbg_disk_knl_major, CCMAKDBG_DISK_KNL_DEVICE_NAME_PREFIX);

    for ( i = 0 ; i < CCMAKDBG_DISK_KNL_DISK_NUM; i++ )
    {
        spin_lock_init(&_ccmakdbg_disk_knl_queue_lock[i]);
    }
    
	if (_ccmakdbg_disk_knl_major < 0) {
		printk(KERN_WARNING "%s%s, unable to get major number, returned %d\n",
				MODULES_NAME, __func__, _ccmakdbg_disk_knl_major);
        return CCMAKDBG_ERR_CODE_FAIL;
	}else{
		printk("%s%s, CCMA_BLKDEV_MAJOR = %d\n",
				MODULES_NAME, __func__, _ccmakdbg_disk_knl_major);
	}

    _ccmakdbg_disk_knl_dev_info = kmalloc( 32, GFP_NOWAIT );

	printk(KERN_ALERT "%s driver(major %d) installed.\n", CCMAKDBG_DISK_KNL_DRIVER_NAME, _ccmakdbg_disk_knl_major);

    return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Exit( void )
{

    unregister_blkdev(_ccmakdbg_disk_knl_major, CCMAKDBG_DISK_KNL_DEVICE_NAME_PREFIX);

    kfree(_ccmakdbg_disk_knl_dev_info);
	printk(KERN_ALERT "%s driver removed.\n", MODULES_NAME);


	return CCMAKDBG_ERR_CODE_SUCCESS;
}

/*
 * create a disk drive and link to sysfs. ex: /dev/hda1
 */
CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Create_Disk( int index )
{
	int retcode = -EFAULT;
    
    retcode = ccmakdbg_disk_knl_create_disk(index);

    if ( retcode < 0 )
    {
    	printk(KERN_INFO "%s%s, before alloc disk\n", MODULES_NAME, __func__);

    	return CCMAKDBG_ERR_CODE_FAIL;    	
    }
    
	return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_DISK_KNL_Destory_Disk( int index )
{
	int retcode = -EFAULT;
    
    retcode = ccmakdbg_disk_knl_destory_disk(index);

    if ( retcode < 0 )
    {
    	printk(KERN_INFO "%s%s, before alloc disk\n", MODULES_NAME, __func__);

    	return CCMAKDBG_ERR_CODE_FAIL;    	
    }
    
	return CCMAKDBG_ERR_CODE_SUCCESS;
}

/* Decide if we can merge bio's of two requests
 * @q: request queue
 * @bio: the existing bio structure
 * @bvec: the bvec to be evaluated
 * return value: amount of bytes we can add to the existing bio
 */
static int my_bvec_merge_fn(struct request_queue * q, struct bio * bio, struct bio_vec * bvec)
{
	printk(KERN_INFO "%s%s, before alloc disk\n", MODULES_NAME, __func__);

	int max = 0;
	sector_t sectors=(bio->bi_size)>>9;
	if(sectors == 0){
		max = bvec->bv_len;
	}

	return max;
}


/*
 * create a disk drive and link to sysfs. ex: /dev/hda1
 */
static int ccmakdbg_disk_knl_create_disk( int index ){

	int retcode = -EFAULT;
	struct gendisk *disk = NULL;
    unsigned long long   capacity;

	printk(KERN_INFO "%s%s, before alloc disk\n", MODULES_NAME, __func__);
	
	if ( !(disk = alloc_disk(CCMAKDBG_DISK_KNL_MINORS)) ) {
		printk(KERN_WARNING "%s%s, alloc_disk failed\n", MODULES_NAME, __func__);
		return -ENOMEM;
	}

    if ( IS_ERR(_ccmakdbg_disk_knl_dev_info) )
    {
        printk(KERN_INFO "%s%s, kmalloc error!\n", MODULES_NAME, __func__);
    }
    
    memset(_ccmakdbg_disk_knl_dev_info, 0x0,32);
    
	printk(KERN_INFO "%s%s, after alloc disk, done!\n", MODULES_NAME, __func__);

	disk->major        = _ccmakdbg_disk_knl_major;
	disk->first_minor  = CCMAKDBG_DISK_KNL_GET_MINOR(index);	//the actaul minor number of this disk.
	disk->fops         = &ccmakdbg_disk_knl_fops;
    disk->private_data = _ccmakdbg_disk_knl_dev_info;
    sprintf(disk->disk_name, "%s%d", CCMAKDBG_DISK_KNL_DEVICE_NAME_PREFIX, index );
	capacity = 1024000 << CCMAKDBG_DISK_KNL_SECTORS_PER_PAGE_ORDER;
	set_capacity(disk, capacity);
	printk("%s%s, diskname = %s, capacity = %llu, disk->first_minor = %d\n",
			MODULES_NAME, __func__, disk->disk_name, capacity, disk->first_minor );

	_ccmakdbg_disk_knl_disks[index] = disk;
    _ccmakdbg_disk_knl_disks[index]->queue               = blk_init_queue(_ccmakdbg_disk_knl_process_io_request,&_ccmakdbg_disk_knl_queue_lock[index]);
    _ccmakdbg_disk_knl_disks[index]->queue->nr_requests  = 64;
    _ccmakdbg_disk_knl_disk_index   = index;    
	blk_queue_merge_bvec(_ccmakdbg_disk_knl_disks[index]->queue, my_bvec_merge_fn);
    
//    blk_queue_max_phys_segments( _ccmakdbg_disk_knl_disks[index]->queue,16 );

    //set the hardsector_size
    blk_queue_hardsect_size(_ccmakdbg_disk_knl_disks[index]->queue, (1 << CCMAKDBG_DISK_KNL_SECTORS_PER_PAGE_ORDER) );
 
    //set the max_setors to control the block size
    blk_queue_max_sectors(_ccmakdbg_disk_knl_disks[index]->queue, CCMAKDBG_DISK_KNL_MAX_SECTORS);
    
	add_disk(disk);
    
#if 0
    blk_queue_init_tags(volume->disk->queue, volume->queue->nr_requests,NULL);
#endif
    
 //   disk->queue->unplug_fn = ccmakdbg_disk_knl_unplug_device;//generic_unplug_device;
#if 0    
    blk_queue_merge_bvec(disk->queue, _ccmakdbg_disk_knl_bvec_merge_fn );    
#endif    
  //  blk_queue_make_request(disk->queue, _ccmakdbg_disk_knl_process_io_request);

	printk(KERN_INFO "%s%s, after\n", MODULES_NAME, __func__);

	retcode = 0;

	return retcode;
}

/*
 * call kernel API to flush all remained request in the disk_queue
 */
void ccmakdbg_disk_knl_sync_disk_queue(struct request_queue * disk_queue){

	unsigned long flags = 0;

	printk(KERN_INFO "%s%s, before\n", MODULES_NAME, __func__);

	spin_lock_irqsave(disk_queue->queue_lock, flags);
	//stop disk_queue, it has to get lock before call stop queue.
	blk_stop_queue (disk_queue);

	spin_unlock_irqrestore(disk_queue->queue_lock, flags);

	//sync any remained requests
	blk_sync_queue(disk_queue);

    printk(KERN_INFO "%s%s, after\n", MODULES_NAME, __func__);
}

static int ccmakdbg_disk_knl_destory_disk( int index )
{
    struct request_queue  *disk_queue  =  _ccmakdbg_disk_knl_disks[index]->queue;

	printk(KERN_INFO "%s%s, before\n", MODULES_NAME, __func__);

    ccmakdbg_disk_knl_sync_disk_queue(disk_queue);
    blk_cleanup_queue(disk_queue);
    del_gendisk(_ccmakdbg_disk_knl_disks[index]);
    put_disk(_ccmakdbg_disk_knl_disks[index]);  

    printk(KERN_INFO "%s%s, after\n", MODULES_NAME, __func__);
        
    return 0;
}
#if 0
static void ccmakdbg_disk_knl_printf( CCMAKDBG_DISK_KNL_IOCTL_CMD *io_cmd )
{
    printk("mod_id         : %d \n",  io_cmd->mod_id );
    printk("sub_mod_id     : %d \n",  io_cmd->sub_mod_id );
    printk("cmd_type       : %d \n",  io_cmd->cmd_type );
    printk("sub_cmd_type   : %d \n",  io_cmd->sub_cmd_type );

    
    return ;
}
#endif

