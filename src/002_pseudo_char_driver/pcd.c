#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DEV_MEM_SIZE 512

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "%s: " fmt, __func__

/* pseudo deivce's memory */
char device_buffer[DEV_MEM_SIZE];

/* this holds the device number */
dev_t device_number;

/* Cdev variable */
struct cdev pcd_cdev;

/* File operations declaration */
loff_t pcd_lseek(struct file *filp, loff_t offet, int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count,
                 loff_t *f_ops);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count,
                  loff_t *f_ops);
int pcd_open(struct inode *inode, struct file *filp);
int pcd_release(struct inode *inode, struct file *filp);

/* File operations definition */
loff_t pcd_lseek(struct file *filp, loff_t offset, int whence) {
    loff_t temp;

    pr_info("lseek requested \n");
    pr_info("Current value of the file position = %lld\n", filp->f_pos);

    switch (whence) {
        case SEEK_SET:
            if ((offset > DEV_MEM_SIZE) || (offset < 0)) {
                return -EINVAL;
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + offset;
            if ((temp > DEV_MEM_SIZE) || (temp < 0)) {
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = DEV_MEM_SIZE + offset;
            if ((temp > DEV_MEM_SIZE) || (temp < 0)) {
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        default:
            return -EINVAL;
            break;
    }

    pr_info("New value of the file position = %lld\n", filp->f_pos);
    return filp->f_pos;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count,
                 loff_t *f_pos) {
    pr_info("read requested for %zu bytes\n", count);
    pr_info("Current file position = %lld\n", *f_pos);

    /* Adjust the 'count' */
    if ((*f_pos + count) > DEV_MEM_SIZE) {
        count = DEV_MEM_SIZE - *f_pos;
    }

    /* Copy to user */
    if (copy_to_user(buff, &device_buffer[*f_pos], count)) {
        return -EFAULT;
    }

    /* Update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully read = %zu\n", count);
    pr_info("Updated file position = %lld", *f_pos);

    /* Return number of bytes which have been successfully read */
    return count;
}

ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count,
                  loff_t *f_pos) {
    pr_info("write requested for %zu bytes\n", count);
    pr_info("Current file position = %lld\n", *f_pos);

    /* Adjust the 'count' */
    if ((*f_pos + count) > DEV_MEM_SIZE) {
        count = DEV_MEM_SIZE - *f_pos;
    }

    if (!count) {
        return -ENOMEM;
    }

    /* Copy to user */
    if (copy_from_user(&device_buffer[*f_pos], buff, count)) {
        return -EFAULT;
    }

    /* Update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully written = %zu\n", count);
    pr_info("Updated file position = %lld", *f_pos);

    /* Return number of bytes which have been successfully written */
    return count;
}

int pcd_open(struct inode *inode, struct file *filp) {
    pr_info("open was successful\n");
    return 0;
}

int pcd_release(struct inode *inode, struct file *filp) {
    pr_info("release was successful\n");
    return 0;
}

/* file operations of the driver */
struct file_operations pcd_fops = {.open = pcd_open,
                                   .write = pcd_write,
                                   .read = pcd_read,
                                   .llseek = pcd_lseek,
                                   .release = pcd_release,
                                   .owner = THIS_MODULE};

struct class *class_pcd;

struct device *device_pcd;

static int __init pcd_driver_init(void) {
    int ret;
    /* 1. Dynamically allocate a device number */
    ret = alloc_chrdev_region(&device_number, 0, 1, "pcd_devices");
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        return ret;
    }

    pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(device_number),
            MINOR(device_number));

    /* 2. Initialize the cdev structure */
    cdev_init(&pcd_cdev, &pcd_fops);
    pcd_cdev.owner = THIS_MODULE;

    /* 3. Register a device (cdev structure) with VFS */
    ret = cdev_add(&pcd_cdev, device_number, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto out_unreg;
    }

    /* Create device class under /sys/class/ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    class_pcd = class_create("pcd_class");
#else
    class_pcd = class_create(THIS_MODULE, "pcd_class");
#endif
    if (IS_ERR(class_pcd)) {
        ret = PTR_ERR(class_pcd);
        pr_err("Failed to create class\n");
        goto out_cdev;
    }

    /* Populate the sysfs with device information */
    device_pcd = device_create(class_pcd, NULL, device_number, NULL, "pcd");
    if (IS_ERR(device_pcd)) {
        ret = PTR_ERR(device_pcd);
        pr_err("Failed to create device\n");
        goto out_class;
    }

    pr_info("Module init successfully\n");

    return 0;

out_class:
    class_destroy(class_pcd);
out_cdev:
    cdev_del(&pcd_cdev);
out_unreg:
    unregister_chrdev_region(device_number, 1);
    return ret;
}

static void __exit pcd_driver_cleanup(void) {
    device_destroy(class_pcd, device_number);
    class_destroy(class_pcd);
    cdev_del(&pcd_cdev);
    unregister_chrdev_region(device_number, 1);
    pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sang Tan Truong");
MODULE_DESCRIPTION("This is my pseudo driver");

