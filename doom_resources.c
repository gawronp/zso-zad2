#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/anon_inodes.h>
#include <linux/uaccess.h>
#include <linux/file.h>

#include "harddoom.h"
#include "doom_commands.h"

#include "doom_resources.h"

static struct file_operations doom_frame_fops = {
        .owner = THIS_MODULE,
//        .llseek = doom_frame_llseek,
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

int create_frame_buffer(struct doom_context * context, struct doomdev_ioctl_create_surface * ptr)
{
    int i;
    int frame_fd;
    struct doom_frame *frame;
    struct doom_device *doomdev;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_surface kernel_arg;
    struct file *created_file;

    if (copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_surface))) {
        pr_err("1\n");
        return -EFAULT;
    }

    if (kernel_arg.width < DOOMDEV_SURFACE_MIN_WIDTH
        || kernel_arg.width % DOOMDEV_SURFACE_WIDTH_DIVISIBLE != 0
        || kernel_arg.height < DOOMDEV_SURFACE_MIN_HEIGHT) {
        pr_err("2\n");
        return -EINVAL;
    }

    if (DOOMDEV_SURFACE_MAX_WIDTH < kernel_arg.width || DOOMDEV_SURFACE_MAX_HEIGHT < kernel_arg.height) {
        pr_err("overflow dims frame buffer!\n");
        return -EOVERFLOW;
    }

    doomdev = context->dev;

    frame = kmalloc(sizeof(struct doom_frame), GFP_KERNEL);
    frame->context = context;
    frame->width = kernel_arg.width;
    frame->height = kernel_arg.height;
    frame->pages_count = roundup(kernel_arg.width * kernel_arg.height, HARDDOOM_PAGE_SIZE) / HARDDOOM_PAGE_SIZE;
    frame->page_table_size = roundup(frame->pages_count * sizeof(doom_dma_ptr_t), DOOMDEV_PT_ALIGN);

    // alloc page table:
    frame->pt_dma = dma_alloc_coherent(&doomdev->pdev->dev,
                                       frame->page_table_size,
                                       &dma_addr,
                                       GFP_KERNEL);
    frame->pt_dma_addr = (doom_dma_ptr_t) dma_addr;
    frame->pt_virt = kmalloc(frame->pages_count * sizeof(doom_dma_ptr_t *), GFP_KERNEL);

    if (unlikely(!frame->pt_dma || !frame->pt_dma_addr || !frame->pt_virt)) {
        pr_err("3\n");
        return -ENOMEM;
    }

    for (i = 0; i < frame->pages_count; i++) {
        frame->pt_virt[i] = dma_alloc_coherent(&doomdev->pdev->dev, HARDDOOM_PAGE_SIZE,
                                               &dma_addr, GFP_KERNEL);
        if (unlikely(!dma_addr)) {
            pr_err("4\n");
            return -ENOMEM;
        }
        frame->pt_dma[i] = (doom_dma_ptr_t) dma_addr | HARDDOOM_PTE_VALID;
    }
    frame_fd = anon_inode_getfd("frame", &doom_frame_fops, frame, 0);
    if (IS_ERR_VALUE((unsigned long) frame_fd)) {
        pr_err("anon_inode_getfd returned err: %d\n", frame_fd);
    }

    created_file = fget(frame_fd);
    created_file->f_mode |= FMODE_READ | FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
    fput(created_file);

    return frame_fd;
}

