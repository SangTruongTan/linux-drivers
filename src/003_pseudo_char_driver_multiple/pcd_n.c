#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "%s: " fmt, __func__

#define NO_OF_DEVICES 4

#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 512

#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR 0x11

/* pseudo device's memory */
char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];

/* Device private data structure */
struct pcdev_private_data {
    char *buffer;
    unsigned size;
    const char *serial_number;
    int perm;
    /* Cdev variable */
    struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_private_data {
    int total_devices;
    /* this holds the device number */
    dev_t device_number;
    struct class *class_pcd;
    struct device *device_pcd;
    struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

struct pcdrv_private_data pcdrv_data = {
    .total_devices = NO_OF_DEVICES,
    .pcdev_data = {[0] =
                       {
                           .buffer = device_buffer_pcdev1,
                           .size = MEM_SIZE_MAX_PCDEV1,
                           .serial_number = "PCDEV1XYZ123",
                           .perm = RDONLY /* RDONLY */
                       },
                   [1] =
                       {
                           .buffer = device_buffer_pcdev2,
                           .size = MEM_SIZE_MAX_PCDEV2,
                           .serial_number = "PCDEV2XYZ123",
                           .perm = WRONLY /* WRONLY */
                       },
                   [2] =
                       {
                           .buffer = device_buffer_pcdev3,
                           .size = MEM_SIZE_MAX_PCDEV3,
                           .serial_number = "PCDEV3XYZ123",
                           .perm = RDWR /* RDWR */
                       },
                   [3] = {
                       .buffer = device_buffer_pcdev4,
                       .size = MEM_SIZE_MAX_PCDEV4,
                       .serial_number = "PCDEV4XYZ123",
                       .perm = RDWR /* RDWR */
                   }}};

/* File operations declaration */
loff_t pcd_lseek(struct file *filp, loff_t offet, int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count,
                 loff_t *f_ops);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count,
                  loff_t *f_ops);
int pcd_open(struct inode *inode, struct file *filp);
int pcd_release(struct inode *inode, struct file *filp);
int check_permission(int dev_perm, int mode);

/* File operations definition */
loff_t pcd_lseek(struct file *filp, loff_t offset, int whence) {
    struct pcdev_private_data *pcdev_data =
        (struct pcdev_private_data *)filp->private_data;

    int max_size = pcdev_data->size;

    loff_t temp;

    pr_info("lseek requested \n");
    pr_info("Current value of the file position = %lld\n", filp->f_pos);

    switch (whence) {
        case SEEK_SET:
            if ((offset > max_size) || (offset < 0)) {
                return -EINVAL;
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + offset;
            if ((temp > max_size) || (temp < 0)) {
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = max_size + offset;
            if ((temp > max_size) || (temp < 0)) {
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
    struct pcdev_private_data *pcdev_data =
        (struct pcdev_private_data *)filp->private_data;

    int max_size = pcdev_data->size;

    pr_info("read requested for %zu bytes\n", count);
    pr_info("Current file position = %lld\n", *f_pos);

    /* Adjust the 'count' */
    if ((*f_pos + count) > max_size) {
        count = max_size - *f_pos;
    }

    /* Copy to user */
    if (copy_to_user(buff, pcdev_data->buffer + (*f_pos), count)) {
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
    struct pcdev_private_data *pcdev_data =
        (struct pcdev_private_data *)filp->private_data;

    int max_size = pcdev_data->size;

    pr_info("write requested for %zu bytes\n", count);
    pr_info("Current file position = %lld\n", *f_pos);

    /* Adjust the 'count' */
    if ((*f_pos + count) > max_size) {
        count = max_size - *f_pos;
    }

    if (!count) {
        pr_err("No space left on the device");
        return -ENOMEM;
    }

    /* Copy to user */
    if (copy_from_user(pcdev_data->buffer + (*f_pos), buff, count)) {
        return -EFAULT;
    }

    /* Update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully written = %zu\n", count);
    pr_info("Updated file position = %lld", *f_pos);

    /* Return number of bytes which have been successfully written */
    return count;
}

int check_permission(int dev_perm, int acc_mode) {
    if (dev_perm == RDWR) {
        return 0;
    }

    // Ensures readonly access
    if ((dev_perm == RDONLY) &&
        ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE))) {
        return 0;
    }

    // Ensures writeonly access
    if ((dev_perm == WRONLY) &&
        (!(acc_mode & FMODE_READ) && (acc_mode & FMODE_WRITE))) {
        return 0;
    }
    return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filp) {
    int ret;

    int minor_n;

    struct pcdev_private_data *pcdev_data;

    /* Find out on which device file open was attempted by the user space */
    minor_n = MINOR(inode->i_rdev);
    pr_info("minor access = %d\n", minor_n);

    /* Get device's private data structure */
    pcdev_data = container_of(inode->i_cdev, struct pcdev_private_data, cdev);

    /* To supply device private data to other methods of the driver */
    filp->private_data = pcdev_data;

    /* Check permission */
    ret = check_permission(pcdev_data->perm, filp->f_mode);

    if (!ret) {
        pr_info("open was successful\n");
    } else {
        pr_err("open was unsuccessful\n");
    }

    return ret;
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

static int __init pcd_driver_init(void) {
    int ret;
    int i;
    /* 1. Dynamically allocate a device number */
    ret = alloc_chrdev_region(&pcdrv_data.device_number, 0, NO_OF_DEVICES,
                              "pcd_devices");
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        goto out;
    }

    /* Create device class under /sys/class/ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    pcdrv_data.class_pcd = class_create("pcd_class");
#else
    pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
#endif
    if (IS_ERR(pcdrv_data.class_pcd)) {
        ret = PTR_ERR(pcdrv_data.class_pcd);
        pr_err("Failed to create class\n");
        goto unreg_chrdev;
    }

    for (i = 0; i < NO_OF_DEVICES; i++) {
        pr_info("Device number <major>:<minor> = %d:%d\n",
                MAJOR(pcdrv_data.device_number + i),
                MINOR(pcdrv_data.device_number + i));

        /* Initialize the cdev structure */
        cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);
        pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;

        /* Register a device (cdev structure) with VFS */
        ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev,
                       pcdrv_data.device_number + i, 1);
        if (ret < 0) {
            pr_err("Failed to add cdev\n");
            goto cdev_del;
        }

        /* Populate the sysfs with device information */
        pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL,
                                              pcdrv_data.device_number + i,
                                              NULL, "pcdev-%d", i + 1);
        if (IS_ERR(pcdrv_data.device_pcd)) {
            ret = PTR_ERR(pcdrv_data.device_pcd);
            pr_err("Failed to create device\n");
            goto class_del;
        }
    }

    pr_info("Module init successfully\n");

    return 0;

cdev_del:
class_del:
    for (; i >= 0; i--) {
        device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);

unreg_chrdev:
    unregister_chrdev_region(pcdrv_data.device_number, NO_OF_DEVICES);
out:
    pr_info("Module insertion failed\n");
    return ret;
}

static void __exit pcd_driver_cleanup(void) {
    int i;
    for (i = 0; i < NO_OF_DEVICES; i++) {
        device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number + i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);

    unregister_chrdev_region(pcdrv_data.device_number, NO_OF_DEVICES);
    pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sang Tan Truong");
MODULE_DESCRIPTION("This is my pseudo driver which handles n devices");

