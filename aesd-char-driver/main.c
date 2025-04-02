/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include "aesd_ioctl.h"
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Nalin Saxena"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t offset = 0;
    size_t rem_bytes = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    struct aesd_dev *dev = filp->private_data;
    /**
     * TODO: handle read
     */

    // perform input validation
    if (filp == NULL || buf == NULL || count < 0 || f_pos == NULL || *f_pos < 0)
    {
        PDEBUG("BAD INPUT ARGS!");
        return EINVAL;
    }
    // try to acquire mutex lock critical section
    if (mutex_lock_interruptible(&dev->buffer_mutex))
    {
        retval = -ERESTARTSYS;
        PDEBUG("FAILED TO ACQUIRE MUTEX!");
        goto unlock_mutex;
    }
    struct aesd_buffer_entry *search_val;
    search_val = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &offset);
    if (search_val == NULL)
    {
        PDEBUG("FFAILED TO SEARCH!");
        retval = 0;
        goto unlock_mutex;
    }
    rem_bytes = search_val->size - offset;
    if (rem_bytes > count)
    {
        rem_bytes = count;
    }
    // copy from kernel space to user space this call wil return how many bytes werent written
    int fail_copied_bytes = copy_to_user(buf, search_val->buffptr + offset, rem_bytes);
    // partial copy
    if (fail_copied_bytes)
    {
        retval = -EFAULT;
        PDEBUG("%d bytes were not copied \n", fail_copied_bytes);
        goto unlock_mutex;
    }
    *f_pos += rem_bytes;
    retval = rem_bytes;
unlock_mutex:
    mutex_unlock(&dev->buffer_mutex);
    PDEBUG("Mutex unlocked \n");
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    char *usr_space_ptr;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */
    // perform input validation
    if (filp == NULL || buf == NULL || count < 0 || f_pos == NULL || *f_pos < 0)
    {
        PDEBUG("BAD INPUT ARGS! \n");
        return -EINVAL;
    }
    if (dev == NULL)
    {
        PDEBUG("INVALID DEVICE! \n");
        return -ENODEV;
    }
    // get dynamic memory
    usr_space_ptr = (char *)kmalloc(count, GFP_KERNEL);
    if (usr_space_ptr == NULL)
    {
        PDEBUG("OUT OF MEMORY \n");
        retval = -ENOMEM;
        goto mem_free;
    }
    retval = copy_from_user(usr_space_ptr, buf, count);
    if (retval)
    {
        PDEBUG("unable to copy from user space\n");
        retval = -ENOMEM; // check error code
        goto mem_free;
    }
    // search for new line
    char *new_line_pos = memchr(usr_space_ptr, '\n', count);
    size_t num_copy = count;
    if (new_line_pos != NULL)
    {
        num_copy = new_line_pos - usr_space_ptr + 1; // 1 for null char;
    }
    // critical section
    if (mutex_lock_interruptible(&dev->buffer_mutex))
    {
        retval = -ERESTARTSYS;
        PDEBUG("FAILED TO ACQUIRE MUTEX!");
        goto mem_free;
    }
    // call realloc
    char *new_ptr = (char *)krealloc(dev->single_data_write.buffptr, dev->single_data_write.size + num_copy, GFP_KERNEL);
    if (new_ptr == NULL)
    {
        PDEBUG("OUT OF MEMORY \n");
        kfree(dev->single_data_write.buffptr);
        retval = -ENOMEM;
        goto unlock_mutex;
    }
    // assign bufferptr to new ptr
    dev->single_data_write.buffptr = new_ptr;
    char *offset = dev->single_data_write.buffptr + dev->single_data_write.size;
    // append and accumulate write commands and check for new line later
    memcpy(offset, usr_space_ptr, num_copy);
    dev->single_data_write.size += num_copy;
    retval = num_copy;
    // if we had encountered new line we will process similar to aesd socket logic
    if (new_line_pos != NULL)
    {
        char *temp_ptr = aesd_circular_buffer_add_entry(&dev->buffer, &dev->single_data_write);
        if (temp_ptr != NULL)
        {
            PDEBUG("freeing ptr returned by aesd_circular_buffer_add_entry\n");
            kfree(temp_ptr);
        }
        // clear the temporary buffer instance and expect another write command
        dev->single_data_write.buffptr = NULL;
        dev->single_data_write.size = 0;
    }
