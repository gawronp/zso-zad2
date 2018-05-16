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

#include "doomcode.h"
#include "doomdev.h"
#include "harddoom.h"

MODULE_LICENSE("GPL");

static const char * DRIVER_NAME = "HardDoomDriver";

static struct pci_device_id doomdev_device = PCI_DEVICE(0x0666, 0x1993);
static struct pci_device_id zero_device = {
        .vendor = 0,
        .device = 0,
        .subvendor = 0,
        .subdevice = 0,
        .class = 0,
        .class_mask = 0,
        .driver_data = 0,
};

static int probe(struct pci_dev *dev, const struct pci_device_id *id);
static void remove(struct pci_dev *dev);
static int suspend(struct pci_dev *dev, pm_message_t state);
static int resume(struct pci_dev *dev);
static void shutdown(struct pci_dev *dev);

static struct pci_driver this_driver = {
        .name = DRIVER_NAME,
        .id_table = {
                doomdev_device, zero_device
        },
        .probe = probe,
        .remove = remove,
        .suspend = suspend,
        .resume = resume,
        .shutdown = shutdown,
};

static irqreturn_t interrupt_handler(int irq, void *dev);

static int probe(struct pci_dev *dev, const struct pci_device_id *id) {
    int err;
    void __iomem *iomem_mapped;

    // maybe add drvdata?

    if ((err = pci_enable_device(dev)))
        goto err_enable_device;
    if ((err = pci_request_regions(dev, DRIVER_NAME)))
        goto err_request_regions;
    iomem_mapped = pci_iomap(dev, 0, 0);
    pci_set_drvdata(dev, iomem_mapped);

    if ((err = request_irq(dev->iqr, interrupt_handler. IRQF_SHARED, "DoomDev", /* TODO: is this a good idea?*/ iomem_mapped)))
        goto err_request_irq;

    goto success;

err_request_irq:
    pci_iounmap(dev, iomem_mapped);
    pci_release_regions(dev);
err_request_regions:
    pci_disable_device(dev);
err_enable_device:
    return err;

success:
    return 0;
}

static void remove(struct pci_dev *dev) {
    void __iomem *iomem_mapped = (void __iomem *) pci_get_drvdata(dev);

    pci_iounmap(dev, iomem_mapped);
    pci_release_regions(dev);
    pci_disable_device(dev);
}

static int suspend(struct pci_dev *dev, pm_message_t state) {

}

static int resume(struct pci_dev *dev) {

}

static void shutdown(struct pci_dev *dev) {
    remove(dev);
}

static long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case DOOMDEV_IOCTL_CREATE_SURFACE:
        break;
    case DOOMDEV_IOCTL_CREATE_TEXTURE:
        break;
    case DOOMDEV_IOCTL_CREATE_FLAT:
        break;
    case DOOMDEV_IOCTL_CREATE_COLORMAPS:
        break;
    default:
        return -EINVAL;
    }

    if (cmd != HELLO_IOCTL_SET_REPEATS)
        return -ENOTTY;
    if (arg > HELLO_MAX_REPEATS)
        return -EINVAL;
    hello_repeats = arg;
    return 0;
}

static int hello_open(struct inode *ino, struct file *filep);
static int hello_release(struct inode *ino, struct file *filep);

static struct file_operations hello_fops = {
        .owner = THIS_MODULE,
        .open = hello_open,
        .release = hello_release,
        .unlocked_ioctl = hello_ioctl,
        .compat_ioctl = hello_ioctl,
};

static int hello_open(struct inode *ino, struct file *filep)
{
    return 0;
}

static int hello_release(struct inode *ino, struct file *filep)
{
    return 0;
}

static int hello_init(void)
{
    int err;
    if ((err = pci_register_driver(&this_driver)))
        goto err_pci_driver_register;


err_pci_driver_register:
    return err;
}

static void hello_cleanup(void)
{
    pci_unregister_driver(&this_driver);
}

module_init(hello_init);
module_exit(hello_cleanup);