int create_column_texture(struct doom_context * context, struct doomdev_ioctl_create_texture * ptr)
{
    int i;
    int col_texture_fd;
    struct doom_col_texture *col_texture;
    struct doom_device *doomdev;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_texture kernel_arg;
    size_t to_copy;

    if (copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_texture))) {
        pr_err("1\n");
        return -EFAULT;
    }

    doomdev = context->dev;

    col_texture = kmalloc(sizeof(struct doom_col_texture), GFP_KERNEL);
    col_texture->context = context;
    col_texture->height = kernel_arg.height;
    col_texture->texture_size = kernel_arg.size;
    col_texture->rounded_texture_size = roundup(kernel_arg.size, DOOMDEV_COL_TEXTURE_MEM_ALIGN);
    col_texture->pages_count = roundup(col_texture->rounded_texture_size, HARDDOOM_PAGE_SIZE) / HARDDOOM_PAGE_SIZE;
    col_texture->page_table_size = roundup(col_texture->pages_count * sizeof(doom_dma_ptr_t), DOOMDEV_PT_ALIGN);

    col_texture->pt_dma = dma_alloc_coherent(&doomdev->pdev->dev,
                                             col_texture->page_table_size,
                                             &dma_addr,
                                             GFP_KERNEL);
    col_texture->pt_dma_addr = (doom_dma_ptr_t) dma_addr;
    col_texture->pt_virt = kmalloc(col_texture->pages_count * sizeof(doom_dma_ptr_t *), GFP_KERNEL);

    if (unlikely(!col_texture->pt_dma || !col_texture->pt_dma_addr || !col_texture->pt_virt)) {
        pr_err("3\n");
        return -ENOMEM; // TODO sonmething smarter
    }

    for (i = 0; i < col_texture->pages_count; i++) {
        col_texture->pt_virt[i] =
                dma_alloc_coherent(&doomdev->pdev->dev,
                                   min((size_t) HARDDOOM_PAGE_SIZE,
                                       (size_t) col_texture->rounded_texture_size - i * HARDDOOM_PAGE_SIZE),
                                   &dma_addr,
                                   GFP_KERNEL);
        if (unlikely(!dma_addr)) {
            pr_err("4\n");
            return -ENOMEM;
        }
        col_texture->pt_dma[i] = (doom_dma_ptr_t) dma_addr | HARDDOOM_PTE_VALID;
        to_copy = min((size_t) HARDDOOM_PAGE_SIZE, col_texture->texture_size - i * HARDDOOM_PAGE_SIZE);
        if (copy_from_user(col_texture->pt_virt[i] + i * HARDDOOM_PAGE_SIZE,
                           (void *) kernel_arg.data_ptr + i * HARDDOOM_PAGE_SIZE, to_copy)) {
            return -EFAULT;
        }
    }
    if (col_texture->texture_size != col_texture->rounded_texture_size) {
        // fill with zeros
        memset(col_texture->pt_virt[col_texture->pages_count - 1] + col_texture->texture_size % HARDDOOM_PAGE_SIZE,
               0,
               col_texture->rounded_texture_size - col_texture->texture_size);
    }
    col_texture_fd = anon_inode_getfd("col_texture", &doom_col_texture_fops, col_texture, 0);
    if (IS_ERR_VALUE((unsigned long) col_texture_fd)) {
        pr_err("anon inode getfd return err %d\n", col_texture_fd);
    }

    return col_texture_fd;
}

int create_flat_texture(struct doom_context * context, struct doomdev_ioctl_create_flat * ptr)
{
    int flat_texture_fd;
    struct doom_flat_texture *texture;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_flat kernel_arg;

    if (copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_flat))) {
        return -EFAULT;
    }

    texture = kmalloc(sizeof(struct doom_flat_texture), GFP_KERNEL);
    texture->context = context;

    texture->ptr_virt = dma_alloc_coherent(&context->dev->pdev->dev, HARDDOOM_FLAT_SIZE,
                                           &dma_addr, GFP_KERNEL);
    if (unlikely(!texture->ptr_virt || !dma_addr)) {
        return -ENOMEM;
    }
    texture->ptr_dma = dma_addr;

    if(copy_from_user(texture->ptr_virt, (void *) kernel_arg.data_ptr, HARDDOOM_FLAT_SIZE)) {
        return -EFAULT;
    }
    // TODO check for errors

    flat_texture_fd = anon_inode_getfd("flat_texture", &doom_flat_texture_fops, texture, 0);
    // TODO check for errors

    return flat_texture_fd;
}

