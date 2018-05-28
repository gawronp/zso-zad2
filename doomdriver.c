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
MODULE_AUTHOR("Piotr Gawryluk, 346895");
MODULE_DESCRIPTION("Sterownik urzadzenia HardDoom");

static const struct pci_device_id ids_table[2] = {
        {PCI_DEVICE(HARDDOOM_VENDOR_ID, HARDDOOM_DEVICE_ID)}
};

static int doom_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void doom_remove(struct pci_dev *dev);

static struct pci_driver this_driver = {
        .name = DRIVER_NAME,
        .id_table = ids_table,
        .probe = doom_probe,
        .remove = doom_remove,
};

static irqreturn_t interrupt_handler(int irq, void *dev);

static DEFINE_IDR(doom_idr);
static DEFINE_SPINLOCK(idr_lock);

static struct class *doom_class;
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

static void run_init_doom_device_codes(struct doom_device *doomdev)
{
    int doomcode_idx;
    int doomcode_len = sizeof(doomcode) / sizeof(uint32_t);

    // doomcode:
    iowrite32(0, doomdev->bar0 + HARDDOOM_FE_CODE_ADDR);
    for (doomcode_idx = 0; doomcode_idx < doomcode_len; doomcode_idx++) {
        iowrite32(doomcode[doomcode_idx], doomdev->bar0 + HARDDOOM_FE_CODE_WINDOW);
    }

    iowrite32(0xffffffe, doomdev->bar0 + HARDDOOM_RESET);

    // for commands block:
    iowrite32(doomdev->dma_buffer, doomdev->bar0 + HARDDOOM_CMD_WRITE_PTR);
    iowrite32(doomdev->dma_buffer, doomdev->bar0 + HARDDOOM_CMD_READ_PTR);

    iowrite32(0x3ff, doomdev->bar0 + HARDDOOM_INTR);
    iowrite32(0x3ff ^ HARDDOOM_INTR_PONG_ASYNC, doomdev->bar0 + HARDDOOM_INTR_ENABLE);

    // initializing fence
    iowrite32(0, doomdev->bar0 + HARDDOOM_FENCE_WAIT);
    iowrite32(0, doomdev->bar0 + HARDDOOM_FENCE_LAST);

    iowrite32(0x3ff, doomdev->bar0 + HARDDOOM_ENABLE);
}

static void doom_tasklet_ping_async(unsigned long _doom_device)
{
    struct doom_device *doomdev = (struct doom_device *) _doom_device;

    wake_up_all(&doomdev->pong_async_wait);
}

static void doom_tasklet_fence(unsigned long _doom_device)
{
//    unsigned long flags;
    struct doom_device *doomdev = (struct doom_device *) _doom_device;

//    spin_lock_bh(&doomdev->fence_spinlock);
    wake_up_all(&doomdev->fence_waitqueue);
//    spin_unlock_bh(&doomdev->fence_spinlock);

//    pr_err("FENCE: %d\n", ioread32(doomdev->bar0 + HARDDOOM_FENCE_LAST));
}

static int init_device_structures(struct doom_device *doomdev)
{
    dma_addr_t buffer_addr;

    mutex_init(&doomdev->device_lock);
    sema_init(&doomdev->kobj_semaphore, 0);

    init_waitqueue_head(&doomdev->pong_async_wait);
    doomdev->commands_sent_since_last_ping_async = 0;

    doomdev->last_dst_frame = 0;
    doomdev->last_src_frame = 0;

    atomic64_set(&doomdev->op_counter, 0);
    init_waitqueue_head(&doomdev->fence_waitqueue);
    doomdev->fence_spinlock = __SPIN_LOCK_UNLOCKED(doomdev->fence_spinlock);

    tasklet_init(&doomdev->tasklet_ping_async, doom_tasklet_ping_async, (unsigned long) doomdev);
    tasklet_init(&doomdev->tasklet_fence, doom_tasklet_fence, (unsigned long) doomdev);

    doomdev->commands_space_left = DOOM_BUFFER_SIZE;
    doomdev->batch_size = 0;
    doomdev->buffer = dma_alloc_coherent(&doomdev->pdev->dev, DOOM_BUFFER_SIZE * sizeof(doom_command_t),
                                         &buffer_addr, GFP_KERNEL);
    doomdev->dma_buffer = (doom_dma_ptr_t) buffer_addr;


    doomdev->doom_buffer_pos_write = 0;

    return 0;
}

static void doomdev_kobj_release(struct kobject *kobject)
{
    struct doom_device *doomdev;

    BUG_ON(!kobject);

    doomdev = container_of(kobject, struct doom_device, kobj);
    kobject_put(kobject->parent);

    up(&doomdev->kobj_semaphore);
}