unlock_mutex:
    mutex_unlock(&dev->buffer_mutex);
mem_free:
    if (usr_space_ptr != NULL)
    {
        kfree(usr_space_ptr);
    }
    return retval;
}
// code refrenced from  LDD chapter 6
loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t newpos;
    struct aesd_dev *dev = filp->private_data;
    int total_bytes_size = 0, index = 0;  // Moved variable declarations

    switch (whence)
    {
    case SEEK_SET: /* SEEK_SET */
        newpos = off;
        break;

    case SEEK_CUR: /* SEEK_CUR */
        newpos = filp->f_pos + off;
        break;

    case SEEK_END: /* SEEK_END */
        if (mutex_lock_interruptible(&dev->buffer_mutex))
        {
            PDEBUG("FAILED TO ACQUIRE MUTEX!");
            return -ERESTARTSYS;
        }

        struct aesd_buffer_entry *entry;
        AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index)
        {
            total_bytes_size += entry->size;
        }
        mutex_unlock(&dev->buffer_mutex);

        newpos = total_bytes_size + off;

        // Prevent seeking beyond valid range
        if (newpos < 0)
            return -EINVAL;

        break;

    default: /* can't happen */
        return -EINVAL;
    }

    if (newpos < 0)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    printk("in ioctl\n");
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seek_args;
    int ret_val = 0;
    if (cmd != AESDCHAR_IOCSEEKTO)
    {
        PDEBUG("INVALID COMMAND \n");
        ret_val = -ENOTTY;
        goto err_exit;
    }

    // need to Copy args from user to kernel space
    if (copy_from_user(&seek_args, (struct aesd_seekto*) arg, sizeof(struct aesd_seekto)))
    {
        PDEBUG("unable to copy from user space\n");
        ret_val = -EFAULT;
        goto err_exit;
    }
    int command_num = seek_args.write_cmd;
    command_num = (command_num + dev->buffer.out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (command_num > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        PDEBUG("command out of bounds\n");
        ret_val = -EINVAL;
        goto err_exit;
    }

    // also check if offset bigger than command size ?
    if (seek_args.write_cmd_offset > dev->buffer.entry[command_num].size)
    {
        PDEBUG("offset value out of bounds\n");
        ret_val = -EINVAL;
        goto err_exit;
    }

    if (mutex_lock_interruptible(&dev->buffer_mutex))
    {
        ret_val = -ERESTARTSYS;
        PDEBUG("FAILED TO ACQUIRE MUTEX!");
       goto err_exit;
    }
    // we need to traveser to the command
    int itr = dev->buffer.out_offs;
    int byte_count = 0;
    while (itr != command_num)
    {
        byte_count += dev->buffer.entry[itr].size;
        itr = (itr + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    printk("ioctl  %d  %d  %d\n",byte_count + seek_args.write_cmd_offset,command_num,seek_args.write_cmd_offset);
    filp->f_pos = byte_count + seek_args.write_cmd_offset;
    PDEBUG("seeked file pointer to %d\n", filp->f_pos);
    mutex_unlock(&dev->buffer_mutex);
err_exit:
    return ret_val;
}


struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.buffer_mutex); // Initialize the mutex for the circular buffer.

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *curr_element;
    int index = 0;
    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    AESD_CIRCULAR_BUFFER_FOREACH(curr_element, &aesd_device.buffer, index)
    {
        if (curr_element->buffptr != NULL)
        {
            kfree(curr_element->buffptr);
        }
    } // destroy mutex
    mutex_destroy(&aesd_device.buffer_mutex);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
