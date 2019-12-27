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

#include <linux/io.h> //iowrite ioread
#include <linux/slab.h>//kmalloc kfree
#include <linux/platform_device.h>//platform driver
#include <linux/of.h>//of match table
#include <linux/ioport.h>//ioremap

#define BUFF_SIZE 20
#define DRIVER_NAME "led"
MODULE_LICENSE("Dual BSD/GPL");

struct led_info {
  unsigned long mem_start;
  unsigned long mem_end;
  void __iomem *base_addr;
};

dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static struct cdev *my_cdev;
static struct led_info *lp = NULL;

int endRead = 0;


static int led_probe(struct platform_device *pdev);
static int led_remove(struct platform_device *pdev);
int led_open(struct inode *pinode, struct file *pfile);
int led_close(struct inode *pinode, struct file *pfile);
ssize_t led_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset);
ssize_t led_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset);
static int __init led_init(void);
static void __exit led_exit(void);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_close,
};

static struct of_device_id led_of_match[] = {
  { .compatible = "led_gpio", },
  { /* end of list */ },
};

static struct platform_driver led_driver = {
  .driver = {
    .name = DRIVER_NAME,
    .owner = THIS_MODULE,
    .of_match_table	= led_of_match,
  },
  .probe		= led_probe,
  .remove		= led_remove,
};


MODULE_DEVICE_TABLE(of, led_of_match);

static int led_probe(struct platform_device *pdev)
{
  struct resource *r_mem;
  int rc = 0;
  r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!r_mem) {
    printk(KERN_ALERT "Failed to get resource\n");
    return -ENODEV;
  }
  lp = (struct led_info *) kmalloc(sizeof(struct led_info), GFP_KERNEL);
  if (!lp) {
    printk(KERN_ALERT "Could not allocate led device\n");
    return -ENOMEM;
  }

  lp->mem_start = r_mem->start;
  lp->mem_end = r_mem->end;
  //printk(KERN_INFO "Start address:%x \t end address:%x", r_mem->start, r_mem->end);

  if (!request_mem_region(lp->mem_start,lp->mem_end - lp->mem_start + 1,	DRIVER_NAME))
  {
    printk(KERN_ALERT "Could not lock memory region at %p\n",(void *)lp->mem_start);
    rc = -EBUSY;
    goto error1;
  }

  lp->base_addr = ioremap(lp->mem_start, lp->mem_end - lp->mem_start + 1);
  if (!lp->base_addr) {
    printk(KERN_ALERT "Could not allocate memory\n");
    rc = -EIO;
    goto error2;
  }

  printk(KERN_WARNING "led platform driver registered\n");
  return 0;//ALL OK

error2:
  release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
error1:
  return rc;
}

static int led_remove(struct platform_device *pdev)
{
  printk(KERN_WARNING "led platform driver removed\n");
  iowrite32(0, lp->base_addr);
  iounmap(lp->base_addr);
  release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
  kfree(lp);
  return 0;
}



int led_open(struct inode *pinode, struct file *pfile) 
{
		//printk(KERN_INFO "Succesfully opened led\n");
		return 0;
}

int led_close(struct inode *pinode, struct file *pfile) 
{
		//printk(KERN_INFO "Succesfully closed led\n");
		return 0;
}

ssize_t led_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) 
{
	int ret;
	int len = 0;
	u32 led_val = 0;
	int i = 0;
	char buff[BUFF_SIZE];
	if (endRead){
		endRead = 0;
		return 0;
	}

	led_val = ioread32(lp->base_addr);

	//buffer: 0b????
	//index:  012345

	buff[0]= '0';
	buff[1]= 'b';
	for(i=0;i<4;i++)
	{
		if((led_val >> i) & 0x01)
			buff[5-i] = '1';
		else
			buff[5-i] = '0';
	}
	buff[6]= '\n';
	len=7;
	ret = copy_to_user(buffer, buff, len);
	if(ret)
		return -EFAULT;
	//printk(KERN_INFO "Succesfully read\n");
	endRead = 1;

	return len;
}

ssize_t led_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) 
{
	char buff[BUFF_SIZE];
	int ret = 0;
	long int led_val=0;

	ret = copy_from_user(buff, buffer, length);
	if(ret)
		return -EFAULT;
	buff[length] = '\0';

	// HEX  INPUT
	if(buff[0] == '0' && (buff[1] == 'x' || buff[1] == 'X')) 
	{
		ret = kstrtol(buff+2,16,&led_val);
	}
	// BINARY INPUT
	else if(buff[0] == '0'  && (buff[1] == 'b' || buff[1] == 'B')) 
	{
		ret = kstrtol(buff+2,2,&led_val);
	}
	// DECIMAL INPUT
	else 
	{
		ret = kstrtol(buff,10,&led_val);
	}

	
	if (!ret)
	{
		iowrite32((u32)led_val, lp->base_addr);
		//printk(KERN_INFO "Succesfully wrote value %#x",(u32)led_val); 
	}
	else
	{
		printk(KERN_INFO "Wrong command format\n"); 
	}

	return length;
}

static int __init led_init(void)
{
   int ret = 0;

   ret = alloc_chrdev_region(&my_dev_id, 0, 1, DRIVER_NAME);
   if (ret){
      printk(KERN_ERR "failed to register char device\n");
      return ret;
   }
   printk(KERN_INFO "char device region allocated\n");

   my_class = class_create(THIS_MODULE, "led_class");
   if (my_class == NULL){
      printk(KERN_ERR "failed to create class\n");
      goto fail_0;
   }
   printk(KERN_INFO "class created\n");
   
   my_device = device_create(my_class, NULL, my_dev_id, NULL, DRIVER_NAME);
   if (my_device == NULL){
      printk(KERN_ERR "failed to create device\n");
      goto fail_1;
   }
   printk(KERN_INFO "device created\n");

	my_cdev = cdev_alloc();	
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, my_dev_id, 1);
	if (ret)
	{
      printk(KERN_ERR "failed to add cdev\n");
		goto fail_2;
	}
   printk(KERN_INFO "cdev added\n");
   printk(KERN_INFO "Hello world\n");

  return platform_driver_register(&led_driver);

   fail_2:
      device_destroy(my_class, my_dev_id);
   fail_1:
      class_destroy(my_class);
   fail_0:
      unregister_chrdev_region(my_dev_id, 1);
   return -1;
}

static void __exit led_exit(void)
{
   platform_driver_unregister(&led_driver);
   cdev_del(my_cdev);
   device_destroy(my_class, my_dev_id);
   class_destroy(my_class);
   unregister_chrdev_region(my_dev_id,1);
   printk(KERN_INFO "Goodbye, cruel world\n");
}


module_init(led_init);
module_exit(led_exit);