static struct kobj_type doomdev_kobj_type = {.release = doomdev_kobj_release};

static int doom_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    unsigned long err;
    struct doom_device *doomdev;

    doomdev = kzalloc(sizeof(struct doom_device), GFP_KERNEL);
    if (unlikely(!doomdev)) {
        pr_err("kmalloc failed!");
        return -ENOMEM;
    }

    doomdev->pdev = pdev;
    cdev_init(&doomdev->cdev, &doom_fops);

    err = pci_enable_device(pdev);
    if (IS_ERR_VALUE(err)) {
        pr_err("pci_enable_device failed with %lu\n", err);
        goto err_kmalloc;
    }

    pci_set_drvdata(pdev, doomdev);

    err = pci_request_regions(pdev, DEVNAME);
    if (IS_ERR_VALUE(err)) {
        pr_err("pci_request_regions failed with %lu\n", err);
        goto err_enable_device;
    }

    doomdev->bar0 = pci_iomap(pdev, 0, 0);
    if (!doomdev->bar0) {
        pr_err("pci_iomap failed\n");
        err = -EIO;
        goto err_request_regions;
    }

    pci_set_master(pdev);
    err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (IS_ERR_VALUE(err)) {
        pr_err("dma_set_mask_and_coherent failed with %lu\n", err);
        goto err_set_master;
    }

    err = request_irq(pdev->irq, interrupt_handler, IRQF_SHARED, DEVNAME, doomdev);
    if (IS_ERR_VALUE(err)) {
        pr_err("request_irq failed with %lu\n", err);
        goto err_set_master;
    }

    spin_lock(&idr_lock);
    doomdev->minor = idr_alloc(&doom_idr, doomdev, 0, DOOM_MAX_DEV_COUNT, GFP_KERNEL);
    spin_unlock(&idr_lock);
    if (IS_ERR_VALUE((unsigned long) doomdev->minor)) {
        err = doomdev->minor;
        pr_err("idr_alloc failed with %lu\n", err);
        goto err_request_irq;
    }

    err = kobject_init_and_add(&doomdev->kobj, &doomdev_kobj_type, &pdev->dev.kobj, DRIVER_NAME);
    if (IS_ERR_VALUE(err)) {
        pr_err("kobject_init_and_add failed with %lu\n", err);
        goto err_idr;
    }

    err = cdev_add(&doomdev->cdev, doom_major + doomdev->minor, 1);
    if (IS_ERR_VALUE(err)) {
        pr_err("cdev_add failed with %lu\n", err);
        goto err_kobject_init_and_add;
    }

    err = kobject_add(&doomdev->cdev.kobj, &doomdev->kobj, "chardev");
    if (IS_ERR_VALUE(err)) {
        pr_err("kobject_add failed with %lu\n", err);
        goto err_cdev_add;
    }

    doomdev->dev = device_create(
            doom_class, &pdev->dev, doomdev->cdev.dev, doomdev, "doom%d", doomdev->minor);
    if (IS_ERR(doomdev->dev)) {
        err = PTR_ERR(doomdev->dev);
        pr_err("device_create failed with %lu\n", err);
        goto err_kobject_add;
    }

    init_device_structures(doomdev);
    run_init_doom_device_codes(doomdev);

    pr_info("device doom%d probed sucessfully!\n", doomdev->minor);

    return 0;

err_kobject_add:
    kobject_del(&doomdev->cdev.kobj);
err_cdev_add:
    cdev_del(&doomdev->cdev);
err_kobject_init_and_add:
    kobject_del(&doomdev->kobj);
err_idr:
    spin_lock(&idr_lock);
    idr_remove(&doom_idr, doomdev->minor);
    spin_unlock(&idr_lock);
err_request_irq:
    free_irq(pdev->irq, doomdev);
err_set_master:
    pci_clear_master(pdev);
    pci_iounmap(pdev, doomdev->bar0);
err_request_regions:
    pci_release_regions(pdev);
err_enable_device:
    pci_disable_device(pdev);
err_kmalloc:
    kfree(doomdev);
    return err;
}

