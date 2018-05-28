#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/anon_inodes.h>
#include <linux/uaccess.h>
#include <linux/file.h>

#include "harddoom.h"
#include "doom_commands.h"

#include "doom_resources.h"

static long doom_frame_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t doom_frame_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);
static int doom_frame_release(struct inode *ino, struct file *filep);

static int doom_col_texture_release(struct inode *ino, struct file *filep);
static int doom_flat_texture_release(struct inode *ino, struct file *filep);
static int doom_colormaps_release(struct inode *ino, struct file *filep);

static long doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects __user *arg);
static long doom_frame_fill_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_fill_rects __user *arg);
static long doom_frame_draw_lines(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines __user *arg);
static long doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background __user *arg);
static long doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns __user *arg);
static long doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans __user *arg);

static struct file_operations doom_frame_fops = {
        .owner = THIS_MODULE,
        .release = doom_frame_release,
        .read = doom_frame_read,
        .unlocked_ioctl = doom_frame_ioctl,
        .compat_ioctl = doom_frame_ioctl,
};

static struct file_operations doom_col_texture_fops = {
        .owner = THIS_MODULE,
        .release = doom_col_texture_release,
};

static struct file_operations doom_flat_texture_fops = {
        .owner = THIS_MODULE,
        .release = doom_flat_texture_release,
};

struct file_operations doom_colormaps_fops = {
        .owner = THIS_MODULE,
        .release = doom_colormaps_release,
};

static int is_fence_ready(struct doom_context * context, uint64_t fence_num)
{
    int fence_last = ioread32(context->dev->bar0 + HARDDOOM_FENCE_LAST);

    return fence_num <= fence_last && (fence_last - fence_num  < HARDDOOM_FENCE_MASK / 2);
}

static void wait_for_fence(struct doom_context * context, uint64_t fence_num)
{
//    unsigned long flags;

//    spin_lock_irqsave(&context->dev->fence_spinlock, flags);
    while(!is_fence_ready(context, fence_num)) {
        iowrite32(fence_num, context->dev->bar0 + HARDDOOM_FENCE_WAIT);
//        spin_unlock_irqrestore(&context->dev->fence_spinlock, flags);
        wait_event_interruptible(context->dev->fence_waitqueue,
                                 is_fence_ready(context, fence_num));
//        spin_lock_irqsave(&context->dev->fence_spinlock, flags);
    }
//    spin_unlock_irqrestore(&context->dev->fence_spinlock, flags);
}

long create_frame_buffer(struct doom_context * context, struct doomdev_ioctl_create_surface *ptr)
{
    int err = 0;
    int i;
    int err_i;
    long frame_fd;
    struct doom_frame *frame;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_surface kernel_arg;
    struct file *created_file;

    if (unlikely(copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_surface)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_ioctl_create_surface from user\n");
        return -EFAULT;
    }

    if (unlikely(
            kernel_arg.width < DOOMDEV_SURFACE_MIN_WIDTH
            || kernel_arg.width % DOOMDEV_SURFACE_WIDTH_DIVISIBLE != 0
            || kernel_arg.height < DOOMDEV_SURFACE_MIN_HEIGHT)) {
        pr_err("Wrong values in argument for creating frame buffer\n");
        return -EINVAL;
    }

    if (unlikely(DOOMDEV_SURFACE_MAX_WIDTH < kernel_arg.width || DOOMDEV_SURFACE_MAX_HEIGHT < kernel_arg.height)) {
        pr_err("frame buffer dimensions exceed max value!\n");
        return -EOVERFLOW;
    }

    frame = kmalloc(sizeof(struct doom_frame), GFP_KERNEL);
    if (unlikely(!frame)) {
        pr_err("kmalloc failed allocating doom_frame structure for frame buffer\n");
        return -ENOMEM;
    }

    frame->context = context;
    frame->kobj = kobject_get(&context->dev->kobj);
    if (unlikely(!frame->kobj)) {
        pr_err("kobject_get failed, probably kobject is in the process of being destroyed\n");
        goto err_kmalloc_frame;
    }

    frame->width = kernel_arg.width;
    frame->height = kernel_arg.height;
    frame->pages_count = roundup(kernel_arg.width * kernel_arg.height, HARDDOOM_PAGE_SIZE) / HARDDOOM_PAGE_SIZE;
    frame->page_table_size = roundup(frame->pages_count * sizeof(doom_dma_ptr_t), DOOMDEV_PT_ALIGN);

    // allocating dma page table - it will always fit on 1 page (1024 entries * 4 bytes)
    frame->pt_dma = dma_alloc_coherent(&context->dev->pdev->dev,
                                       HARDDOOM_PAGE_SIZE,
                                       &dma_addr,
                                       GFP_KERNEL);
    frame->pt_dma_addr = (doom_dma_ptr_t) dma_addr;
    // and also virt page table:
    frame->pt_virt = kmalloc(frame->pages_count * sizeof(doom_dma_ptr_t *), GFP_KERNEL);

    if (unlikely(!frame->pt_dma || !frame->pt_dma_addr || !frame->pt_virt)) {
        pr_err("dma_alloc_coherent or kmalloc failed when allocating page table for frame buffer\n");
        err = -ENOMEM;
        goto err_alloc_frame_pagetable;
    }

    // allocating pages:
    for (i = 0; i < frame->pages_count; i++) {
        frame->pt_virt[i] = dma_alloc_coherent(&context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, &dma_addr, GFP_KERNEL);
        if (unlikely(!frame->pt_virt[i] || !dma_addr)) {
            pr_err("dma_alloc_coherent failed when allocating page for frame buffer\n");
            err = -ENOMEM;
            goto err_dma_alloc_frame_pages;
        }
        frame->pt_dma[i] = (doom_dma_ptr_t) dma_addr | HARDDOOM_PTE_VALID;
    }

    frame->last_fence = atomic64_read(&context->dev->op_counter);

//    wmb();

    frame_fd = anon_inode_getfd("frame", &doom_frame_fops, frame, 0);
    if (IS_ERR_VALUE(frame_fd)) {
        pr_err("anon_inode_getfd failed with code: %ld when creating fd for frame buffer\n", frame_fd);
        err = frame_fd;
        goto err_dma_alloc_frame_pages;
    }

    // making file seekable / readable and also open for read (necessary)
    created_file = fget(frame_fd);
    if (IS_ERR(created_file)) {
        pr_err("fget failed with code %ld when getting just created frame buffer file", PTR_ERR(created_file));
        err = PTR_ERR(created_file);
        goto err_dma_alloc_frame_pages;
    }
    created_file->f_mode |= FMODE_READ | FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
    fput(created_file);

    return frame_fd;

err_dma_alloc_frame_pages:
    for (err_i = 0; err_i < i; err_i++) {
        dma_free_coherent(&context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          frame->pt_virt[err_i],
                          frame->pt_dma[err_i] ^ HARDDOOM_PTE_VALID);
    }
err_alloc_frame_pagetable:
    if (frame->pt_dma && frame->pt_dma_addr)
        dma_free_coherent(&context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, frame->pt_dma, frame->pt_dma_addr);
    if (frame->pt_virt)
        kfree(frame->pt_virt);
    kobject_put(frame->kobj);
err_kmalloc_frame:
    kfree(frame);
    return err;
}

