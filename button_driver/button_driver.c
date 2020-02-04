#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/of.h>

#include <linux/io.h> //iowrite ioread
#include <linux/slab.h>//kmalloc kfree
#include <linux/platform_device.h>//platform driver
#include <linux/ioport.h>//ioremap

#define BUFF_SIZE 20
#define BUFF_SIZE 20
#define DRIVER_NAME "button"
MODULE_LICENSE("Dual BSD/GPL");

struct button_info {
  unsigned long mem_start;
  unsigned long mem_end;
  void __iomem *base_addr;
};

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;
static struct button_info *bp = NULL;

int endRead = 0;


static int button_probe(struct platform_device *pdev);
static int button_remove(struct platform_device *pdev);
int button_open(struct inode *pinode, struct file *pfile);
int button_close(struct inode *pinode, struct file *pfile);
ssize_t button_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t button_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);
static int __init button_init(void);
static void __exit button_exit(void);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = button_open,
	.read = button_read,
	.write = button_write,
	.release = button_close,
};

static struct of_device_id button_of_match[] = {
  { .compatible = "button_gpio", },
  { /* end of list */ },
};

static struct platform_driver button_driver = {
  .driver = {
    .name = DRIVER_NAME,
    .owner = THIS_MODULE,
    .of_match_table	= button_of_match,
  },
  .probe		= button_probe,
  .remove		= button_remove,
};


MODULE_DEVICE_TABLE(of, button_of_match);

static int button_probe(struct platform_device *pdev)
{
  struct resource *r_mem;
  int rc = 0;
  r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!r_mem) {
    printk(KERN_ALERT "Faibutton to get resource\n");
    return -ENODEV;
  }
  bp = (struct button_info *) kmalloc(sizeof(struct button_info), GFP_KERNEL);
  if (!bp) {
    printk(KERN_ALERT "Could not allocate button device\n");
    return -ENOMEM;
  }

  bp->mem_start = r_mem->start;
  bp->mem_end = r_mem->end;
  //printk(KERN_INFO "Start address:%x \t end address:%x", r_mem->start, r_mem->end);

  if (!request_mem_region(bp->mem_start,bp->mem_end - bp->mem_start + 1,	DRIVER_NAME))
  {
    printk(KERN_ALERT "Could not lock memory region at %p\n",(void *)bp->mem_start);
    rc = -EBUSY;
    goto error1;
  }

  bp->base_addr = ioremap(bp->mem_start, bp->mem_end - bp->mem_start + 1);
  if (!bp->base_addr) {
    printk(KERN_ALERT "Could not allocate memory\n");
    rc = -EIO;
    goto error2;
  }

  printk(KERN_WARNING "button platform driver registered\n");
  return 0;//ALL OK

error2:
  release_mem_region(bp->mem_start, bp->mem_end - bp->mem_start + 1);
error1:
  return rc;
}

static int button_remove(struct platform_device *pdev)
{
  printk(KERN_WARNING "button platform driver removed\n");
  iowrite32(0, bp->base_addr);
  iounmap(bp->base_addr);
  release_mem_region(bp->mem_start, bp->mem_end - bp->mem_start + 1);
  kfree(bp);
  return 0;
}



int button_open(struct inode *pinode, struct file *pfile) 
{
		//printk(KERN_INFO "Succesfully opened button\n");
		return 0;
}

int button_close(struct inode *pinode, struct file *pfile) 
{
		//printk(KERN_INFO "Succesfully closed button\n");
		return 0;
}

ssize_t button_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret;
	int len = 0;
	u32 button_val = 0;
	int i = 0;
	char buff[BUFF_SIZE];
	if (endRead){
		endRead = 0;
		return 0;
	}

	button_val = ioread32(bp->base_addr);

	//buffer: 0b????
	//index:  012345

	buff[0]= '0';
	buff[1]= 'b';
	for(i=0;i<4;i++)
	{
		if((button_val >> i) & 0x01)
			buff[5-i] = '1';
		else
			buff[5-i] = '0';
	}
	len=6;
	ret = copy_to_user(buffer, buff, len);
	if(ret)
		return -EFAULT;
	//printk(KERN_INFO "Succesfully read\n");
	endRead = 1;

	return len;
}

ssize_t button_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{

	printk(KERN_WARNING "Writing to button driver not allowed\n");
	return length;
}

static int __init button_init(void)
{
   int ret = 0;

	//Initialize array

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, DRIVER_NAME);
   if (ret){
      printk(KERN_ERR "faibutton to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "button_class");
   if (my_class == NULL){
      printk(KERN_ERR "faibutton to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, DRIVER_NAME);
   if (my_device == NULL){
      printk(KERN_ERR "faibutton to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      printk(KERN_ERR "faibutton to add cdev\n");
		goto fail_2;
	}
   printk(KERN_INFO "cdev added\n");
   printk(KERN_INFO "Hello world\n");

  return platform_driver_register(&button_driver);

   fail_2:
      device_destroy(my_class, my_dev_id);
   fail_1:
      class_destroy(my_class);
   fail_0:
      unregister_chrdev_region(my_dev_id, 1);
   return -1;
}

static void __exit button_exit(void)
{
   platform_driver_unregister(&button_driver);
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(button_init);
module_exit(button_exit);