int create_colormaps_array(struct doom_context * context, struct doomdev_ioctl_create_colormaps * ptr)
{
    int colormaps_array_fd;
    struct doom_colormaps *colormaps;
    dma_addr_t dma_addr;
    struct doomdev_ioctl_create_colormaps kernel_arg;

    if (copy_from_user(&kernel_arg, ptr, sizeof(struct doomdev_ioctl_create_colormaps))) {
        return -EFAULT;
    }

    colormaps = kmalloc(sizeof(struct doom_colormaps), GFP_KERNEL);
    colormaps->context = context;
    colormaps->count = kernel_arg.num;

    colormaps->ptr_virt = dma_alloc_coherent(&context->dev->pdev->dev, colormaps->count * HARDDOOM_COLORMAP_SIZE,
                                             &dma_addr, GFP_KERNEL);
    if (unlikely(!colormaps->ptr_virt || !dma_addr)) {
        return -ENOMEM;
    }
    colormaps->ptr_dma = dma_addr;

    if (copy_from_user(colormaps->ptr_virt, (void *) kernel_arg.data_ptr,
                       colormaps->count * HARDDOOM_COLORMAP_SIZE)) {
        return -EFAULT;
    }
    // TODO check for errors

    colormaps_array_fd = anon_inode_getfd("colormaps", &doom_colormaps_fops, colormaps, 0);
    // TODO check for errors

    return colormaps_array_fd;
}

