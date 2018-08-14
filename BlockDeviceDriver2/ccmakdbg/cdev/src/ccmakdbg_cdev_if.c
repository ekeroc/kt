#include <linux/version.h>
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
#include "../inc/ccmakdbg_cdev_if.h"
#include "../h/ccmakdbg_cdev_qm.h"
#include "../h/ccmakdbg_cdev_cbf_tbl.h"

#define MODULES_NAME               "CCMAKDBG_CDEV_IF:"

MODULE_LICENSE("Dual BSD/GPL");


static int ccmakdbg_cdev_num 		  = 1;
static int ccmakdbg_cdev_major 		  = 0;
static int ccmakdbg_cdev_minor  		  = 0;
#if CCMAKDBG_CDEV_REG_CLASS
static struct class *kdbug_cdev_class = NULL;
#endif
module_param(ccmakdbg_cdev_major, uint, 0);
static struct cdev ccmakdbg_cdev_cdev;
#if CCMAKDBG_CDEV_REG_CLASS
static dev_t ccmakdbg_cdev_dev;
#endif

static int ccmakdbg_cdev_open(struct inode *inode, struct file *file);
static int ccmakdbg_cdev_close(struct inode *inode, struct file *file);
ssize_t ccmakdbg_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
ssize_t ccmakdbg_cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

static void ccmakdbg_cdev_printf( CCMAKDBG_CDEV_IF_IOCTL_CMD *io_cmd );

struct ccmakdbg_cdev_data {
	CCMAKDBG_CDEV_IF_IOCTL_CMD   ioctl_cmd;
	rwlock_t lock;
};

int ccmakdbg_cdev_ioctl_common(struct ccmakdbg_cdev_data *dev, unsigned int ioctl_num, unsigned long ioctl_param);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
long ccmakdbg_cdev_ioctl(struct file *file,	unsigned int ioctl_num, unsigned long ioctl_param) {
	struct ccmakdbg_cdev_data *dev = file->private_data;
	return (long)ccmakdbg_cdev_ioctl_common(dev, ioctl_num, ioctl_param);
}
#else
int ccmakdbg_cdev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
int ccmakdbg_cdev_ioctl(struct inode *inode, /* see include/linux/fs.h */
                 struct file *file, /* ditto */
                 unsigned int ioctl_num, /* number and param for ioctl */
                 unsigned long ioctl_param) {
	struct ccmakdbg_cdev_data *dev = file->private_data;
	return ccmakdbg_cdev_ioctl_common(dev, ioctl_num, ioctl_param);
}
#endif

struct file_operations ccmakdbg_cdev_fops = {
	.owner	  = THIS_MODULE,
	.open     = ccmakdbg_cdev_open,
	.release  = ccmakdbg_cdev_close,
	.read     = ccmakdbg_cdev_read,
	.write    = ccmakdbg_cdev_write,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
	.unlocked_ioctl = ccmakdbg_cdev_ioctl,
#else
	.ioctl = ccmakdbg_cdev_ioctl,
#endif
};

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Init( void )
{
    CCMAKDBG_ERR_CODE_T ret;
	dev_t dev = MKDEV(ccmakdbg_cdev_major, 0);
	int alloc_ret = 0;
	int major;
	int cdev_err = 0;
#if CCMAKDBG_CDEV_REG_CLASS
	//struct class_device *class_dev = NULL;
#endif

    printk("%s In function %s at line %d\n",MODULES_NAME, __FUNCTION__, __LINE__);    

	alloc_ret = alloc_chrdev_region(&dev, 0, ccmakdbg_cdev_num , CCMAKDBG_DRIVER_NAME );
	if (alloc_ret)
		goto error;
	ccmakdbg_cdev_major = major = MAJOR(dev);

	cdev_init(&ccmakdbg_cdev_cdev, &ccmakdbg_cdev_fops);
	ccmakdbg_cdev_cdev.ops = &ccmakdbg_cdev_fops;
	cdev_err = cdev_add(&ccmakdbg_cdev_cdev, MKDEV(ccmakdbg_cdev_major, ccmakdbg_cdev_minor), ccmakdbg_cdev_num);
	if (cdev_err) 
		goto error;

#if CCMAKDBG_CDEV_REG_CLASS
	/* register class */
	kdbug_cdev_class = class_create(THIS_MODULE, CCMAKDBG_DRIVER_NAME );
	if (IS_ERR(kdbug_cdev_class)) {
		goto error;
	}

	ccmakdbg_cdev_dev = MKDEV(ccmakdbg_cdev_major, ccmakdbg_cdev_minor);
	//class_dev =
	class_device_create(
					kdbug_cdev_class, 
					NULL, 
					ccmakdbg_cdev_dev,
					NULL, 
					"ccmakdbg_d%d",
					ccmakdbg_cdev_minor );
#endif
	printk(KERN_ALERT "%s driver(major %d) installed.\n", CCMAKDBG_DRIVER_NAME, ccmakdbg_cdev_major);

    ret = CCMAKDBG_CDEV_CBF_TBL_Init();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS OW: CCMAKDBG_CDEV_IF_Init() fail!\n");    
        return ret;
    }


    ret = CCMAKDBG_CDEV_QM_Init();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS OW: CCMAKDBG_CDEV_IF_Init() fail!\n");    
        return ret;
    }

	return CCMAKDBG_ERR_CODE_SUCCESS;