static irqreturn_t interrupt_handler(int irq, void *dev)
{
    struct doom_device *doomdev = dev;
    u32 intr;
    uint32_t enabled_interrupts;

    intr = ioread32(doomdev->bar0 + HARDDOOM_INTR);
    if (!intr) {
        return IRQ_NONE;
    }
    iowrite32(intr, doomdev->bar0 + HARDDOOM_INTR);

    if (intr & HARDDOOM_INTR_FE_ERROR)
        pr_err("received pci FE ERROR interrupt, about critical failure caused by instruction: %x\n",
               ioread32(doomdev->bar0 + HARDDOOM_FE_ERROR_CODE));

    if (intr & (HARDDOOM_INTR_FIFO_OVERFLOW
                | HARDDOOM_INTR_SURF_DST_OVERFLOW
                | HARDDOOM_INTR_SURF_SRC_OVERFLOW
                | HARDDOOM_INTR_PAGE_FAULT_SURF_DST
                | HARDDOOM_INTR_PAGE_FAULT_SURF_SRC
                | HARDDOOM_INTR_PAGE_FAULT_TEXTURE))
        pr_err("received pci interrupt about critical device failure, interrupt code: %x\n", intr);

    if (intr & HARDDOOM_INTR_PONG_ASYNC) {
        enabled_interrupts = ioread32(doomdev->bar0 + HARDDOOM_INTR_ENABLE);
        iowrite32(enabled_interrupts & (~HARDDOOM_INTR_PONG_ASYNC), doomdev->bar0 + HARDDOOM_INTR_ENABLE);
        tasklet_schedule(&doomdev->tasklet_ping_async);
    }

    if (intr & HARDDOOM_INTR_FENCE) {
        tasklet_schedule(&doomdev->tasklet_fence);
    }

    return IRQ_HANDLED;
}

static void doom_remove(struct pci_dev *pdev)
{
    struct doom_device *dev = pci_get_drvdata(pdev);

    BUG_ON(!dev);

    device_destroy(doom_class, dev->cdev.dev);
    cdev_del(&dev->cdev);
    kobject_del(&dev->kobj);

    // waiting for reference count to drop:
    kobject_put(&dev->kobj);
    down(&dev->kobj_semaphore);
    kobject_del(&dev->cdev.kobj);

    iowrite32(0, dev->bar0 + HARDDOOM_ENABLE);
    iowrite32(0, dev->bar0 + HARDDOOM_INTR_ENABLE);
    (void) ioread32(dev->bar0 + HARDDOOM_FIFO_FREE);

    tasklet_kill(&dev->tasklet_fence);
    tasklet_kill(&dev->tasklet_ping_async);

    spin_lock(&idr_lock);
    idr_remove(&doom_idr, dev->minor);
    spin_unlock(&idr_lock);

    free_irq(pdev->irq, dev);
    pci_iounmap(pdev, dev->bar0);
    pci_release_regions(dev->pdev);
    pci_disable_device(dev->pdev);
    kfree(dev);
}

static long doom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct doom_context *context;

    if (unlikely(file->f_op != &doom_fops)) {
        pr_err("doom ioctl operation was run on incorrect file/device!\n");
        return -EINVAL;
    }
    context = file->private_data;

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
        pr_err("Error getting device data, id might not be valid.");
        return -ENOENT;
    }

    context = kmalloc(sizeof(struct doom_context), GFP_KERNEL);
    if (unlikely(!context)) {
        pr_err("kmalloc failed when allocating memory for struct doom_context\n");
        return -ENOMEM;
    }

    context->dev = dev;
    filep->private_data = context;
    return 0;
}

static int doom_release(struct inode *ino, struct file *filep)
{
    if (unlikely(filep->f_op != &doom_fops)) {
        pr_err("doom_release invoked on incorrect file\n");
        return -EINVAL;
    }
    kfree(filep->private_data); // freeing memory of struct doom_context
    return 0;
}

static int doom_init(void)
{
    long err;

    doom_class = class_create(THIS_MODULE, DEVNAME);
    if (IS_ERR(doom_class)) {
        return PTR_ERR(doom_class);
    }

    err = alloc_chrdev_region(&doom_major, 0, DOOM_MAX_DEV_COUNT, DEVNAME);
    if (IS_ERR_VALUE(err)) {
        pr_err("alloc_chdev_region failed with %ld\n", err);
        goto err_class_create;
    }

    err = pci_register_driver(&this_driver);
    if (IS_ERR_VALUE(err)) {
        pr_err("pci_register_driver failed with %ld\n", err);
        goto err_alloc_chrdev_region;
    }

err_alloc_chrdev_region:
    unregister_chrdev_region(doom_major, DOOM_MAX_DEV_COUNT);
err_class_create:
//    class_unregister(&doom_class);
    class_destroy(doom_class);
    return err;
}

static void doom_cleanup(void)
{
    pci_unregister_driver(&this_driver);
    idr_destroy(&doom_idr);
    unregister_chrdev_region(doom_major, DOOM_MAX_DEV_COUNT);
//    class_unregister(&doom_class);
    class_destroy(doom_class);
}

module_init(doom_init);
module_exit(doom_cleanup);