long doom_frame_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case DOOMDEV_SURF_IOCTL_COPY_RECTS:
        return doom_frame_copy_rects(file->private_data, (struct doomdev_surf_ioctl_copy_rects *) arg);
    case DOOMDEV_SURF_IOCTL_FILL_RECTS:
        return doom_frame_fill_rects(file->private_data, (struct doomdev_surf_ioctl_fill_rects *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_LINES:
        return doom_frame_draw_line(file->private_data, (struct doomdev_surf_ioctl_draw_lines *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_BACKGROUND:
        return doom_frame_draw_background(file->private_data, (struct doomdev_surf_ioctl_draw_background *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_COLUMNS:
        return doom_frame_draw_columns(file->private_data, (struct doomdev_surf_ioctl_draw_columns *) arg);
    case DOOMDEV_SURF_IOCTL_DRAW_SPANS:
        return doom_frame_draw_spans(file->private_data, (struct doomdev_surf_ioctl_draw_spans *) arg);
    default:
        return -EINVAL;
    }
}

//loff_t doom_frame_llseek(struct file * filp, loff_t off, int whence) {
//    struct doom_frame *frame = filp->private_data;
//    loff_t newpos;
//
//    switch(whence) {
//        case 0: /* SEEK_SET */
//            newpos = off;
//            break;
//
//        case 1: /* SEEK_CUR */
//            newpos = filp->f_pos + off;
//            break;
//
//        case 2: /* SEEK_END */
//            newpos = frame->width * frame->height + off;
//            break;
//
//        default: /* can't happen */
//            return -EINVAL;
//    }
//    if (newpos<0)
//        return -EINVAL;
//
//    filp->f_pos = newpos;
//    return newpos;
//}

int doom_frame_release(struct inode *ino, struct file *filep)
{
    // TODO: PING_SYNC wait for completing everything
    int i;
    struct doom_frame *frame = filep->private_data;
    unsigned long spin_lock_flags;

    mutex_lock(&frame->context->dev->surface_lock);

    send_command(frame->context, HARDDOOM_CMD_PING_SYNC);
    spin_lock_irqsave(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
    while (!frame->context->dev->read_flag) {
        spin_unlock_irqrestore(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
        wait_event_interruptible(frame->context->dev->read_sync_wait, frame->context->dev->read_flag > 0);
        spin_lock_irqsave(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
    }
    frame->context->dev->read_flag = 0;
    spin_unlock_irqrestore(&frame->context->dev->read_flag_spinlock, spin_lock_flags);

    for (i = 0; i < frame->pages_count; i++) {
        dma_free_coherent(&frame->context->dev->pdev->dev, HARDDOOM_PAGE_SIZE,
                          frame->pt_virt[i], frame->pt_dma[i]);
    }
    dma_free_coherent(&frame->context->dev->pdev->dev, frame->page_table_size,
                      frame->pt_dma, frame->pt_dma_addr);
    kfree(frame->pt_virt);

    kfree(frame);
    // TODO : check for errors
    mutex_unlock(&frame->context->dev->surface_lock);

    return 0;
}

ssize_t doom_frame_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
    unsigned long not_copied;
    struct doom_frame *frame;
    size_t copied = 0;
    uint32_t page_num = (*offp) / HARDDOOM_PAGE_SIZE;
    int to_copy;
    unsigned long spin_lock_flags;

//    pr_err("starting doom_frame_read!!!\n");

    if (filp->f_op != &doom_frame_fops) {
        return -EINVAL; // TODO?
    }

    frame = filp->private_data;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_PING_SYNC);
    spin_lock_irqsave(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
    while (!frame->context->dev->read_flag) {
        spin_unlock_irqrestore(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
        wait_event_interruptible(frame->context->dev->read_sync_wait, frame->context->dev->read_flag > 0);
        spin_lock_irqsave(&frame->context->dev->read_flag_spinlock, spin_lock_flags);
    }
    frame->context->dev->read_flag = 0;
    spin_unlock_irqrestore(&frame->context->dev->read_flag_spinlock, spin_lock_flags);

    while (copied < count) {
        to_copy = min(count - copied, (size_t) (HARDDOOM_PAGE_SIZE - ((*offp) % HARDDOOM_PAGE_SIZE)));
        not_copied = copy_to_user(buff + copied, frame->pt_virt[page_num] + ((*offp) % HARDDOOM_PAGE_SIZE), to_copy);
//        pr_err("frame->pt_virt[page_num]: %x offp: %x count: %d to_copy: %d not copied: %d\n", frame->pt_virt[page_num], *offp, count, to_copy, not_copied);
        if (not_copied) {
            mutex_unlock(&frame->context->dev->surface_lock);
            return copied + to_copy - not_copied;
        }
        copied += to_copy;
        (*offp) += to_copy;
        page_num = (*offp) / HARDDOOM_PAGE_SIZE;
    }

    mutex_unlock(&frame->context->dev->surface_lock);
    return count;
}

int doom_col_texture_release(struct inode *ino, struct file *filep)
{
    // TODO: PING_SYNC wait for completing everything
    int i;
    struct doom_col_texture *col_texture = filep->private_data;

    for (i = 0; i < col_texture->pages_count; i++) {
        dma_free_coherent(&col_texture->context->dev->pdev->dev,
                          min((size_t) HARDDOOM_PAGE_SIZE, col_texture->rounded_texture_size - i * HARDDOOM_PAGE_SIZE),
                          col_texture->pt_virt[i],
                          col_texture->pt_dma[i]);
    }
    dma_free_coherent(&col_texture->context->dev->pdev->dev, col_texture->page_table_size,
                      col_texture->pt_dma, col_texture->pt_dma_addr);
    kfree(col_texture->pt_virt);

    kfree(col_texture);
    // TODO : check for errors

    return 0;


    return 0;
}

int doom_flat_texture_release(struct inode *ino, struct file *filep)
{
    struct doom_flat_texture *texture = filep->private_data;
    dma_free_coherent(&texture->context->dev->pdev->dev, HARDDOOM_PAGE_SIZE, texture->ptr_virt, texture->ptr_dma);

    kfree(texture);
    // TODO : check for errors

    return 0;
}

int doom_colormaps_release(struct inode *ino, struct file *filep)
{
    struct doom_colormaps *colormaps = filep->private_data;
    dma_free_coherent(&colormaps->context->dev->pdev->dev, colormaps->count * HARDDOOM_COLORMAP_SIZE,
                      colormaps->ptr_virt, colormaps->ptr_dma);

    kfree(colormaps);
    // TODO : check for errors

    return 0;
}

// TODO!!!! pointers "arg" are in user space!!!!

int doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects *arg)
{
    uint16_t i;
    struct file *src_file;
    struct doomdev_surf_ioctl_copy_rects kernel_arg;
    struct doom_frame *src_frame;
    struct doomdev_copy_rect current_copy;
    void *ptr;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_copy_rects));
    ptr = (void *) kernel_arg.rects_ptr;

    src_file = fget(kernel_arg.surf_src_fd);
    if (src_file->f_op != &doom_frame_fops) {
        // OOPS - the file is incorrect
        return -EINVAL;
    }
    src_frame = src_file->private_data;

    if (frame->width != src_frame->width || frame->height != src_frame->height)
        return -EINVAL;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_SRC_PT(src_frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    send_command(frame->context, HARDDOOM_CMD_INTERLOCK);
    for (i = 0; i < kernel_arg.rects_num; i++) {
        if (copy_from_user(&current_copy, ptr, sizeof(struct doomdev_copy_rect))) {
            fput(src_file);
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0) {
                // TODO normal error code!!
                return -1;
            } else {
                return i;
            }
        }
        ptr += sizeof(struct doomdev_copy_rect);

        if (current_copy.pos_dst_x < 0
            || current_copy.pos_dst_x + current_copy.width > frame->width
            || current_copy.pos_dst_y < 0
            || current_copy.pos_dst_y + current_copy.height > frame->height
            || current_copy.pos_src_x < 0
            || current_copy.pos_src_x + current_copy.width > src_frame->width
            || current_copy.pos_src_y  < 0
            || current_copy.pos_src_y + current_copy.height > src_frame->height) {
            fput(src_file);
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0)
                return -EINVAL;
            else
                return i;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(current_copy.pos_dst_x, current_copy.pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(current_copy.pos_src_x, current_copy.pos_src_y));
        send_command(frame->context, HARDDOOM_CMD_COPY_RECT(current_copy.width, current_copy.height));
    }
//    pr_err("FIFO free: %d\n", ioread32(frame->context->dev->bar0 + HARDDOOM_FIFO_FREE));

    fput(src_file);
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.rects_num;
}

int doom_frame_fill_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_fill_rects *arg)
{
    uint16_t i;
    struct doomdev_surf_ioctl_fill_rects kernel_arg;
    struct doomdev_fill_rect current_rect;
    void *ptr;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_fill_rects));
    ptr = (void *) kernel_arg.rects_ptr;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < arg->rects_num; i++) {
        if (copy_from_user(&current_rect, ptr, sizeof(struct doomdev_fill_rect))) {
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0)
                return -EFAULT; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_fill_rect);

        if (current_rect.pos_dst_x < 0
            || current_rect.pos_dst_x + current_rect.width > frame->width
            || current_rect.pos_dst_y < 0
            || current_rect.pos_dst_y + current_rect.height > frame->height) {
            pr_err("pos dst x: %d pos dst y: %d current height: %d current width: %d frame height: %d frame width: %d\n",
                   current_rect.pos_dst_x, current_rect.pos_dst_y, current_rect.height, current_rect.width,
                   frame->height, frame->width);
            mutex_unlock(&frame->context->dev->surface_lock);
            return -EINVAL;
        }
        send_command(frame->context, HARDDOOM_CMD_XY_A(current_rect.pos_dst_x, current_rect.pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(current_rect.color));
        send_command(frame->context, HARDDOOM_CMD_FILL_RECT(current_rect.width, current_rect.height));
    }
//    pr_err("FIFO free: %d\n", ioread32(frame->context->dev->bar0 + HARDDOOM_FIFO_FREE));
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.rects_num;
}

