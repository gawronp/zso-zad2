#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock_types.h>

#include "doomcode.h"
#include "doomdev.h"
#include "doom_common.h"
#include "doom_resources.h"
#include "harddoom.h"

#define DOOM_MAX_DEV_COUNT 256
#define DRIVER_NAME "HardDoomDriver"

MODULE_LICENSE("GPL");

static const struct pci_device_id ids_table[2] = {
        {PCI_DEVICE(HARDDOOM_VENDOR_ID, HARDDOOM_DEVICE_ID)}
};

static int doom_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void doom_remove(struct pci_dev *dev);

//static const struct pci_device_id ids_table = { doomdev_device, zero_device };
static struct pci_driver this_driver = {
        .name = DRIVER_NAME,
        .id_table = ids_table,
        .probe = doom_probe,
        .remove = doom_remove,
};

static irqreturn_t interrupt_handler(int irq, void *dev);

static DEFINE_IDR(doom_idr);
static DEFINE_SPINLOCK(idr_lock);

static struct class doom_class = {
    .name = "doom",
    .owner = THIS_MODULE,
};

static dev_t doom_major;

static long doom_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int doom_open(struct inode *ino, struct file *filep);
static int doom_release(struct inode *ino, struct file *filep);

static struct file_operations doom_fops = {
    .owner = THIS_MODULE,
    .open = doom_open,
    .release = doom_release,
    .unlocked_ioctl = doom_ioctl,
    .compat_ioctl = doom_ioctl,
};

static int init_doom_device(struct doom_device *doomdev)
{
    int doomcode_idx;
    int doomcode_len = sizeof(doomcode) / sizeof(uint32_t);

    iowrite32(0, doomdev->bar0 + HARDDOOM_FE_CODE_ADDR);
    for (doomcode_idx = 0; doomcode_idx < doomcode_len; doomcode_idx++) {
        iowrite32(doomcode[doomcode_idx], doomdev->bar0 + HARDDOOM_FE_CODE_WINDOW);
    }
    iowrite32(0xffffffe, doomdev->bar0 + HARDDOOM_RESET);
    // maybe: zainicjować CMD_*_PTR, jeśli chcemy użyć bloku wczytywania pleceń,
    iowrite32(0x3ff, doomdev->bar0 + HARDDOOM_INTR);
//    iowrite32(0x3ff, doomdev->bar0 + HARDDOOM_INTR_ENABLE); // maybe change that!!!
    iowrite32(0x3ff ^ HARDDOOM_INTR_PONG_ASYNC, doomdev->bar0 + HARDDOOM_INTR_ENABLE);
    // maybe: zainicjować FENCE_*, jeśli czujemy taką potrzebę,
    iowrite32(0x3fe, doomdev->bar0 + HARDDOOM_ENABLE); // włączyć wszystkie bloki urządzenia w ENABLE (być może z wyjątkiem FETCH_CMD)
    (void) ioread32(doomdev->bar0 + HARDDOOM_FIFO_FREE);

    return 0;
}

static void cleanup_doom_device(struct doom_device *doomdev) {
    iowrite32(0, doomdev->bar0 + HARDDOOM_ENABLE);
    iowrite32(0, doomdev->bar0 + HARDDOOM_INTR_ENABLE);
    (void) ioread32(doomdev->bar0 + HARDDOOM_FIFO_FREE);
}

static int doom_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int err;
    struct doom_device *doomdev;

    doomdev = kmalloc(sizeof(struct doom_device), GFP_KERNEL);
    // TODO: check pointer

    // maybe add drvdata?
    spin_lock(&idr_lock);
    doomdev->minor = idr_alloc(&doom_idr, doomdev, 0, DOOM_MAX_DEV_COUNT, GFP_KERNEL);
    spin_unlock(&idr_lock);

    doomdev->pdev = dev;
    mutex_init(&doomdev->surface_lock);
    doomdev->mmio_lock = __SPIN_LOCK_UNLOCKED(doomdev->mmio_lock);
    doomdev->fifo_ping_remaining = PING_ASYNC_MMIO_COMMANDS_SPAN;
    init_waitqueue_head(&doomdev->pong_async_wait);

    cdev_init(&doomdev->cdev, &doom_fops);

    if ((err = cdev_add(&doomdev->cdev, doom_major + doomdev->minor, 1)))
        goto err_cdev_add;

    doomdev->dev = device_create(
            &doom_class, NULL, doom_major + doomdev->minor, NULL, "doom%d", doomdev->minor);