static long create_column_texture_pagetable_on_last_page(struct doom_context *context,
                                                         struct doom_col_texture *col_texture,
                                                         struct doomdev_ioctl_create_texture kernel_arg)
{
    int err;
    int err_i;
    int i;
    doom_dma_ptr_t last_page_dma;
    void * last_page_virt;
    dma_addr_t dma_addr;
    size_t to_copy;
    long col_texture_fd;


    col_texture->is_page_table_on_last_page = 1;
    last_page_virt = dma_alloc_coherent(&context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, &dma_addr, GFP_KERNEL);
    last_page_dma = (doom_dma_ptr_t) dma_addr;
    if (unlikely(!last_page_virt || !last_page_dma)) {
        pr_err("dma_alloc_coherent failed when allocating page table / last page for column texture\n");
        return -ENOMEM;
    }

    col_texture->pt_dma = last_page_virt + (col_texture->rounded_texture_size % HARDDOOM_PAGE_SIZE);
    col_texture->pt_dma[col_texture->pages_count - 1] = last_page_dma | HARDDOOM_PTE_VALID;
    col_texture->pt_dma_addr = last_page_dma + (col_texture->rounded_texture_size % HARDDOOM_PAGE_SIZE);

    col_texture->pt_virt = kmalloc(col_texture->pages_count * sizeof(doom_dma_ptr_t *), GFP_KERNEL);
    if (unlikely(!col_texture->pt_virt)) {
        pr_err("kmalloc failed allocating virtual memory page table for column texture\n");
        err = -ENOMEM;
        goto err_dma_alloc_col_texture_last_page_1;
    }
    col_texture->pt_virt[col_texture->pages_count - 1] = last_page_virt;

    to_copy = min((size_t) HARDDOOM_PAGE_SIZE,
                  col_texture->texture_size - (col_texture->pages_count - 1) * HARDDOOM_PAGE_SIZE);
    if (unlikely(
            copy_from_user(col_texture->pt_virt[col_texture->pages_count - 1],
                           (void *) kernel_arg.data_ptr + (col_texture->pages_count - 1) * HARDDOOM_PAGE_SIZE,
                           to_copy))) {
        pr_err("copy_from_user failed copying last page of column texture data\n");
        err = -EFAULT;
        goto err_kmalloc_col_texture_virt_pagetable_1;
    }

    for (i = 0; i < col_texture->pages_count - 1; i++) {
        col_texture->pt_virt[i] =
                dma_alloc_coherent(&context->dev->pdev->dev,
                                   HARDDOOM_PAGE_SIZE,
                                   &dma_addr,
                                   GFP_KERNEL);
        if (unlikely(!col_texture->pt_virt[i] || !dma_addr)) {
            pr_err("dma_alloc_coherent failed when allocating page for column texture\n");
            err = -ENOMEM;
            goto err_dma_alloc_col_texture_pages_1;
        }
        col_texture->pt_dma[i] = (doom_dma_ptr_t) dma_addr | HARDDOOM_PTE_VALID;
        if (unlikely(
                copy_from_user(col_texture->pt_virt[i],
                               (void *) kernel_arg.data_ptr + i * HARDDOOM_PAGE_SIZE,
                               HARDDOOM_PAGE_SIZE))) {
            pr_err("copy_from_user failed copying page of column texture data\n");
            err = -EFAULT;
            i++; // so we also free space allocated for "current" page
            goto err_dma_alloc_col_texture_pages_1;
        }
    }

    col_texture_fd = anon_inode_getfd("col_texture", &doom_col_texture_fops, col_texture, 0);
    if (IS_ERR_VALUE(col_texture_fd)) {
        pr_err("anon_inode_getfd failed with code: %ld when creating fd for column texture\n", col_texture_fd);
        err = col_texture_fd;
        goto err_dma_alloc_col_texture_pages_1;
    }

    return col_texture_fd;

err_dma_alloc_col_texture_pages_1:
    for (err_i = 0; err_i < i && err_i < col_texture->pages_count - 1; err_i++) {
        dma_free_coherent(&context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          col_texture->pt_virt[err_i],
                          col_texture->pt_dma[err_i] ^ HARDDOOM_PTE_VALID);
    }
err_kmalloc_col_texture_virt_pagetable_1:
    kfree(col_texture->pt_virt);
err_dma_alloc_col_texture_last_page_1:
    dma_free_coherent(&context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, last_page_virt, last_page_dma);
    return err;
}


static long create_column_texture_separate_pagetable(struct doom_context *context,
                                                     struct doom_col_texture *col_texture,
                                                     struct doomdev_ioctl_create_texture kernel_arg)
{
    long err;
    int err_i;
    int i;
    dma_addr_t dma_addr;
    long col_texture_fd;


    col_texture->is_page_table_on_last_page = 0;
    col_texture->pt_dma = dma_alloc_coherent(&context->dev->pdev->dev,
                                             HARDDOOM_PAGE_SIZE,
                                             &dma_addr,
                                             GFP_KERNEL);
    col_texture->pt_dma_addr = (doom_dma_ptr_t) dma_addr;
    if (unlikely(!col_texture->pt_dma || !dma_addr)) {
        pr_err("dma_alloc_coherent failed when allocating page table for column texture\n");
        return -ENOMEM;
    }
    col_texture->pt_virt = kmalloc(col_texture->pages_count * sizeof(doom_dma_ptr_t *), GFP_KERNEL);
    if (unlikely(!col_texture->pt_dma || !col_texture->pt_dma_addr || !col_texture->pt_virt)) {
        pr_err("dma_alloc_coherent or kmalloc failed when allocating page table for column texture\n");
        err = -ENOMEM;
        goto err_dma_alloc_col_texture_pagetable_2;
    }

