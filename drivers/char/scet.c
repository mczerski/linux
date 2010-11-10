/*
 *  scet.c - Create an input/output  device driver for scet timer
 */

#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>	/* for get_user and put_user */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/byteorder.h>
#include "scet.h"
#define SUCCESS 0
#define DEVICE_NAME "scet_dev"
#define BUF_LEN TIMER_WIDTH	//We never read/write more then 6 bytes of data

 //#define SOFTWARE_TESTING
void scet_release(struct device *dev);
/* 
 * Is the device open right now? Used to prevent
 * concurent access into the same device 
 */
static int Device_Open = 0;

static struct scet_cd *scd;

//0 to SCET_TCTRL will, stop the counter 
//1 the counter will run
//2 value freeze and counter is counting
//3 value freeze but counter running
static void stop_timer(struct scet_cd *s_scet_cd)
{
	s_scet_cd->timer_run = 0;
	INFO("sct STOP run:%d, freeze: %d ctrl: %d \n", s_scet_cd->timer_run,
	     s_scet_cd->timer_freeze,
	     s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));
	scet_write(s_scet_cd, SCET_TCTRL,
		   (s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1)));

}

static void start_timer(struct scet_cd *s_scet_cd)
{
	s_scet_cd->timer_run = 1;
	INFO("sct START run:%d, freeze: %d ctrl: %d \n", s_scet_cd->timer_run,
	     s_scet_cd->timer_freeze,
	     s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));
	scet_write(s_scet_cd, SCET_TCTRL,
		   s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));

}

static void freeze_timer(struct scet_cd *s_scet_cd)
{
	s_scet_cd->timer_freeze = 1;
	INFO("sct FREEZE run:%d, freeze:%d, ctrl: %d \n", s_scet_cd->timer_run,
	     s_scet_cd->timer_freeze,
	     s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));
	scet_write(s_scet_cd, SCET_TCTRL,
		   s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));

}

static void ufreeze_timer(struct scet_cd *s_scet_cd)
{
	s_scet_cd->timer_freeze = 0;
	INFO("sct UFREEZE run:%d, freeze: %d, ctrl: %d \n",
	     s_scet_cd->timer_run, s_scet_cd->timer_freeze,
	     s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));
	scet_write(s_scet_cd, SCET_TCTRL,
		   s_scet_cd->timer_run | (s_scet_cd->timer_freeze << 1));

}

static void reset_timer(struct scet_cd *s_scet_cd)
{
	scet_write(s_scet_cd, SCET_TB0, 0);
	scet_write(s_scet_cd, SCET_TB1, 0);
	scet_write(s_scet_cd, SCET_TB2, 0);
	scet_write(s_scet_cd, SCET_TB3, 0);
	scet_write(s_scet_cd, SCET_TB4, 0);
	scet_write(s_scet_cd, SCET_TB5, 0);
}

static void read_timer(struct scet_cd *s_scet_cd)
{
	int i = 0;

	freeze_timer(s_scet_cd);

	for (i = 0; i < TIMER_WIDTH; i++) {
		s_scet_cd->current_time[i] = scet_read(s_scet_cd, i);
	}

	ufreeze_timer(s_scet_cd);

}

/* 
 * This is called whenever a process attempts to open the device file 
 */
