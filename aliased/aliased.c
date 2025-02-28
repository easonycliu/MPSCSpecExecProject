#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>

#define CLASS_NAME "redacted"
#define DEVICE_NAME "aliased"

static struct class* device_class = NULL;
static struct device* device = NULL;
static int device_major_number = 0;

static void aliased_vm_open(struct vm_area_struct* vma) {
}

static void aliased_vm_close(struct vm_area_struct* vma) {
}

static vm_fault_t aliased_vm_fault(struct vm_fault* vmf) {
    struct page* aliased_page = vmf->vma->vm_private_data;
    get_page(aliased_page);

    vmf->page = aliased_page;
    return 0;
}

static struct vm_operations_struct aliased_vm_ops = {
    .open = aliased_vm_open,
    .close = aliased_vm_close,
    .fault = aliased_vm_fault,
};

static int aliased_mmap(struct file* filp, struct vm_area_struct* vma) {
    vma->vm_ops = &aliased_vm_ops;
    vma->vm_private_data = filp->private_data;
    aliased_vm_open(vma);
    return 0;
}

static int aliased_open(struct inode* inode, struct file* filp) {
    struct page* aliased_page = alloc_page(GFP_HIGHUSER | __GFP_ZERO);
    if (aliased_page == NULL) {
        pr_notice("Failed to allocate aliased page");
        return -ENOMEM;
    }
    filp->private_data = aliased_page;

    try_module_get(THIS_MODULE);
    return 0;
}

static int aliased_release(struct inode* inode, struct file* filp) {
    struct page* aliased_page = filp->private_data;
    put_page(aliased_page);

    module_put(THIS_MODULE);
    return 0;
}

static struct file_operations aliased_file_ops = {
    .open = aliased_open,
    .release = aliased_release,
    .mmap = aliased_mmap,
};

/*
 * Based on tty_devnode in drivers/tty/tty_io.c and
 * https://stackoverflow.com/questions/18934085/how-can-i-setup-permission-of-linux-char-driver.
 */
static char* osprey_devnode(struct device* dev, umode_t* mode) {
    if (mode != NULL) {
        *mode = S_IRUGO | S_IWUGO;
    }
    return NULL;
}

static int __init init_aliased(void) {
    device_major_number = register_chrdev(0, DEVICE_NAME, &aliased_file_ops);
    if (device_major_number < 0) {
        pr_err("Registering character device failed with %d\n", device_major_number);
        return device_major_number;
    }

    pr_info("Registered character device with major number %d\n", device_major_number);

    device_class = class_create(THIS_MODULE, "osprey");
    if (IS_ERR(device_class)) {
        pr_err("Failed to create device class\n");
        unregister_chrdev(device_major_number, DEVICE_NAME);
        return PTR_ERR(device_class);
    }
    device_class->devnode = osprey_devnode;

    device = device_create(device_class, NULL, MKDEV(device_major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        pr_err("Failed to create device\n");
        class_destroy(device_class);
        unregister_chrdev(device_major_number, DEVICE_NAME);
        return PTR_ERR(device);
    }

    pr_info("Created device /dev/" DEVICE_NAME "\n");

    return 0;
}

static void __exit cleanup_aliased(void) {
    device_destroy(device_class, MKDEV(device_major_number, 0));
    class_destroy(device_class);
    unregister_chrdev(device_major_number, DEVICE_NAME);

    pr_info("Removed device /dev/" DEVICE_NAME "\n");
}

module_init(init_aliased);
module_exit(cleanup_aliased);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Sam Kumar <samkumar@cs.ucla.edu>");
MODULE_DESCRIPTION("Driver that maps virtual pages in a VMA to the same physical page");