    for (i = 0; i < col_texture->pages_count; i++) {
        col_texture->pt_virt[i] =
                dma_alloc_coherent(&context->dev->pdev->dev,
                                   HARDDOOM_PAGE_SIZE,
                                   &dma_addr,
                                   GFP_KERNEL);
        if (unlikely(!col_texture->pt_virt[i] || !dma_addr)) {
            pr_err("dma_alloc_coherent failed when allocating page for column texture\n");
            err = -ENOMEM;
            goto err_dma_alloc_col_texture_pages_2;
        }
        col_texture->pt_dma[i] = (doom_dma_ptr_t) dma_addr | HARDDOOM_PTE_VALID;
        if (unlikely(
                copy_from_user(
                        col_texture->pt_virt[i],
                        (void *) kernel_arg.data_ptr + i * HARDDOOM_PAGE_SIZE,
                        min((size_t) HARDDOOM_PAGE_SIZE, col_texture->texture_size - i * HARDDOOM_PAGE_SIZE)))) {
            pr_err("copy_from_user failed copying page of column texture data\n");
            err = -EFAULT;
            i++; // so we also free space allocated for "current" page
            goto err_dma_alloc_col_texture_pages_2;
        }
    }

    col_texture_fd = anon_inode_getfd("col_texture", &doom_col_texture_fops, col_texture, 0);
    if (IS_ERR_VALUE(col_texture_fd)) {
        pr_err("anon_inode_getfd failed with code: %ld when creating fd for column texture\n", col_texture_fd);
        err = col_texture_fd;
        goto err_dma_alloc_col_texture_pages_2;
    }
    return col_texture_fd;

err_dma_alloc_col_texture_pages_2:
    for (err_i = 0; err_i < i; err_i++) {
        dma_free_coherent(&context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          col_texture->pt_virt[err_i],
                          col_texture->pt_dma[err_i] ^ HARDDOOM_PTE_VALID);
    }
err_dma_alloc_col_texture_pagetable_2:
    dma_free_coherent(&context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, col_texture->pt_dma, col_texture->pt_dma_addr);
    return err;
}

long create_column_texture(struct doom_context * context, struct doomdev_ioctl_create_texture *ptr)
{
    long err = 0;
    long col_texture_fd;
    struct doom_col_texture *col_texture;
    struct doomdev_ioctl_create_texture kernel_arg;


    if (unlikely(copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_texture)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_ioctl_create_texture from user\n");
        return -EFAULT;
    }

    col_texture = kmalloc(sizeof(struct doom_col_texture), GFP_KERNEL);
    if (unlikely(!col_texture)) {
        pr_err("kmalloc failed allocating doom_col_texture structure for column texture\n");
        return -ENOMEM;
    }

    col_texture->context = context;
    col_texture->kobj = kobject_get(&context->dev->kobj);
    if (unlikely(!col_texture->kobj)) {
        pr_err("kobject_get failed, probably kobject is in the process of being destroyed\n");
        goto err_kmalloc_col_texture;
    }

    col_texture->height = kernel_arg.height;
    col_texture->texture_size = kernel_arg.size;
    col_texture->rounded_texture_size = roundup(kernel_arg.size, DOOMDEV_COL_TEXTURE_MEM_ALIGN);
    col_texture->pages_count = roundup(col_texture->rounded_texture_size, HARDDOOM_PAGE_SIZE) / HARDDOOM_PAGE_SIZE;
    col_texture->page_table_size = roundup(col_texture->pages_count * sizeof(doom_dma_ptr_t), DOOMDEV_PT_ALIGN);

    // in case we have enough space left on last page to put pagetable there
    if ((0 < (col_texture->rounded_texture_size % HARDDOOM_PAGE_SIZE))
            && (col_texture->page_table_size <= HARDDOOM_PAGE_SIZE
                                                - (col_texture->rounded_texture_size % HARDDOOM_PAGE_SIZE))) {
        col_texture_fd = create_column_texture_pagetable_on_last_page(context, col_texture, kernel_arg);
        if (IS_ERR_VALUE(col_texture_fd)) {
            err = col_texture_fd;
            pr_err("create_column_texture_pagetable_on_last_page failed with code %ld\n", err);
            goto err_kobject_get_col_texture;
        }
    } else { // otherwise - we need separate allocation for pagetable
        col_texture_fd = create_column_texture_separate_pagetable(context, col_texture, kernel_arg);
        if (IS_ERR_VALUE(col_texture_fd)) {
            err = col_texture_fd;
            pr_err("create_column_texture_separate_pagetable failed with code %ld\n", err);
            goto err_kobject_get_col_texture;
        }
    }

    // fill with zeros unaligned part of column texture (to align to 0x100 bytes)
    if (col_texture->texture_size != col_texture->rounded_texture_size) {
        memset(col_texture->pt_virt[col_texture->pages_count - 1] + col_texture->texture_size % HARDDOOM_PAGE_SIZE,
               0,
               col_texture->rounded_texture_size - col_texture->texture_size);
    }
    col_texture->last_fence = atomic64_read(&context->dev->op_counter);

    return col_texture_fd;

//    wmb();

err_kobject_get_col_texture:
    kobject_put(col_texture->kobj);
err_kmalloc_col_texture:
    kfree(col_texture);
    return err;
}

long create_flat_texture(struct doom_context * context, struct doomdev_ioctl_create_flat *ptr)
{
    long err = 0;
    long flat_texture_fd;
    struct doom_flat_texture *texture;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_flat kernel_arg;

    if (unlikely(copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_flat)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_ioctl_create_flat from user\n");
        return -EFAULT;
    }

    texture = kmalloc(sizeof(struct doom_flat_texture), GFP_KERNEL);
    if (unlikely(!texture)) {
        pr_err("kmalloc failed allocating doom_flat_texture structure for flat texture\n");
        return -ENOMEM;
    }
    texture->context = context;
    texture->kobj = kobject_get(&context->dev->kobj);
    if (unlikely(!texture->kobj)) {
        pr_err("kobject_get failed, probably kobject is in the process of being destroyed\n");
        goto err_kmalloc_flat_texture;
    }

    //HARDDOOM_FLAT_SIZE == HARDDOOM_PAGE_SIZE:
    texture->ptr_virt = dma_alloc_coherent(&context->dev->pdev->dev, HARDDOOM_FLAT_SIZE, &dma_addr, GFP_KERNEL);
    texture->ptr_dma = (doom_dma_ptr_t) dma_addr;
    if (unlikely(!texture->ptr_virt || !texture->ptr_dma)) {
        pr_err("dma_alloc_coherent failed when allocating memory for flat texture\n");
        err = -ENOMEM;
        goto err_kobject_get_flat_texture;
    }

    if (unlikely(copy_from_user(texture->ptr_virt, (void *) kernel_arg.data_ptr, HARDDOOM_FLAT_SIZE))) {
        pr_err("copy_from_user failed when copying flat texture data\n");
        err = -EFAULT;
        goto err_dma_alloc_flat_texture;
    }

    texture->last_fence = atomic64_read(&context->dev->op_counter);
//    wmb();

    flat_texture_fd = anon_inode_getfd("flat_texture", &doom_flat_texture_fops, texture, 0);
    if (IS_ERR_VALUE(flat_texture_fd)) {
        err = flat_texture_fd;
        pr_err("anon_inode_getfd failed with code: %ld when creating fd for flat texture\n", err);
        goto err_dma_alloc_flat_texture;
    }

    return flat_texture_fd;

err_dma_alloc_flat_texture:
    dma_free_coherent(&context->dev->pdev->dev, HARDDOOM_FLAT_SIZE, texture->ptr_virt, texture->ptr_dma);
err_kobject_get_flat_texture:
    kobject_put(texture->kobj);
err_kmalloc_flat_texture:
    kfree(texture);
    return err;
}