static int device_open(struct inode *inode, struct file *file)
{
	DBG(KERN_INFO "\n SCET device_open(%p)\n", file);
	/* 
	 * We don't want to talk to two processes at the same time 
	 */
	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{

	DBG(KERN_INFO "device_release(%p,%p)\n", inode, file);

	/* 
	 * We're now ready for our next caller 
	 */
	Device_Open--;

	module_put(THIS_MODULE);
	return SUCCESS;
}

static ssize_t device_read(struct file *file,	/* see include/linux/fs.h   */
			   char __user * buffer,	/* buffer to be
							 * filled with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{

	int bytes_read = 0;
	u8 next_out;
	INFO(KERN_INFO "device_read(%p,%p,%d)\n", file, buffer, length);

	freeze_timer(scd);
	while (bytes_read < TIMER_WIDTH || length < 0) {
		//DBG("read: %d\n", bytes_read);       
		next_out = scet_read(scd, bytes_read);
		DBG("next out: %d\n", next_out);
		put_user(next_out, buffer++);
		length--;
		bytes_read++;
	}
	ufreeze_timer(scd);

	INFO("Read %d bytes\n", bytes_read);
	return bytes_read;
}

static ssize_t
device_write(struct file *file,
	     const char __user * buffer, size_t length, loff_t * offset)
{
	int i;
	u8 next_out;

	INFO(KERN_INFO "SCET_write(%p,%p,%d)", file, buffer, length);

	for (i = 0; i < TIMER_WIDTH && i < BUF_LEN; i++) {
		get_user(next_out, buffer + i);
		INFO("Write %d to, i %d \n", next_out, i);
		scet_write(scd, i, next_out);
	}
	INFO("SCET byte writen to timer %d", i);
	return i;
}

/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */

/*
int device_ioctl(struct inode *inode,	/see include/linux/fs.h 
		 struct file *file,	 ditto 
		 unsigned int ioctl_num,	 number and param for ioctl 
		 unsigned long arg) */
long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	int i;
	unsigned char *temp;
	unsigned char ch;

	/* 
	 * Switch according to the ioctl called 
	 */
	switch (cmd) {

		INFO("cmd %u \n", cmd);

	case IOCTL_SET_SCET_TIME:
		temp = (unsigned char *)arg;

		/* 
		 * Find the length of the message 
		 */
		get_user(ch, temp);
		for (i = 0; ch && i < BUF_LEN; i++, temp++)
			get_user(ch, temp);

		device_write(file, (unsigned char *)arg, i, 0);
		break;

	case IOCTL_GET_SCET_TIME:

		i = device_read(file, (unsigned char *)arg, 6, 0);
		break;

	case IOCTL_SET_SCET_OPERATION:

		INFO("Set SCET timer mode to %u \n", (unsigned int)arg);
		if (arg == 0)
			stop_timer(scd);
		else
			start_timer(scd);

		/* 
		 * This ioctl is both input (ioctl_param) and 
		 * output (the return value of this function) 
		 */
		return (scd->timer_run | (scd->timer_freeze << 1));
		break;

	case IOCTL_GET_SCET_STATUS:

		put_user((scd->timer_run | (scd->timer_freeze << 1)),
			 (int *)arg);
		break;
	}

	return SUCCESS;
}

static int __devinit scet_probe(struct platform_device *pdev)
{

	int retval;
	struct resource *addr;
	void __iomem *addr_reg;
	u8 ioaddr = 0;

	scd = kzalloc(sizeof(struct scet_cd), GFP_KERNEL);
	DBG("driver %s, starting scet_probe %s \n", DEVICE_NAME, pdev->name);

	addr = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!addr)
		DBG("-ENODEV \n");
	DBG("driver %s, ioremap addr->start = 0x%lX\n", DEVICE_NAME,
	    (unsigned long)(addr->start));

	addr =
	    request_mem_region(addr->start, addr->end - addr->start,
			       DEVICE_NAME);

	DBG("hejda\n");

	retval = -EBUSY;
	if (!addr) {
		addr = platform_get_resource(pdev, IORESOURCE_IO, 0);
		if (!addr)
			return -ENODEV;
		ioaddr = 1;
		DBG("adriver %s, setting addr_reg to addr->start == 0x%hX\n",
		    DEVICE_NAME, addr->start);
		addr_reg = (void __iomem *)(unsigned long)addr->start;
	} else {
		addr_reg =
		    ioremap_nocache(addr->start, addr->end - addr->start + 1);
		DBG("driver %s, setting addr_reg to ioremap \n (addr->start = 0x%lX), end  == : 0x%lX , addr_reg : 0x%lX\n", DEVICE_NAME, (unsigned long)(addr->start), (unsigned long)(addr->end), (unsigned long)addr_reg);
		if (addr_reg == NULL) {
			retval = -ENOMEM;
			goto err2;
		}
	}
	scd->addr_reg = addr_reg;
	scd->timer_freeze = 0;
	scd->timer_run = 0;

	stop_timer(scd);
	reset_timer(scd);
	//start_timer(scd);        
	// read_timer(scd);

	return 0;

err2:
	kfree(scd);
	DBG("init error, %d\n", retval);
	return retval;

}

static int __devexit scet_probe_remove(struct platform_device *dev)
{
	DBG("SCET removed");
	kfree(scd);
	return 0;
}

static struct of_device_id scet_match[] = {
	{
	 .compatible = "opencores,scet",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, scet_match);
MODULE_ALIAS("platform:" DEVICE_NAME);

static struct platform_driver scet_driver = {
	.probe = scet_probe,
	.remove = __devexit_p(scet_probe_remove),

	.driver = {
		   .name = DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = scet_match,
		   },
};

void scet_release(struct device *dev)
{
	/*
	   This function is needed because otherwise the
	   platform_device_unregister() call fails. Should we be actually
	   doing something here?
	 */
}

static int __init scet_init(void)
{
	init_module();

	//platform_device_register(&scet_driver); 

	DBG("driver init SCET \n");
	return platform_driver_register(&scet_driver);
}

module_init(scet_init);

static void __exit scet_cleanup(void)
{
	platform_driver_unregister(&scet_driver);
	//platform_device_unregister(&scet_device);
}

module_exit(scet_cleanup);

static const struct file_operations Fops = {
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release,	/* a.k.a. close */
};

/* 
 * Initialize the module - Register the unsigned character device 
 */
int init_module()
{
	int ret_val;
	/* 
	 * Register the unsigned character device (atleast try) 
	 */
	ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);
	/* 
	 * Negative values signify an error 
	 */
	if (ret_val < 0) {
		DBG(KERN_ALERT "%s failed with %d\n",
		    "Sorry, registering the unsigned character device ",
		    ret_val);
		return ret_val;
	}

	DBG(KERN_INFO "%s The major device number is %d.\n",
	    "Registeration is a success", MAJOR_NUM);
	DBG(KERN_INFO "mknod %s c %d 0\n", DEVICE_NAME, MAJOR_NUM);

	return 0;
}

/* 
 * Cleanup - unregister the appropriate file from /proc 
 */
void cleanup_module()
{
	int reta = 0;

	/* 
	 * Unregister the device 
	 */
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

	/* 
	 * If there's an error, report it 
	 */
	if (reta < 0)
		DBG(KERN_ALERT "Error: unregister_chrdev: %d\n", reta);
}