int doom_frame_draw_line(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines *arg)
{
    uint16_t i;
    struct doomdev_surf_ioctl_draw_lines kernel_arg;
    struct doomdev_line current_line;
    void *ptr;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_lines));
    ptr = (void *) kernel_arg.lines_ptr;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < kernel_arg.lines_num; i++) {
        if (copy_from_user(&current_line, ptr, sizeof(struct doomdev_line))) {
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0)
                return -1; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_line);

        if (current_line.pos_a_x < 0
            || current_line.pos_a_x > frame->width
            || current_line.pos_a_y < 0
            || current_line.pos_a_y > frame->height
            || current_line.pos_b_x < 0
            || current_line.pos_b_x > frame->width
            || current_line.pos_b_y < 0
            || current_line.pos_b_y > frame->height) {
            pr_err("current_line.pos_a_x: %d current_line.pos_a_y: %d current_line.pos_b_x: %d current_line.pos_b_y: %d frame->height: %d frame->width: %d\n",
                   current_line.pos_a_x, current_line.pos_a_y, current_line.pos_b_x, current_line.pos_b_y,
                   frame->height, frame->width);
            mutex_unlock(&frame->context->dev->surface_lock);
            return -EINVAL;
        }
        send_command(frame->context, HARDDOOM_CMD_XY_A(current_line.pos_a_x, current_line.pos_a_y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(current_line.pos_b_x, current_line.pos_b_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(current_line.color));
        send_command(frame->context, HARDDOOM_CMD_DRAW_LINE);
    }
//    pr_err("FIFO free: %d\n", ioread32(frame->context->dev->bar0 + HARDDOOM_FIFO_FREE));
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.lines_num;
}

int doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background *arg)
{
    struct file *flat_file;
    struct doomdev_surf_ioctl_draw_background kernel_arg;
    struct doom_flat_texture *texture;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_background));

    flat_file = fget(kernel_arg.flat_fd);
    if (flat_file->f_op != &doom_flat_texture_fops) {
        // OOPS - the file is incorrect
        return -EINVAL;
    }

    texture = flat_file->private_data;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    send_command(frame->context, HARDDOOM_CMD_FLAT_ADDR(texture->ptr_dma));
    send_command(frame->context, HARDDOOM_CMD_DRAW_BACKGROUND);
    mutex_unlock(&frame->context->dev->surface_lock);

    fput(flat_file);

    return 0;
}

int doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns *arg)
{
    uint16_t i;
    struct doomdev_surf_ioctl_draw_columns kernel_arg;
    struct doomdev_column current_collumn;
    struct doomdev_column prev_column;
    void *ptr;
    struct file *help_file = NULL;
    struct doom_col_texture *texture;
    struct doom_colormaps *colormaps_color = NULL;
    struct doom_colormaps *colormaps_transl;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_columns));
    ptr = (void *) kernel_arg.columns_ptr;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    if (!(kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ)) {
        help_file = fget(kernel_arg.texture_fd);
        // TODO check for errors
        texture = help_file->private_data;
        send_command(frame->context, HARDDOOM_CMD_TEXTURE_PT(texture->pt_dma_addr));
        send_command(frame->context,
                     HARDDOOM_CMD_TEXTURE_DIMS(texture->rounded_texture_size / DOOMDEV_COL_TEXTURE_MEM_ALIGN - 1,
                                               texture->height));
    }
    send_command(frame->context, HARDDOOM_CMD_DRAW_PARAMS(kernel_arg.draw_flags));
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ || kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP) {
        help_file = fget(kernel_arg.colormaps_fd);
        colormaps_color = help_file->private_data;
    }
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
        help_file = fget(kernel_arg.translations_fd);
        colormaps_transl = help_file->private_data;
        send_command(frame->context,
                     HARDDOOM_CMD_TRANSLATION_ADDR(colormaps_transl->ptr_dma
                                                   + kernel_arg.translation_idx * HARDDOOM_COLORMAP_SIZE));
    }
    for (i = 0; i < kernel_arg.columns_num; i++) {
        if (copy_from_user(&current_collumn, ptr, sizeof(struct doomdev_column))) {
            if (help_file != NULL)
                fput(help_file);
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0)
                return -EFAULT; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_column);

        if (current_collumn.y1 > current_collumn.y2) {
            if (help_file != NULL)
                fput(help_file);
            mutex_unlock(&frame->context->dev->surface_lock);
            return -EINVAL;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(current_collumn.x, current_collumn.y1));
        send_command(frame->context, HARDDOOM_CMD_XY_B(current_collumn.x, current_collumn.y2));
        if (colormaps_color != NULL && (i == 0 || prev_column.colormap_idx != current_collumn.colormap_idx)) {
            send_command(frame->context,
                         HARDDOOM_CMD_COLORMAP_ADDR(colormaps_color->ptr_dma
                                                    + current_collumn.colormap_idx * HARDDOOM_COLORMAP_SIZE));
        }
        if (!(kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ)) {
            send_command(frame->context, HARDDOOM_CMD_USTART(current_collumn.ustart));
            send_command(frame->context, HARDDOOM_CMD_USTEP(current_collumn.ustep));
        }
        send_command(frame->context, HARDDOOM_CMD_DRAW_COLUMN(current_collumn.texture_offset));
        prev_column = current_collumn;
    }
