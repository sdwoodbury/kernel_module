/* YOUR FILE-HEADER COMMENT HERE */
/*
STUART WOODBURY
yr45570@umbc.edu
os421 project 2
april 2015

starter code was provided by Jason Tang. view starter code and original assignment at http://www.csee.umbc.edu/~jtang/archives/cs421.s15/homework/proj2.html

*/
/*
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments.  See
 * /usr/src/linux/Documentation/kernel-doc-nano-HOWTO.txt for details.
 */

/*
 * Getting compilation warnings?  The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read through /usr/src/linux/Documentation/CodingStyle to ensure
 * your project compiles without warnings.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <linux/uaccess.h>	/*for copy_to_user and copy_from_user */

#include <linux/spinlock.h>	/*for access control to the read and write functions */
#include <linux/babblebot.h>	/*for babbles */
#include <linux/completion.h>	/*for a cv */
#include <linux/irqreturn.h>

#include <linux/time.h>

#define PREFIX "BABBLER: "

#define get_time_data do_gettimeofday(&other_time); time_to_tm(other_time.tv_sec, 0, &time); \
	hours = time.tm_hour - 4;\
	minutes = time.tm_min;\
	seconds = time.tm_sec;

static char mem_array[BABBLE_MAX_SIZE + 9];	/*the buffer */

static DEFINE_SPINLOCK(lock);

static DECLARE_COMPLETION(cv);

static int cursor;		/*set to buffer length on write and zero afte read */

static struct tm time;

static struct timeval other_time;

static int hours;		/*i'm hoping this will all be cheaper than declaring these locally for two separate functions */
static int seconds;
static int minutes;

static int counter;

/**
 * babbler_open() - callback invoked when a process tries to open this
 * character device
 * @inode: inode of character device (ignored)
 * @filp: process's file object that is opening the device (ignored)
 * Return: 0 if open is permitted, negative on error
 */
static int babbler_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 * babbler_release() - callback invoked when a process closes this 
 * cahracter device
 * @inode: inode of character device (ignored)
 * @filp: process's file object that is closing the device (ignored)
 * Return: 0 on success, negative on error
 */
static int babbler_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 * babbler_read() - callback invoked when a process reads from this
 * character device
 * @filp: process's file object that is reading this device
 * @ubuf: location to write incoming babble
 * @count: number of bytes requested by process
 * @ppos: file offset (ignored)
 *
 * Write to @ubuf the lesser of @count and of the incoming
 * babble.
 *
 * While there are no babbles, if the requestd process is performing
 * a non-blocking read (see @filp), then return -EAGAIN. Otherwise
 * block this process (as interruptible) until a babble is 
 * received. See wait_for_completion_interruptible().
 *
 * The first time any process successfully reads the babble, clear the 
 * babble. Subsequent reads from the same process or from any process 
 * blocks until a new babble arrives.
 * 
 * Return: number of bytes written to @ubuf, or negative on error.
 */
static ssize_t
babbler_read(struct file *filp, char __user * ubuf, size_t count, loff_t * ppos)
{
	spin_lock(&lock);

	if (cursor == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			spin_unlock(&lock);
			return -EAGAIN;
		}

		spin_unlock(&lock);
		while (cursor == 0) {
			wait_for_completion_interruptible(&cv);
		}
		reinit_completion(&cv);
		spin_lock(&lock);
	}

	/*if requested bytes is too many, only return the maximum size of the buffer */
	if (count > cursor) {
		count = (size_t) cursor;
	}

	count += (size_t) 9;

	/*write to user space */
	counter = cursor;
	for (; counter > -1; counter--) {
		mem_array[counter + 9] = mem_array[counter];
	}

	mem_array[8] = ' ';
	mem_array[0] = (hours / 10) + 48;
	mem_array[1] = (hours % 10) + 48;
	mem_array[2] = ':';
	mem_array[3] = (minutes / 10) + 48;
	mem_array[4] = (minutes % 10) + 48;
	mem_array[5] = ':';
	mem_array[6] = (seconds / 10) + 48;
	mem_array[7] = (seconds % 10) + 48;

	if (copy_to_user(ubuf, mem_array, count) < 0) {
		printk(KERN_ERR "error writing data to user space");
		spin_unlock(&lock);
		return -EFAULT;
	}

	cursor = 0;

	spin_unlock(&lock);
	return count;

}

/**
 * babbler_write() - callback invoked when a process writes to this
 * character device
 * @filp: process's file object that is writing this device (ignored)
 * @ubuf: source buffer of bytes to copy into internal buffer
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Read from @ubuf the lesser of @count bytes or BABBLE_MAX_SIZE
 * bytes. Then wake up any readers (via complete() or complete all())
 * that were blocked. 
 * 
 * Return: number of bytes read from @ubuf, or negative on error
--add timestamp here
 */
static ssize_t
babbler_write(struct file *filp, const char __user * ubuf,
	      size_t count, loff_t * ppos)
{
	spin_lock(&lock);

	if (count > BABBLE_MAX_SIZE) {
		count = BABBLE_MAX_SIZE;
	}

	/*write to buffer */
	if (copy_from_user(mem_array, ubuf, count) < 0) {
		printk(KERN_ERR "error reading from user");
		spin_unlock(&lock);
		return -EFAULT;
	}

	/*update cursor */
	cursor = count;

	/*record time */
	get_time_data complete_all(&cv);

	spin_unlock(&lock);

	return count;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = babbler_read,
	.write = babbler_write,
	.open = babbler_open,
	.release = babbler_release,	/*means close */
};

static struct miscdevice my_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "babbler",
	.fops = &fops,
	.mode = 0666,
};

/*bottom half
--add timestamp here*/
static irqreturn_t my_dev_interrupt_threaded_handler(int irq, void *dev_id)
{

	spin_lock(&lock);

	babblebot_read(mem_array, babblebot_size());
	cursor = babblebot_size();

	/*record time */
	get_time_data complete_all(&cv);

	spin_unlock(&lock);

	babblebot_enable();
	return IRQ_HANDLED;
}

/*top half*/
static irqreturn_t my_dev_interrupt_handler(int irq, void *dev_id)
{
	babblebot_disable();
	return IRQ_WAKE_THREAD;
}

/**
 * babbler_init() - entry point into the babbler kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init babbler_init(void)
{

	if (misc_register(&my_dev)) {
		printk(KERN_ERR "unable to register misc device");
		return -ENODEV;
	}
	pr_info(PREFIX "Hello, brave new world!\n");

	if (request_threaded_irq
	    (BABBLE_IRQ, my_dev_interrupt_handler,
	     my_dev_interrupt_threaded_handler, 0, my_dev.name, NULL) < 0) {
		printk(KERN_ERR "unable to register isr");
		return -EBUSY;
	}

	babblebot_enable();

	return 0;

}

/**
 * babbler_exit() - called by kernel to clean up resources
 */
static void __exit babbler_exit(void)
{
	babblebot_disable();

	free_irq(BABBLE_IRQ, NULL);

	misc_deregister(&my_dev);
	pr_info(PREFIX "Goodbye, cruel world!\n");
}

module_init(babbler_init);
module_exit(babbler_exit);

MODULE_DESCRIPTION("CS421 babbler driver");
MODULE_LICENSE("GPL");