error:
	if (cdev_err == 0)
		cdev_del(&ccmakdbg_cdev_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, ccmakdbg_cdev_num);

	return -1;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Exit( void )
{
    CCMAKDBG_ERR_CODE_T ret;

	dev_t dev = MKDEV(ccmakdbg_cdev_major, 0);

    printk("%s In function %s at line %d\n",MODULES_NAME, __FUNCTION__, __LINE__);    
	
#if CCMAKDBG_CDEV_REG_CLASS
	/* unregister class */
	class_device_destroy(kdbug_cdev_class, ccmakdbg_cdev_dev);
	class_destroy(kdbug_cdev_class);
#endif

    ret = CCMAKDBG_CDEV_CBF_TBL_Exit();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("DMS OW: CCMAKDBG_CDEV_IF_Init() fail!\n");    
        return ret;
    }

    ret = CCMAKDBG_CDEV_QM_Exit();
    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("In function %s at line %d failed\n", __FUNCTION__, __LINE__);    
        return ret;
    }

	cdev_del(&ccmakdbg_cdev_cdev);
	unregister_chrdev_region(dev, ccmakdbg_cdev_num);

	printk(KERN_ALERT "%s driver removed.\n", CCMAKDBG_DRIVER_NAME);

	return CCMAKDBG_ERR_CODE_SUCCESS;
}

CCMAKDBG_ERR_CODE_T CCMAKDBG_CDEV_IF_Register_Callback( CCMAKDBG_GLOBAL_MOD_ID_T mod_id, int func_id,  CCMAKDBG_CDEV_IF_CB_FUNC callback, int payload_size )
{
    CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T    cbf_ent;
    CCMAKDBG_ERR_CODE_T                   ret;
    
    if ( mod_id >= CCMAKDBG_GLOBAL_MOD_ID_END )
    {
        printk("CCMAKDBG_CDEV: In function %s at line %d failed!!\n", __FUNCTION__, __LINE__);
        return CCMAKDBG_ERR_CODE_FAIL;
    }

    if ( payload_size > CCMAKDBG_CDEV_MAX_PAYLOAD_LEN )
    {
        printk(" In function %s at line2 %d\n", __FUNCTION__, __LINE__);    
        return CCMAKDBG_ERR_CODE_FAIL;
    }

    ret = CCMAKDBG_CDEV_CBF_TBL_Get_Entry( mod_id, func_id, &cbf_ent );

    if ( ret == CCMAKDBG_ERR_CODE_SUCCESS )
    {
        /*
         * Update an old entry.
         */
        cbf_ent.cb_func         = callback;
        cbf_ent.payload_size    = payload_size;
    }
    else
    {    
        /*
         * Add a new entry.
         */
        cbf_ent.mod_id          = mod_id;
        cbf_ent.cb_func         = callback;
        cbf_ent.func_id         = func_id;
        cbf_ent.payload_size    = payload_size;
    }
    
    ret = CCMAKDBG_CDEV_CBF_TBL_Add_Entry( mod_id, &cbf_ent);

    if ( ret != CCMAKDBG_ERR_CODE_SUCCESS )
    {
        printk("CCMAKDBG_CDEV: In function %s at line %d failed!!\n", __FUNCTION__, __LINE__);    
    }
    
    return CCMAKDBG_ERR_CODE_SUCCESS;
}

static int ccmakdbg_cdev_open(struct inode *inode, struct file *file)
{
	struct ccmakdbg_cdev_data *p;

	printk("%s: major %d minor %d (pid %d)\n", __func__,
			imajor(inode),
			iminor(inode),
			current->pid
		  );

	p = kmalloc(sizeof(struct ccmakdbg_cdev_data), GFP_KERNEL);
	if (p == NULL) {
		printk("%s: Not memory\n", __func__);
		return -ENOMEM;
	}

    memset(p, 0x0, sizeof(struct ccmakdbg_cdev_data) );
	rwlock_init(&p->lock);

	file->private_data = p;

	return 0;
}