long create_colormaps_array(struct doom_context * context, struct doomdev_ioctl_create_colormaps *ptr)
{
    long err = 0;
    long colormaps_array_fd;
    struct doom_colormaps *colormaps;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_colormaps kernel_arg;

    if (unlikely(copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_colormaps)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_ioctl_create_colormaps from user\n");
        return -EFAULT;
    }

    colormaps = kmalloc(sizeof(struct doom_colormaps), GFP_KERNEL);
    if (unlikely(!colormaps)) {
        pr_err("kmalloc failed allocating doom_colormaps structure for array of colormaps\n");
        return -ENOMEM;
    }
    colormaps->context = context;
    colormaps->count = kernel_arg.num;
    colormaps->kobj = kobject_get(&context->dev->kobj);
    if (unlikely(!colormaps->kobj)) {
        pr_err("kobject_get failed, probably kobject is in the process of being destroyed\n");
        goto err_kmalloc_colormaps;
    }

    colormaps->ptr_virt = dma_alloc_coherent(&context->dev->pdev->dev,
                                             roundup(colormaps->count * HARDDOOM_COLORMAP_SIZE, HARDDOOM_PAGE_SIZE),
                                             &dma_addr,
                                             GFP_KERNEL);
    colormaps->ptr_dma = (doom_dma_ptr_t) dma_addr;
    if (unlikely(!colormaps->ptr_virt || !colormaps->ptr_dma)) {
        pr_err("dma_alloc_coherent failed when allocating memory for array of colormaps\n");
        err = -ENOMEM;
        goto err_kobject_get_colormaps;
    }

    if (unlikely(
            copy_from_user(colormaps->ptr_virt,
                           (void *) kernel_arg.data_ptr,
                           colormaps->count * HARDDOOM_COLORMAP_SIZE))) {
        pr_err("copy_from_user failed when copying array of colormaps data\n");
        err = -EFAULT;
        goto err_dma_alloc_colormaps;
    }

    colormaps->last_fence = atomic64_read(&context->dev->op_counter);
//    wmb();

    colormaps_array_fd = anon_inode_getfd("colormaps", &doom_colormaps_fops, colormaps, 0);
    if (IS_ERR_VALUE(colormaps_array_fd)) {
        err = colormaps_array_fd;
        pr_err("anon_inode_getfd failed with code: %ld when creating fd for array of colormaps\n", err);
        goto err_dma_alloc_colormaps;
    }

    return colormaps_array_fd;


err_dma_alloc_colormaps:
    dma_free_coherent(&context->dev->pdev->dev,
                      roundup(colormaps->count * HARDDOOM_COLORMAP_SIZE, HARDDOOM_PAGE_SIZE),
                      colormaps->ptr_virt,
                      colormaps->ptr_dma);
err_kobject_get_colormaps:
    kobject_put(colormaps->kobj);
err_kmalloc_colormaps:
    kfree(colormaps);
    return err;
}

