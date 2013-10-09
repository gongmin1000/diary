#include <linux/version.h>      /*LINUX_VERSION_CODE*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
//#include <linux/device.h>
//#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/atomic.h>
#include <linux/usb.h>

#include <linux/errno.h>

/* Define these values to match your devices */
#define USB_MY_YF23_VENDOR_ID	0xfff0
#define USB_MY_YF23_PRODUCT_ID	0xfff0

/* table of devices that work with this driver */
static struct usb_device_id MyYF23_table [] = {
	{ USB_DEVICE(USB_MY_YF23_VENDOR_ID, USB_MY_YF23_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, MyYF23_table);

#define MY_YF23_MAJOR 0   /* dynamic major by default */
#define MY_YF23_DEVS  1024    /* my_yf23_0 through my_yf23_1024 */

static char driver_name[] = "My_YF23"; 
static int MyYF23_major =   MY_YF23_MAJOR;
static int MyYF23_devs =    MY_YF23_DEVS;    /* number of bare scullc devices */

static struct class*        _class = NULL;

/* Get a minor range for your devices from the usb maintainer */
#define USB_MY_YF23_MINOR_BASE	192

struct MyYF23Dev{
	int No;                                         /*设备编号*/
	struct usb_device *	udev;			/* the usb device for this device */
	struct usb_interface *	interface;
};

static int MyYF23_open(struct inode *inode, struct file *file)
{
	return 0;
}


static int MyYF23_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t MyYF23_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t MyYF23_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	return 0;
}

char* MyYF23_devnode(struct device *dev, umode_t *mode)
{
	return NULL;
}

static struct file_operations MyYF23_fops = {
	.owner =	THIS_MODULE,
	.read =		MyYF23_read,
	.write =	MyYF23_write,
	.open =		MyYF23_open,
	.release =	MyYF23_release,
};

/* 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core
 */
static struct usb_class_driver MyYF23_class = {
	.name = "usb/MyYF23%d",
	.fops = &MyYF23_fops,
	.devnode = MyYF23_devnode,
	.minor_base = USB_MY_YF23_MINOR_BASE,
};



static int MyYF23_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int i,ret;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	
	/* allocate memory for our device state and initialize it */
	struct MyYF23Dev* dev = kmalloc(sizeof(struct MyYF23Dev), GFP_KERNEL);
	if (dev == NULL) {
		printk("Out of memory\n");
		return -ENODEV;
	}
	memset(dev, 0x00, sizeof (struct MyYF23Dev));

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;
		
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);
	

	ret = usb_register_dev(interface,&MyYF23_class);
	if( ret ){
		printk("Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto usb_register_dev_err;
	}
	printk("MyYF23 init ok\n");
	return 0;
usb_register_dev_err:
	kfree(dev);
	return ret;
}

static void MyYF23_disconnect(struct usb_interface *interface)
{
	struct MyYF23Dev* dev = NULL;
	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface,NULL);
	if( dev != NULL ){
		kfree(dev);
	}
}


static struct usb_driver MyYF23_driver = {
	.name = "MyYF23",
	.id_table = MyYF23_table,
	.probe = MyYF23_probe,
	.disconnect = MyYF23_disconnect,
};



static int __init MyYF23Driver_init(void)
{
	///////////////////////////////////////
	int result = 0;
	dev_t dev_id = MKDEV(MyYF23_major, 0);
	printk(KERN_NOTICE"Hello MyYF23 \n");
	/*
	* Register your major, and accept a dynamic number.
	* 分配设备号
	*/
	if (MyYF23_major)
		result = register_chrdev_region(dev_id, MyYF23_devs, "asi");
	else {
		result = alloc_chrdev_region(&dev_id, 0, MyYF23_devs, "asi");
		MyYF23_major = MAJOR(dev_id);
	}
	if (result < 0)
		return result;
	//建立/sysfs文件系统下相关文件
	_class = class_create(THIS_MODULE,driver_name);
	if( _class == NULL ){
		printk(KERN_WARNING"class_create error\n");
		unregister_chrdev_region(MKDEV(MyYF23_major, 0), MyYF23_devs);
		return 0;
	}



	/* register this driver with the USB subsystem */
	result = usb_register(&MyYF23_driver);
	if (result)
		printk("usb_register failed. Error number %d", result);

	return result;
}


static void __exit MyYF23Driver_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&MyYF23_driver);

	class_destroy(_class);

	unregister_chrdev_region(MKDEV(MyYF23_major, 0), MyYF23_devs);
}

module_init(MyYF23Driver_init);
module_exit(MyYF23Driver_exit);


MODULE_AUTHOR("DONGRUI");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Driver for the MyYF23 demo");