static int ccmakdbg_cdev_close(struct inode *inode, struct file *file)
{
	printk("%s: major %d minor %d (pid %d)\n", __func__,
			imajor(inode),
			iminor(inode),
			current->pid
		  );

	if (file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

ssize_t ccmakdbg_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct ccmakdbg_cdev_data *p = filp->private_data;
	unsigned char val;
	int retval = 0;

	printk("%s: count %d pos %lld\n", __func__, ( int ) count, *f_pos);

	if (count >= 1) {
		if (copy_from_user(&val, &buf[0], 1)) {
			retval = -EFAULT;
			goto out;
		}

		write_lock(&p->lock);
		printk("%02x ", val);
		write_unlock(&p->lock);
		retval = count;
	}

out:
	return (retval);
}

ssize_t ccmakdbg_cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct ccmakdbg_cdev_data *p = filp->private_data;
	int i;
	unsigned char val;
	int retval;

	read_lock(&p->lock);
	read_unlock(&p->lock);

	printk("%s: count %d pos %lld\n", __func__, ( int ) count, *f_pos);

	for (i = 0 ; i < count ; i++) {
		if (copy_to_user(&buf[i], &val, 1)) {
			retval = -EFAULT;
			goto out;
		}
	}

	retval = count;
out:
	return (retval);
}

//int ccmakdbg_cdev_ioctl_common(struct inode *inode, struct file *filp,
//					unsigned int cmd, unsigned long arg)
int ccmakdbg_cdev_ioctl_common(struct ccmakdbg_cdev_data *dev, unsigned int cmd, unsigned long arg) {
	int retval = 0;

    CCMAKDBG_CDEV_CBF_TBL_FUNC_ENTRY_T    cbf_ent;
	CCMAKDBG_CDEV_IF_IOCTL_CMD            data;
    CCMAKDBG_ERR_CODE_T                   ret;


	memset(&data, 0, sizeof(CCMAKDBG_CDEV_IF_IOCTL_CMD));
	switch (cmd) {
	case CCMAKDBG_CDEV_IF_IOCTL_VALSET:
		if (!capable(CAP_SYS_ADMIN)) {
			retval = -EPERM;
			goto done;
		}
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user((char*)&data, (int __user *)arg, sizeof(CCMAKDBG_CDEV_IF_IOCTL_CMD)) ) {
			retval = -EFAULT;
			goto done;
		}

		ccmakdbg_cdev_printf(&data);

		write_lock(&dev->lock);
		memcpy( &dev->ioctl_cmd, &data,  sizeof(CCMAKDBG_CDEV_IF_IOCTL_CMD));
		write_unlock(&dev->lock);

		if ( data.mod_id >= CCMAKDBG_GLOBAL_MOD_ID_END )
		{
			printk("CCMAKDBG_CDEV: In function %s at line %d failed!!\n", __FUNCTION__, __LINE__);
			retval = -EFAULT;
			goto done;
		}

		ret = CCMAKDBG_CDEV_CBF_TBL_Get_Entry( data.mod_id, data.func_id , &cbf_ent );
		if ( ret == CCMAKDBG_ERR_CODE_SUCCESS )
		{
			if ( cbf_ent.cb_func == NULL )
			{
    			retval = -EFAULT;
    			printk("CCMAKDBG_CDEV: In function %s at line %d failed!!\n", __FUNCTION__, __LINE__);
    			goto done;
			}
			(*cbf_ent.cb_func) (data );
		} else {
			printk("CCMAKDBG_CDEV: In function %s at line %d failed, ret=%d!!\n", __FUNCTION__, __LINE__, ret );
		}
		break;
	case CCMAKDBG_CDEV_IF_IOCTL_VALGET:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}
		read_lock(&dev->lock);
		memcpy( &data, &dev->ioctl_cmd, sizeof(CCMAKDBG_CDEV_IF_IOCTL_CMD));
		read_unlock(&dev->lock);

		ccmakdbg_cdev_printf(&data);

		if ( copy_to_user((int __user *)arg, &data, sizeof(CCMAKDBG_CDEV_IF_IOCTL_CMD)) ) {
			retval = -EFAULT;
			goto done;
		}
		break;

	default:
		retval = -ENOTTY;
		break;
	}
done:
	return (retval);
}

static void ccmakdbg_cdev_printf( CCMAKDBG_CDEV_IF_IOCTL_CMD *io_cmd )
{
    printk("mod_id         : %d \n",  io_cmd->mod_id );
    printk("func_id        : %d \n",  io_cmd->func_id );    
    printk("payload1       : %2x \n",  io_cmd->payload[1]);
    printk("payload2       : %2x \n",  io_cmd->payload[2] );
    printk("payload3       : %2x \n",  io_cmd->payload[3] );

    
    return ;
}