//    pr_err("COLUMN FIFO free: %d\n", ioread32(frame->context->dev->bar0 + HARDDOOM_FIFO_FREE));
    if (help_file != NULL)
        fput(help_file);
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.columns_num;
}

int doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans *arg)
{
    uint16_t i;
    struct doomdev_surf_ioctl_draw_spans kernel_arg;
    struct doomdev_span current_span;
    struct doomdev_span prev_span;
    void *ptr;
    struct file *help_file;
    struct doom_flat_texture *texture;
    struct doom_colormaps *colormaps_color = NULL;
    struct doom_colormaps *colormaps_transl;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_spans));
    ptr = (void *) kernel_arg.spans_ptr;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));

    help_file = fget(kernel_arg.flat_fd);
    texture = help_file->private_data;
    fput(help_file);
    send_command(frame->context, HARDDOOM_CMD_FLAT_ADDR(texture->ptr_dma));

    send_command(frame->context, HARDDOOM_CMD_DRAW_PARAMS(kernel_arg.draw_flags));
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP) {
        help_file = fget(kernel_arg.colormaps_fd);
        colormaps_color = help_file->private_data;
        fput(help_file);
    }
    if (kernel_arg.draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
        help_file = fget(kernel_arg.translations_fd);
        colormaps_transl = help_file->private_data;
        fput(help_file);
        send_command(frame->context,
                     HARDDOOM_CMD_TRANSLATION_ADDR(colormaps_transl->ptr_dma
                                                   + kernel_arg.translation_idx * HARDDOOM_COLORMAP_SIZE));
    }

    for (i = 0; i < kernel_arg.spans_num; i++) {
        if (copy_from_user(&current_span, ptr, sizeof(struct doomdev_span))) {
            mutex_unlock(&frame->context->dev->surface_lock);
            if (i == 0)
                return -EFAULT; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_span);

        if (current_span.x1 > current_span.x2) {
            mutex_unlock(&frame->context->dev->surface_lock);
            return -EINVAL;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(current_span.x1, current_span.y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(current_span.x2, current_span.y));
        if (colormaps_color != NULL && (i == 0 || current_span.colormap_idx != prev_span.colormap_idx)) {
            send_command(frame->context,
                         HARDDOOM_CMD_COLORMAP_ADDR(colormaps_color->ptr_dma
                                                    + current_span.colormap_idx * HARDDOOM_COLORMAP_SIZE));
        }
        send_command(frame->context, HARDDOOM_CMD_USTART(current_span.ustart));
        send_command(frame->context, HARDDOOM_CMD_USTEP(current_span.ustep));
        send_command(frame->context, HARDDOOM_CMD_VSTART(current_span.vstart));
        send_command(frame->context, HARDDOOM_CMD_VSTEP(current_span.vstep));
        send_command(frame->context, HARDDOOM_CMD_DRAW_SPAN);
        prev_span = current_span;
    }
//    pr_err("SPAN FIFO free: %d\n", ioread32(frame->context->dev->bar0 + HARDDOOM_FIFO_FREE));
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.spans_num;
}