static long doom_frame_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (unlikely(file->f_op != &doom_frame_fops)) {
        pr_err("frame ioctl operation was run on incorrect file!\n");
        return -EINVAL;
    }

    switch (cmd) {
    case DOOMDEV_SURF_IOCTL_COPY_RECTS:
        return doom_frame_copy_rects(file->private_data, (struct doomdev_surf_ioctl_copy_rects *) arg);
    case DOOMDEV_SURF_IOCTL_FILL_RECTS:
        return doom_frame_fill_rects(file->private_data, (struct doomdev_surf_ioctl_fill_rects *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_LINES:
        return doom_frame_draw_lines(file->private_data, (struct doomdev_surf_ioctl_draw_lines *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_BACKGROUND:
        return doom_frame_draw_background(file->private_data, (struct doomdev_surf_ioctl_draw_background *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_COLUMNS:
        return doom_frame_draw_columns(file->private_data, (struct doomdev_surf_ioctl_draw_columns *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_SPANS:
        return doom_frame_draw_spans(file->private_data, (struct doomdev_surf_ioctl_draw_spans *) arg);
    default:
        pr_err("ioctl on frame file called with incorrect command\n");
        return -EINVAL;
    }
}

static ssize_t doom_frame_read(struct file *filep, char __user *buff, size_t count, loff_t *offp)
{
    unsigned long not_copied;
    struct doom_frame *frame;
    size_t copied = 0;
    uint32_t page_num = (*offp) / HARDDOOM_PAGE_SIZE;
    int to_copy;

    if (unlikely(filep->f_op != &doom_frame_fops)) {
        pr_err("frame read operation was run on incorrect file descriptor\n");
        return -EINVAL;
    }

    frame = filep->private_data;

    mutex_lock(&frame->context->dev->device_lock);
    wait_for_fence(frame->context, frame->last_fence);

    while (copied < count) {
        to_copy = min(count - copied, (size_t) (HARDDOOM_PAGE_SIZE - ((*offp) % HARDDOOM_PAGE_SIZE)));
        not_copied = copy_to_user(buff + copied, frame->pt_virt[page_num] + ((*offp) % HARDDOOM_PAGE_SIZE), to_copy);
        if (unlikely(not_copied)) {
            mutex_unlock(&frame->context->dev->device_lock);
            pr_err("unable to copy %lu frame bytes to user, retry needed\n", not_copied);
            return copied + to_copy - not_copied;
        }
        copied += to_copy;
        (*offp) += to_copy;
        page_num = (*offp) / HARDDOOM_PAGE_SIZE;
    }

    mutex_unlock(&frame->context->dev->device_lock);
    return count;
}


static int doom_frame_release(struct inode *ino, struct file *filep)
{
    int i;
    struct doom_frame *frame;

    if (unlikely(filep->f_op != &doom_frame_fops)) {
        pr_err("frame release operation was run on incorrect file descriptor\n");
        return -EINVAL;
    }

    frame = filep->private_data;

    mutex_lock(&frame->context->dev->device_lock);
    wait_for_fence(frame->context, frame->last_fence);
    for (i = 0; i < frame->pages_count; i++) {
        dma_free_coherent(&frame->context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          frame->pt_virt[i],
                          frame->pt_dma[i] ^ HARDDOOM_PTE_VALID);
    }
    dma_free_coherent(&frame->context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, frame->pt_dma, frame->pt_dma_addr);
    kfree(frame->pt_virt);
    kobject_put(frame->kobj);
    mutex_unlock(&frame->context->dev->device_lock);

    kfree(frame);
    return 0;
}

static int doom_col_texture_release(struct inode *ino, struct file *filep)
{
    int i;
    struct doom_col_texture *col_texture;

    if (unlikely(filep->f_op != &doom_col_texture_fops)) {
        pr_err("column texture release operation was run on incorrect file descriptor\n");
        return -EINVAL;
    }

    col_texture = filep->private_data;

    mutex_lock(&col_texture->context->dev->device_lock);
    wait_for_fence(col_texture->context, col_texture->last_fence);
    for (i = 0; i < col_texture->pages_count; i++) {
        dma_free_coherent(&col_texture->context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          col_texture->pt_virt[i],
                          col_texture->pt_dma[i] ^ HARDDOOM_PTE_VALID);
    }
    if (!col_texture->is_page_table_on_last_page)
        dma_free_coherent(&col_texture->context->dev->pdev->dev,
                          HARDDOOM_PAGE_SIZE,
                          col_texture->pt_dma,
                          col_texture->pt_dma_addr);
    kfree(col_texture->pt_virt);
    kobject_put(col_texture->kobj);
    mutex_unlock(&col_texture->context->dev->device_lock);

    kfree(col_texture);
    return 0;
}

static int doom_flat_texture_release(struct inode *ino, struct file *filep)
{
    struct doom_flat_texture *texture;

    if (unlikely(filep->f_op != &doom_flat_texture_fops)) {
        pr_err("flat texture release operation was run on incorrect file descriptor\n");
        return -EINVAL;
    }

    texture = filep->private_data;

    mutex_lock(&texture->context->dev->device_lock);
    wait_for_fence(texture->context, texture->last_fence);
    dma_free_coherent(&texture->context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, texture->ptr_virt, texture->ptr_dma);
    kobject_put(texture->kobj);
    mutex_unlock(&texture->context->dev->device_lock);

    kfree(texture);
    return 0;
}

static int doom_colormaps_release(struct inode *ino, struct file *filep)
{
    struct doom_colormaps *colormaps;

    if (unlikely(filep->f_op != &doom_colormaps_fops)) {
        pr_err("colormaps release operation was run on incorrect file descriptor\n");
        return -EINVAL;
    }

    colormaps = filep->private_data;

    mutex_lock(&colormaps->context->dev->device_lock);
    wait_for_fence(colormaps->context, colormaps->last_fence);
    dma_free_coherent(&colormaps->context->dev->pdev->dev,
                      roundup(colormaps->count * HARDDOOM_COLORMAP_SIZE, HARDDOOM_PAGE_SIZE),
                      colormaps->ptr_virt,
                      colormaps->ptr_dma);
    kobject_put(colormaps->kobj);
    mutex_unlock(&colormaps->context->dev->device_lock);

    kfree(colormaps);
    return 0;
}

static long doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects *arg)
{
    long err;
    uint16_t i;
    struct file *src_file;
    struct doomdev_surf_ioctl_copy_rects kernel_arg;
    struct doom_frame *src_frame;
    struct doomdev_copy_rect *rects_to_copy;
    int fence_num;

    if (unlikely(copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_copy_rects)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_copy_rects from user\n");
        return -EFAULT;
    }

    src_file = fget(kernel_arg.surf_src_fd);
    if (IS_ERR(src_file)) {
        pr_err("fget failed with code: %ld when getting source frame for copy rects operation\n", PTR_ERR(src_file));
        return PTR_ERR(src_file);
    }

    if (unlikely(src_file->f_op != &doom_frame_fops)) {
        pr_err("copy rects operation was run with incorrect source frame file descriptor\n");
        err = -EINVAL;
        goto err_copy_rects_put_src_file;
    }
    src_frame = src_file->private_data;

    if (unlikely(frame->width != src_frame->width || frame->height != src_frame->height)) {
        pr_err("source and destination frame have different dimensions in copy_rect\n");
        err = -EINVAL;
        goto err_copy_rects_put_src_file;
    }

    rects_to_copy = kmalloc(kernel_arg.rects_num * sizeof(struct doomdev_copy_rect), GFP_KERNEL);
    if (unlikely(!rects_to_copy)) {
        pr_err("kmalloc failed when allocating memory for doomdev_copy_rect structures\n");
        err = -ENOMEM;
        goto err_copy_rects_put_src_file;
    }
    if (unlikely(
            copy_from_user(
                    rects_to_copy,
                    (void *) kernel_arg.rects_ptr,
                    kernel_arg.rects_num * sizeof(struct doomdev_copy_rect)))) {
        pr_err("copy_from_user failed when copying multiple doomdev_copy_rect from user\n");
        err = -EFAULT;
        goto err_copy_rects_kmalloc_rects_to_copy;
    }

    mutex_lock(&frame->context->dev->device_lock);
    send_command(frame->context, HARDDOOM_CMD_INTERLOCK);
    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    if (frame->context->dev->last_src_frame != src_frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_SRC_PT(src_frame->pt_dma_addr));
        frame->context->dev->last_src_frame = src_frame->pt_dma_addr;
    }
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < kernel_arg.rects_num; i++) {
        if (unlikely(
                rects_to_copy[i].pos_dst_x < 0
                || rects_to_copy[i].pos_dst_x + rects_to_copy[i].width > frame->width
                || rects_to_copy[i].pos_dst_y < 0
                || rects_to_copy[i].pos_dst_y + rects_to_copy[i].height > frame->height
                || rects_to_copy[i].pos_src_x < 0
                || rects_to_copy[i].pos_src_x + rects_to_copy[i].width > src_frame->width
                || rects_to_copy[i].pos_src_y  < 0
                || rects_to_copy[i].pos_src_y + rects_to_copy[i].height > src_frame->height)) {
            pr_err("processing copy_rect failed - rectangle to copy is outside of frame\n");
            if (i == 0)
                err = -EINVAL;
            else
                err = i;
            goto err_copy_rects_unlock_mutex;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(rects_to_copy[i].pos_dst_x, rects_to_copy[i].pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(rects_to_copy[i].pos_src_x, rects_to_copy[i].pos_src_y));
        send_command(frame->context, HARDDOOM_CMD_COPY_RECT(rects_to_copy[i].width, rects_to_copy[i].height));
    }

    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;
    src_frame->last_fence = fence_num;
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    mutex_unlock(&frame->context->dev->device_lock);
    kfree(rects_to_copy);
    fput(src_file);

    return kernel_arg.rects_num;

err_copy_rects_unlock_mutex:
    mutex_unlock(&frame->context->dev->device_lock);
err_copy_rects_kmalloc_rects_to_copy:
    kfree(rects_to_copy);
err_copy_rects_put_src_file:
    fput(src_file);
    return err;
}

static long doom_frame_fill_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_fill_rects *arg)
{
    long err;
    uint16_t i;
    struct doomdev_surf_ioctl_fill_rects kernel_arg;
    struct doomdev_fill_rect *rects_to_fill;
    int fence_num;

    if (unlikely(copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_fill_rects)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_fill_rects from user\n");
        return -EFAULT;
    }

    rects_to_fill = kmalloc(kernel_arg.rects_num * sizeof(struct doomdev_fill_rect), GFP_KERNEL);
    if (unlikely(!rects_to_fill)) {
        pr_err("kmalloc failed when allocating memory for doomdev_fill_rect structures\n");
        return -ENOMEM;
    }
    if (unlikely(
            copy_from_user(
                    rects_to_fill,
                    (void *) kernel_arg.rects_ptr,
                    kernel_arg.rects_num * sizeof(struct doomdev_fill_rect)))) {
        pr_err("copy_from_user failed when copying multiple doomdev_fill_rect from user\n");
        err = -EFAULT;
        goto err_fill_rects_kmalloc_rects_to_fill;
    }

    mutex_lock(&frame->context->dev->device_lock);
    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < arg->rects_num; i++) {
        if (unlikely(
                rects_to_fill[i].pos_dst_x < 0
                || rects_to_fill[i].pos_dst_x + rects_to_fill[i].width > frame->width
                || rects_to_fill[i].pos_dst_y < 0
                || rects_to_fill[i].pos_dst_y + rects_to_fill[i].height > frame->height)) {
            pr_err("processing fill_rect failed - rectangle to fill is outside of frame\n");
            if (i == 0)
                err = -EINVAL;
            else
                err = i;
            goto err_fill_rects_unlock_mutex;
        }
        send_command(frame->context, HARDDOOM_CMD_XY_A(rects_to_fill[i].pos_dst_x, rects_to_fill[i].pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(rects_to_fill[i].color));
        send_command(frame->context, HARDDOOM_CMD_FILL_RECT(rects_to_fill[i].width, rects_to_fill[i].height));
    }

    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    mutex_unlock(&frame->context->dev->device_lock);
    kfree(rects_to_fill);

    return kernel_arg.rects_num;

err_fill_rects_unlock_mutex:
    mutex_unlock(&frame->context->dev->device_lock);
err_fill_rects_kmalloc_rects_to_fill:
    kfree(rects_to_fill);
    return err;
}

static long doom_frame_draw_lines(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines *arg)
{
    long err;
    uint16_t i;
    struct doomdev_surf_ioctl_draw_lines kernel_arg;
    struct doomdev_line *lines_to_draw;
    int fence_num;

    if (unlikely(copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_lines)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_draw_lines from user\n");
        return -EFAULT;
    }

    lines_to_draw = kmalloc(kernel_arg.lines_num * sizeof(struct doomdev_line), GFP_KERNEL);
    if (unlikely(!lines_to_draw)) {
        pr_err("kmalloc failed when allocating memory for doomdev_line structures\n");
        return -ENOMEM;
    }
    if (unlikely(
            copy_from_user(
                    lines_to_draw,
                    (void *) kernel_arg.lines_ptr,
                    kernel_arg.lines_num * sizeof(struct doomdev_line)))) {
        pr_err("copy_from_user failed when copying multiple doomdev_line from user\n");
        err = -EFAULT;
        goto err_draw_lines_kmalloc_lines_to_draw;
    }

    mutex_lock(&frame->context->dev->device_lock);
    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < kernel_arg.lines_num; i++) {
        if (unlikely(
                lines_to_draw[i].pos_a_x < 0
                || lines_to_draw[i].pos_a_x > frame->width
                || lines_to_draw[i].pos_a_y < 0
                || lines_to_draw[i].pos_a_y > frame->height
                || lines_to_draw[i].pos_b_x < 0
                || lines_to_draw[i].pos_b_x > frame->width
                || lines_to_draw[i].pos_b_y < 0
                || lines_to_draw[i].pos_b_y > frame->height)) {
            pr_err("processing draw_line failed - line_to_draw is outside of frame\n");
            if (i == 0)
                err = -EINVAL;
            else
                err = i;
            goto err_draw_lines_unlock_mutex;
        }
        send_command(frame->context, HARDDOOM_CMD_XY_A(lines_to_draw[i].pos_a_x, lines_to_draw[i].pos_a_y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(lines_to_draw[i].pos_b_x, lines_to_draw[i].pos_b_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(lines_to_draw[i].color));
        send_command(frame->context, HARDDOOM_CMD_DRAW_LINE);
    }

    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    mutex_unlock(&frame->context->dev->device_lock);
    kfree(lines_to_draw);

    return kernel_arg.lines_num;

err_draw_lines_unlock_mutex:
    mutex_unlock(&frame->context->dev->device_lock);
err_draw_lines_kmalloc_lines_to_draw:
    kfree(lines_to_draw);
    return err;
}

static long doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background *arg)
{
    long err;
    struct file *flat_file;
    struct doomdev_surf_ioctl_draw_background kernel_arg;
    struct doom_flat_texture *texture;
    int fence_num;

    if (unlikely(copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_background)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_draw_background from user\n");
        return -EFAULT;
    }

    flat_file = fget(kernel_arg.flat_fd);
    if (IS_ERR(flat_file)) {
        pr_err("fget failed with code: %ld when getting flat texture for draw background operation\n",
               PTR_ERR(flat_file));
        return PTR_ERR(flat_file);
    }

    if (unlikely(flat_file->f_op != &doom_flat_texture_fops)) {
        pr_err("draw background operation was run with incorrect flat texture file descriptor\n");
        err = -EINVAL;
        goto err_draw_bcg_put_flat_file;
    }
    texture = flat_file->private_data;

    mutex_lock(&frame->context->dev->device_lock);
    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    send_command(frame->context, HARDDOOM_CMD_FLAT_ADDR(texture->ptr_dma));
    send_command(frame->context, HARDDOOM_CMD_DRAW_BACKGROUND);

    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;
    texture->last_fence = fence_num;
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    mutex_unlock(&frame->context->dev->device_lock);
    fput(flat_file);

    return 0;

err_draw_bcg_put_flat_file:
    fput(flat_file);
    return err;
}

static long doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns *arg)
{
    long err;
    uint16_t i;
    struct doomdev_surf_ioctl_draw_columns kernel_arg;
    struct doomdev_column *columns_to_draw;
    struct file *texture_file = NULL;
    struct file *colormap_file = NULL;
    struct file *color_translate_file = NULL;
    struct doom_col_texture *texture = NULL;
    struct doom_colormaps *colormaps_color = NULL;
    struct doom_colormaps *colormaps_transl = NULL;
    int fence_num;

    if (copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_columns))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_draw_columns from user\n");
        return -EFAULT;
    }

    columns_to_draw = kmalloc(kernel_arg.columns_num * sizeof(struct doomdev_column), GFP_KERNEL);
    if (unlikely(!columns_to_draw)) {
        pr_err("kmalloc failed when allocating memory for doomdev_column structures\n");
        return -ENOMEM;
    }
    if (unlikely(
            copy_from_user(
                    columns_to_draw,
                    (void *) kernel_arg.columns_ptr,
                    kernel_arg.columns_num * sizeof(struct doomdev_column)))) {
        pr_err("copy_from_user failed when copying multiple doomdev_column from user\n");
        err = -EFAULT;
        goto err_draw_columns_kmalloc_columns_to_draw;
    }

    mutex_lock(&frame->context->dev->device_lock);
    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;

    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    if (!(kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ)) {
        texture_file = fget(kernel_arg.texture_fd);
        if (IS_ERR(texture_file)) {
            pr_err("fget failed with code: %ld when getting column texture for draw columns operation\n",
                   PTR_ERR(texture_file));
            err = PTR_ERR(texture_file);
            texture_file = NULL;
            goto err_draw_columns_unlock_mutex;
        }
        if (unlikely(texture_file->f_op != &doom_col_texture_fops)) {
            pr_err("draw columns operation was run with incorrect column texture file descriptor\n");
            err = -EINVAL;
            goto err_draw_columns_fput_files;
        }
        texture = texture_file->private_data;
        texture->last_fence = fence_num;
        send_command(frame->context, HARDDOOM_CMD_TEXTURE_PT(texture->pt_dma_addr));
        send_command(frame->context, HARDDOOM_CMD_TEXTURE_DIMS(texture->rounded_texture_size, texture->height));
    }
    send_command(frame->context, HARDDOOM_CMD_DRAW_PARAMS(kernel_arg.draw_flags));
    if ((kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ) || (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP)) {
        colormap_file = fget(kernel_arg.colormaps_fd);
        if (IS_ERR(colormap_file)) {
            pr_err("fget failed with code: %ld when getting colormaps file for draw columns operation\n",
                   PTR_ERR(colormap_file));
            err = PTR_ERR(colormap_file);
            colormap_file = NULL;
            goto err_draw_columns_fput_files;
        }
        if (unlikely(colormap_file->f_op != &doom_colormaps_fops)) {
            pr_err("draw columns operation was run with incorrect colormaps file descriptor\n");
            err = -EINVAL;
            goto err_draw_columns_fput_files;
        }
        colormaps_color = colormap_file->private_data;
        colormaps_color->last_fence = fence_num;
    }
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
        color_translate_file = fget(kernel_arg.translations_fd);
        if (IS_ERR(color_translate_file)) {
            pr_err("fget failed with code: %ld when getting translate colormaps file for draw columns operation\n",
                   PTR_ERR(color_translate_file));
            err = PTR_ERR(color_translate_file);
            color_translate_file = NULL;
            goto err_draw_columns_fput_files;
        }
        if (unlikely(color_translate_file->f_op != &doom_colormaps_fops)) {
            pr_err("draw columns operation was run with incorrect translate colormaps file descriptor\n");
            err = -EINVAL;
            goto err_draw_columns_fput_files;
        }
        colormaps_transl = color_translate_file->private_data;
        colormaps_transl->last_fence = fence_num;
        send_command(frame->context,
                     HARDDOOM_CMD_TRANSLATION_ADDR(colormaps_transl->ptr_dma
                                                   + kernel_arg.translation_idx * HARDDOOM_COLORMAP_SIZE));
    }
    for (i = 0; i < kernel_arg.columns_num; i++) {
        if (unlikely(!(kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ)
                     && (columns_to_draw[i].y1 > columns_to_draw[i].y2
                         || (texture != NULL && columns_to_draw[i].texture_offset >= texture->rounded_texture_size)))) {
            pr_err("draw columns operation was run with incorrect parameters! either y1 > y2 or texture offset is outside texture\n");
            if (i == 0)
                err = -EINVAL;
            else
                err = i;
            goto err_draw_columns_fput_files;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(columns_to_draw[i].x, columns_to_draw[i].y1));
        send_command(frame->context, HARDDOOM_CMD_XY_B(columns_to_draw[i].x, columns_to_draw[i].y2));

        if ((kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ) || (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP))
            send_command(frame->context,
                         HARDDOOM_CMD_COLORMAP_ADDR(colormaps_color->ptr_dma
                                                    + columns_to_draw[i].colormap_idx * HARDDOOM_COLORMAP_SIZE));

        if (!(kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ)) {
            send_command(frame->context, HARDDOOM_CMD_USTART(columns_to_draw[i].ustart));
            send_command(frame->context, HARDDOOM_CMD_USTEP(columns_to_draw[i].ustep));
        }
        send_command(frame->context, HARDDOOM_CMD_DRAW_COLUMN(columns_to_draw[i].texture_offset));
    }
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    if (colormap_file != NULL)
        fput(colormap_file);
    if (texture_file != NULL)
        fput(texture_file);
    if (color_translate_file != NULL)
        fput(color_translate_file);
    mutex_unlock(&frame->context->dev->device_lock);
    kfree(columns_to_draw);

    return kernel_arg.columns_num;

err_draw_columns_fput_files:
    if (texture_file != NULL)
        fput(texture_file);
    if (colormap_file != NULL)
        fput(colormap_file);
    if (color_translate_file != NULL)
        fput(color_translate_file);
err_draw_columns_unlock_mutex:
    mutex_unlock(&frame->context->dev->device_lock);
err_draw_columns_kmalloc_columns_to_draw:
    kfree(columns_to_draw);
    return err;
}