//    doomdev->dma_pool = dma_pool_create(DEVNAME, dev, DOOMDEV_PAGE_SIZE, DOOMDEV_PAGE_SIZE, 0);

    if ((err = pci_enable_device(dev)))
        goto err_enable_device;
    if ((err = pci_request_regions(dev, DRIVER_NAME)))
        goto err_request_regions;
    doomdev->bar0 = pci_iomap(dev, 0, 0);

    pci_set_master(dev);
    if ((err = pci_set_dma_mask(dev, DMA_BIT_MASK(32))))
        goto err_set_master;
    if ((err = pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32))))
        goto err_set_master;

    pci_set_drvdata(dev, doomdev);

    if ((err = request_irq(dev->irq, interrupt_handler, IRQF_SHARED, DEVNAME, doomdev)))
        goto err_request_irq;

    init_doom_device(doomdev);

    return 0;

err_request_irq:

err_set_master:
    pci_clear_master(dev);
    pci_iounmap(dev, doomdev->bar0);

    pci_release_regions(dev);
err_request_regions:
    pci_disable_device(dev);
err_enable_device:
err_cdev_add:
    return err;
}

static irqreturn_t interrupt_handler(int irq, void *dev)
{
    struct doom_device *doomdev = dev;
    u32 intr;

    intr = ioread32(doomdev->bar0 + HARDDOOM_INTR);
    if (!intr) {
        return IRQ_NONE;
    }
    iowrite32(intr, doomdev->bar0 + HARDDOOM_INTR);

    // TODO: something?
    return IRQ_HANDLED;
}

static void doom_remove(struct pci_dev *pdev)
{
    struct doom_device *dev = pci_get_drvdata(pdev);

    cdev_del(&dev->cdev);
    device_destroy(&doom_class, doom_major + dev->minor);

    spin_lock(&idr_lock);
    idr_remove(&doom_idr, dev->minor);
    spin_unlock(&idr_lock);

    cleanup_doom_device(dev);

    pci_iounmap(pdev, dev->bar0);
    pci_release_regions(dev->pdev);
    pci_disable_device(dev->pdev);

    kfree(dev);
}

static long doom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct doom_context *context = file->private_data;

    switch (cmd) {
    case DOOMDEV_IOCTL_CREATE_SURFACE:
        return create_frame_buffer(context, (struct doomdev_ioctl_create_surface *) arg);
    case DOOMDEV_IOCTL_CREATE_TEXTURE:
        return create_column_texture(context, (struct doomdev_ioctl_create_texture *) arg);
    case DOOMDEV_IOCTL_CREATE_FLAT:
        return create_flat_texture(context, (struct doomdev_ioctl_create_flat *) arg);
    case DOOMDEV_IOCTL_CREATE_COLORMAPS:
        return create_colormaps_array(context, (struct doomdev_ioctl_create_colormaps *) arg);
    default:
        return -EINVAL;
    }
}

static int doom_open(struct inode *ino, struct file *filep)
{
    struct doom_device *dev;
    struct doom_context *context;

    spin_lock(&idr_lock);
    dev = idr_find(&doom_idr, MINOR(ino->i_rdev));
    spin_unlock(&idr_lock);

    if (unlikely(dev == NULL)) {
        // TODO
        return -EIO;
    }

    context = kmalloc(sizeof(struct doom_context), GFP_KERNEL);
    if (unlikely(!context)) {
        pr_err("kmalloc failed\n");
        return -ENOMEM;
    }

//    init_context(context);
    context->dev = dev;
    filep->private_data = context;
    return 0;
}

static int doom_release(struct inode *ino, struct file *filep)
{
    // TODO: ????
    return 0;
}

static int doom_init(void)
{
    int err;

    if ((err = class_register(&doom_class)))
        return err;

    if ((err = alloc_chrdev_region(&doom_major, 0, DOOM_MAX_DEV_COUNT, "doom")))
        goto err_alloc_region;

    if ((err = pci_register_driver(&this_driver)))
        goto err_pci_driver_register;

err_pci_driver_register:
    unregister_chrdev_region(doom_major, DOOM_MAX_DEV_COUNT);
err_alloc_region:
    class_unregister(&doom_class);
    return err;
}

static void doom_cleanup(void)
{
    pci_unregister_driver(&this_driver);
    idr_destroy(&doom_idr);
    unregister_chrdev_region(doom_major, DOOM_MAX_DEV_COUNT);
    class_unregister(&doom_class);
}

module_init(doom_init);
module_exit(doom_cleanup);
