/*example of char dev driver*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/uio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>


#define CDEVDEMO_MAJOR 255
#define	MEM_CLEAR	_IO(0xa1, 0)
#define MEM_SIZE 0x1000

static int cdevdemo_major = CDEVDEMO_MAJOR;
struct cdevdemo_dev {
	struct cdev cdev;
	unsigned char mem[MEM_SIZE];
};

struct cdevdemo_dev *cdevdemo_devp;
static int cdevdemo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = cdevdemo_devp;
	return 0;
}

static int cdevdemo_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long cdevdemo_ioctl(struct file *filp, unsigned
			  int cmd, unsigned long arg)
{
	struct cdevdemo_dev *dev = filp->private_data;

	switch (cmd) {
	case MEM_CLEAR:
		memset(dev->mem, 0, MEM_SIZE);
		printk(KERN_INFO "globalmem is set to zero\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static ssize_t cdevdemo_read(struct file *filp, char __user *buf, size_t size,
			     loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size ;
	int ret = 0;
	struct cdevdemo_dev *dev = filp->private_data;

	if (p >= MEM_SIZE)
		return count ? -ENXIO : 0;
	if (count > MEM_SIZE - p)
		count = MEM_SIZE - p;

	if (copy_to_user(buf, (void *)(dev->mem + p), count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
		printk(KERN_DEBUG "read %d bytes(s) from %lu\n", count, p); }
	return ret;
}

static ssize_t cdevdemo_write(struct file *filp, const char __user *buf, size_t size,
			      loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct cdevdemo_dev *dev = filp->private_data;

	if (p >= MEM_SIZE)
		return count ? -ENXIO : 0;
	if (count > MEM_SIZE - p)
		count =  MEM_SIZE - p;

	if (copy_from_user((void *)(dev->mem + p), buf, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
		printk(KERN_DEBUG "written %d bytes(s) from %lu\n", count, p);
	}
	return ret;
}

static const struct file_operations cdevdemo_ops = {
	.owner = THIS_MODULE,
	.read  = cdevdemo_read,
	.write = cdevdemo_write,
	.open  = cdevdemo_open,
	.unlocked_ioctl = cdevdemo_ioctl,
	.release = cdevdemo_release,
};

static void cdevdemo_setup_cdev(struct cdevdemo_dev *dev, int index)
{
	int err;
	dev_t devno = MKDEV(cdevdemo_major, index);

	cdev_init(&dev->cdev, &cdevdemo_ops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &cdevdemo_ops;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		printk(KERN_NOTICE "Error %d adding cdevdemo", err);
}


static int cdevdemo_init(void)
{
	int ret;
	dev_t devno = MKDEV(cdevdemo_major, 0);

	if (cdevdemo_major) {
		ret = register_chrdev_region(devno, 1, "cdevdemo");
	} else {
		ret = alloc_chrdev_region(&devno, 0, 1, "cdevdemo");
		cdevdemo_major = MAJOR(devno);
	}
	if (ret < 0)
		return ret;

	/*alloc struct dynamic*/
	cdevdemo_devp = kmalloc(sizeof(struct cdevdemo_dev), GFP_KERNEL);
	if (!cdevdemo_devp) {
		ret = -ENOMEM;
		return ret;
		goto fail_malloc;
	}

	memset(cdevdemo_devp, 0, sizeof(struct cdevdemo_dev));
	cdevdemo_setup_cdev(cdevdemo_devp, 0);
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void cdevdemo_exit(void)
{
	cdev_del(&cdevdemo_devp->cdev);
	kfree(cdevdemo_devp);
	unregister_chrdev_region(MKDEV(cdevdemo_major, 0), 1);
}

module_init(cdevdemo_init);
module_exit(cdevdemo_exit);

module_param(cdevdemo_major, int, S_IRUGO);
MODULE_AUTHOR("Li Liu <liliu@rdamicro.com>");
MODULE_DESCRIPTION("Cdev Demo Driver");
MODULE_LICENSE("GPL");