static long doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans *arg)
{
    long err;
    uint16_t i;
    struct doomdev_surf_ioctl_draw_spans kernel_arg;
    struct doomdev_span *spans_to_draw;
    struct file *flat_texture_file = NULL;
    struct file *colormap_file = NULL;
    struct file *translate_file = NULL;
    struct doom_flat_texture *texture = NULL;
    struct doom_colormaps *colormaps_color = NULL;
    struct doom_colormaps *colormaps_transl;
    int fence_num;

    if (unlikely(copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_spans)))) {
        pr_err("copy_from_user failed when copying argument of type doomdev_surf_ioctl_draw_spans from user\n");
        return -EFAULT;
    }

    spans_to_draw = kmalloc(kernel_arg.spans_num * sizeof(struct doomdev_span), GFP_KERNEL);
    if (unlikely(!spans_to_draw)) {
        pr_err("kmalloc failed when allocating memory for doomdev_span structures\n");
        return -ENOMEM;
    }
    if (unlikely(copy_from_user(spans_to_draw,
                                (void *) kernel_arg.spans_ptr,
                                kernel_arg.spans_num * sizeof(struct doomdev_span)))) {
        pr_err("copy_from_user failed when copying multiple doomdev_span from user\n");
        err = -EFAULT;
        goto err_draw_spans_kmalloc_spans_to_draw;
    }

    mutex_lock(&frame->context->dev->device_lock);
    fence_num = atomic64_add_return(1, &frame->context->dev->op_counter);
    frame->last_fence = fence_num;

    if (frame->context->dev->last_dst_frame != frame->pt_dma_addr) {
        send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
        frame->context->dev->last_dst_frame = frame->pt_dma_addr;
    }
    flat_texture_file = fget(kernel_arg.flat_fd);
    if (IS_ERR(flat_texture_file)) {
        pr_err("fget failed with code: %ld when getting flat texture for draw spans operation\n",
               PTR_ERR(flat_texture_file));
        err = PTR_ERR(flat_texture_file);
        goto err_draw_spans_unlock_mutex;
    }
    if (unlikely(flat_texture_file->f_op != &doom_flat_texture_fops)) {
        pr_err("draw spans operation was run with incorrect flat texture file descriptor\n");
        err = -EINVAL;
        goto err_draw_spans_fput_flat_texture;
    }
    texture = flat_texture_file->private_data;
    texture->last_fence = fence_num;
    send_command(frame->context, HARDDOOM_CMD_FLAT_ADDR(texture->ptr_dma));
    send_command(frame->context, HARDDOOM_CMD_DRAW_PARAMS(kernel_arg.draw_flags));
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP) {
        colormap_file = fget(kernel_arg.colormaps_fd);
        if (IS_ERR(colormap_file)) {
            pr_err("fget failed with code: %ld when getting colormaps file for draw spans operation\n",
                   PTR_ERR(colormap_file));
            err = PTR_ERR(colormap_file);
            colormap_file = NULL;
            goto err_draw_spans_fput_flat_texture;
        }
        if (unlikely(colormap_file->f_op != &doom_colormaps_fops)) {
            pr_err("draw spans operation was run with incorrect colormaps file descriptor\n");
            err = -EINVAL;
            goto err_draw_spans_fput_colormap_textures;
        }
        colormaps_color = colormap_file->private_data;
        colormaps_color->last_fence = fence_num;
    }
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
        translate_file = fget(kernel_arg.translations_fd);
        if (IS_ERR(translate_file)) {
            pr_err("fget failed with code: %ld when getting colormaps translate file for draw spans operation\n",
                   PTR_ERR(translate_file));
            err = PTR_ERR(translate_file);
            translate_file = NULL;
            goto err_draw_spans_fput_colormap_textures;
        }
        if (unlikely(translate_file->f_op != &doom_colormaps_fops)) {
            pr_err("draw spans operation was run with incorrect colormaps translate file descriptor\n");
            err = -EINVAL;
            goto err_draw_spans_fput_colormap_textures;
        }
        colormaps_transl = translate_file->private_data;
        colormaps_transl->last_fence = fence_num;
        send_command(frame->context,
                     HARDDOOM_CMD_TRANSLATION_ADDR(colormaps_transl->ptr_dma
                                                   + kernel_arg.translation_idx * HARDDOOM_COLORMAP_SIZE));
    }

    for (i = 0; i < kernel_arg.spans_num; i++) {
        if (spans_to_draw[i].x1 > spans_to_draw[i].x2) {
            pr_err("draw spans operation was run with incorrect parameters! x1 > x2 for one of the spans\n");
            if (i == 0)
                err = -EINVAL;
            else
                err = i;
            goto err_draw_spans_fput_colormap_textures;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(spans_to_draw[i].x1, spans_to_draw[i].y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(spans_to_draw[i].x2, spans_to_draw[i].y));
        if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP)
            send_command(frame->context,
                         HARDDOOM_CMD_COLORMAP_ADDR(colormaps_color->ptr_dma
                                                    + spans_to_draw[i].colormap_idx * HARDDOOM_COLORMAP_SIZE));
        send_command(frame->context, HARDDOOM_CMD_USTART(spans_to_draw[i].ustart));
        send_command(frame->context, HARDDOOM_CMD_USTEP(spans_to_draw[i].ustep));
        send_command(frame->context, HARDDOOM_CMD_VSTART(spans_to_draw[i].vstart));
        send_command(frame->context, HARDDOOM_CMD_VSTEP(spans_to_draw[i].vstep));
        send_command(frame->context, HARDDOOM_CMD_DRAW_SPAN);
    }
    send_command(frame->context, HARDDOOM_CMD_FENCE(fence_num & HARDDOOM_FENCE_MASK));

    // if there are some commands still in buffer, we should flush them
    flush_batch(frame->context);

    if (colormap_file != NULL)
        fput(colormap_file);
    if (translate_file != NULL)
        fput(translate_file);
    fput(flat_texture_file);
    mutex_unlock(&frame->context->dev->device_lock);
    kfree(spans_to_draw);

    return kernel_arg.spans_num;

err_draw_spans_fput_colormap_textures:
    if (colormap_file != NULL)
        fput(colormap_file);
    if (translate_file != NULL)
        fput(translate_file);
err_draw_spans_fput_flat_texture:
    fput(flat_texture_file);
err_draw_spans_unlock_mutex:
    mutex_unlock(&frame->context->dev->device_lock);
err_draw_spans_kmalloc_spans_to_draw:
    kfree(spans_to_draw);
    return err;
}