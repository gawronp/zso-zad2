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
    int page_table_required_entries;
    dma_addr_t dma_addr;

    if (ptr == NULL
        || ptr->width < DOOMDEV_SURFACE_MIN_WIDTH
        || ptr->width % DOOMDEV_SURFACE_WIDTH_DIVISIBLE != 0
        || DOOMDEV_SURFACE_MIN_WIDTH < ptr->width
        || ptr->height < DOOMDEV_SURFACE_MIN_HEIGHT
        || DOOMDEV_SURFACE_MAX_HEIGHT < ptr->height)
        return -EINVAL;

    doomdev = context->dev;

    frame = kmalloc(sizeof(struct doom_frame), GFP_KERNEL);
    frame->context = context;
    frame->width = ptr->width;
    frame->height = ptr->height;
    frame->pages_count = roundup(ptr->width * ptr->height, HARDDOOM_PAGE_SIZE) / HARDDOOM_PAGE_SIZE;
    page_table_required_entries = roundup(frame->pages_count * sizeof(uint32_t), DOOMDEV_PT_ALIGN);

    /* alloc page table */
    frame->pt_dma = dma_alloc_coherent(doomdev->dev, page_table_required_entries * sizeof(doom_dma_ptr_t),
            &dma_addr, GFP_KERNEL);
    frame->pt_dma_addr = (doom_dma_ptr_t) dma_addr;
    frame->pt_virt = kmalloc(frame->pages_count * sizeof(void *), GFP_KERNEL);

    if (unlikely(!frame->pt_dma || !frame->pt_dma_addr || !frame->pt_virt)) {
        return -ENOMEM;
    }

    for (i = 0; i < frame->pages_count; i++) {
        frame->pt_virt[i] = dma_alloc_coherent(doomdev->dev, HARDDOOM_PAGE_SIZE * sizeof(doom_dma_ptr_t),
                                               &dma_addr, GFP_KERNEL);
        if (unlikely(!dma_addr)) {
            return -ENOMEM;
        }
        frame->pt_dma[i] = (dma_addr & HARDDOOM_PTE_PHYS_MASK) | HARDDOOM_PTE_VALID;
    }

    frame_fd = anon_inode_getfd("frame", &doom_frame_fops, frame, FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
    // TODO check for errors

    return frame_fd;
}

int create_column_texture(struct doom_context * context, struct doomdev_ioctl_create_texture * ptr)
{
    return 0;
}

int create_flat_texture(struct doom_context * context, struct doomdev_ioctl_create_flat * ptr)
{
    int flat_texture_fd;
    struct doom_flat_texture *texture;
    int copy_from_user_retval;
    dma_addr_t dma_addr;

    texture = kmalloc(sizeof(struct doom_flat_texture), GFP_KERNEL);
    texture->context = context;

    texture->ptr_virt = dma_alloc_coherent(context->dev->dev, HARDDOOM_FLAT_SIZE,
                                           &dma_addr, GFP_KERNEL);
    if (unlikely(!texture->ptr_virt || !dma_addr)) {
        return -ENOMEM;
    }
    texture->ptr_dma = dma_addr;

    copy_from_user_retval = copy_from_user(texture->ptr_virt, (void *) ptr->data_ptr, HARDDOOM_FLAT_SIZE);
    // TODO check for errors

    flat_texture_fd = anon_inode_getfd("flat_texture", &doom_flat_texture_fops, texture, 0);
    // TODO check for errors

    return flat_texture_fd;
}

int create_colormaps_array(struct doom_context * context, struct doomdev_ioctl_create_colormaps * ptr)
{
    int colormaps_array_fd;
    struct doom_colormaps *colormaps;
    int copy_from_user_retval;
    dma_addr_t dma_addr;

    colormaps = kmalloc(sizeof(struct doom_colormaps), GFP_KERNEL);
    colormaps->context = context;
    colormaps->count = ptr->num;

    colormaps->ptr_virt = dma_alloc_coherent(context->dev->dev, colormaps->count * HARDDOOM_COLORMAP_SIZE,
                                             &dma_addr, GFP_KERNEL);
    if (unlikely(!colormaps->ptr_virt || !dma_addr)) {
        return -ENOMEM;
    }
    colormaps->ptr_dma = dma_addr;

    copy_from_user_retval = copy_from_user(colormaps->ptr_virt, (void *) ptr->data_ptr,
                                           colormaps->count * HARDDOOM_COLORMAP_SIZE);
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

int doom_frame_release(struct inode *ino, struct file *filep)
{
    int i;
    struct doom_frame *frame = filep->private_data;

    for (i = 0; i < frame->pages_count; i++) {
        dma_free_coherent(frame->context->dev->dev, HARDDOOM_PAGE_SIZE * sizeof(doom_dma_ptr_t),
                          frame->pt_virt[i], frame->pt_dma[i]);
    }
    dma_free_coherent(frame->context->dev->dev, HARDDOOM_PAGE_SIZE * sizeof(doom_dma_ptr_t),
                      frame->pt_virt, frame->pt_dma_addr);

    kfree(frame);
    // TODO : check for errors

    return 0;
}

ssize_t doom_frame_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
    unsigned long not_copied;
    struct doom_frame *frame;
    size_t copied = 0;
    uint32_t page_num = (*offp) / HARDDOOM_PAGE_SIZE;

    if (filp->f_op != &doom_frame_fops)
        return -EINVAL; // TODO?

    frame = filp->private_data;
    not_copied = copy_to_user(buff, frame->pt_virt[page_num] + ((*offp) % HARDDOOM_PAGE_SIZE),
                              min(count, (size_t) (HARDDOOM_PAGE_SIZE - ((*offp) % HARDDOOM_PAGE_SIZE))));
    if (not_copied)
        return min(count, (size_t) (HARDDOOM_PAGE_SIZE - ((*offp) % HARDDOOM_PAGE_SIZE))) - not_copied;
    copied += min(count, (size_t) (HARDDOOM_PAGE_SIZE - ((*offp) % HARDDOOM_PAGE_SIZE)));
    page_num += 1;

    while (copied < count) {
        not_copied = copy_to_user(buff + copied,
                                  frame->pt_virt[page_num] + (*offp) + copied,
                                  min(count - copied, (size_t) HARDDOOM_PAGE_SIZE));
        if (not_copied)
            return copied + min(count - copied, (size_t) HARDDOOM_PAGE_SIZE) - not_copied;

        copied += min(count - copied, (size_t) HARDDOOM_PAGE_SIZE);
        page_num += 1;
    }

    return count;
}

int doom_col_texture_release(struct inode *ino, struct file *filep)
{
    return 0;
}

int doom_flat_texture_release(struct inode *ino, struct file *filep)
{
    struct doom_flat_texture *texture = filep->private_data;
    dma_free_coherent(texture->context->dev->dev, HARDDOOM_PAGE_SIZE, texture->ptr_virt, texture->ptr_dma);

    kfree(texture);
    // TODO : check for errors

    return 0;
}

int doom_colormaps_release(struct inode *ino, struct file *filep)
{
    struct doom_colormaps *colormaps = filep->private_data;
    dma_free_coherent(colormaps->context->dev->dev, colormaps->count * HARDDOOM_COLORMAP_SIZE,
                      colormaps->ptr_virt, colormaps->ptr_dma);

    kfree(colormaps);
    // TODO : check for errors

    return 0;
}

// TODO!!!! pointers "arg" are in user space!!!!

int doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects *arg)
{
    uint16_t i;
    struct fd fd_src;
    struct doomdev_surf_ioctl_copy_rects kernel_arg;
    struct doom_frame *src_frame;
    struct doomdev_copy_rect current_copy;
    void *ptr;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_copy_rects));
    ptr = (void *) kernel_arg.rects_ptr;

    fd_src = fdget(kernel_arg.surf_src_fd);
    if (fd_src.file->f_op != &doom_frame_fops) {
        // OOPS - the file is incorrect
        return -EINVAL;
    }
    src_frame = fd_src.file->private_data;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_INTERLOCK);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    send_command(frame->context, HARDDOOM_CMD_SURF_SRC_PT(src_frame->pt_dma_addr));

    for (i = 0; i < kernel_arg.rects_num; i++) {
        if (copy_from_user(&current_copy, ptr, sizeof(struct doomdev_copy_rect))) {
            if (i == 0) {
                // TODO normal error code!!
                return -1;
            } else {
                return i;
            }
        }
        ptr += sizeof(struct doomdev_copy_rect);

        if (current_copy.pos_dst_x + current_copy.height < 0
            || current_copy.pos_dst_x + current_copy.height >= frame->height
            || current_copy.pos_dst_y + current_copy.width < 0
            || current_copy.pos_dst_y + current_copy.width >= frame->width
            || current_copy.pos_src_x + current_copy.height < 0
            || current_copy.pos_src_x + current_copy.height >= src_frame->height
            || current_copy.pos_src_y + current_copy.width < 0
            || current_copy.pos_src_y + current_copy.width >= src_frame->width) {
            if (i == 0)
                return -EINVAL;
            else
                return i;
        }

        send_command(frame->context, HARDDOOM_CMD_XY_A(current_copy.pos_dst_x, current_copy.pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_XY_B(current_copy.pos_src_x, current_copy.pos_src_y));
        send_command(frame->context, HARDDOOM_CMD_COPY_RECT(current_copy.width, current_copy.height));
    }
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
            if (i == 0)
                return -1; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_fill_rect);

        if (current_rect.pos_dst_x + current_rect.height < 0
            || current_rect.pos_dst_x + current_rect.height >= frame->height
            || current_rect.pos_dst_y + current_rect.width < 0
            || current_rect.pos_dst_y + current_rect.width >= frame->width)
            return -EINVAL;
        send_command(frame->context, HARDDOOM_CMD_XY_A(current_rect.pos_dst_x, current_rect.pos_dst_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(current_rect.color));
        send_command(frame->context, HARDDOOM_CMD_FILL_RECT(current_rect.width, current_rect.height));
    }
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.rects_num;
}

int doom_frame_draw_line(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines *arg)
{
    uint16_t i;
    struct doomdev_surf_ioctl_draw_lines kernel_arg;
    struct doomdev_line current_line;
    void *ptr = (void *) arg->lines_ptr;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_lines));
    ptr = (void *) kernel_arg.lines_ptr;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    for (i = 0; i < kernel_arg.lines_num; i++) {
        if (copy_from_user(&current_line, ptr, sizeof(struct doomdev_line))) {
            if (i == 0)
                return -1; // TODO normal error code!!
            else
                return i;
        }
        ptr += sizeof(struct doomdev_line);

        if (current_line.pos_a_x < 0
            || current_line.pos_a_x >= frame->height
            || current_line.pos_a_y < 0
            || current_line.pos_a_y >= frame->width
            || current_line.pos_b_x < 0
            || current_line.pos_b_x >= frame->height
            || current_line.pos_b_y < 0
            || current_line.pos_b_y >= frame->width)
            return -EINVAL;
        send_command(frame->context, HARDDOOM_CMD_XY_A(current_line.pos_a_x, current_line.pos_a_y));
        send_command(frame->context, HARDDOOM_CMD_XY_A(current_line.pos_b_x, current_line.pos_b_y));
        send_command(frame->context, HARDDOOM_CMD_FILL_COLOR(current_line.color));
    }
    mutex_unlock(&frame->context->dev->surface_lock);

    return kernel_arg.lines_num;
}

int doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background *arg)
{
    struct fd flat_fd;
    struct doomdev_surf_ioctl_draw_background kernel_arg;
    struct doom_flat_texture *texture;

    copy_from_user(&kernel_arg, arg, sizeof(struct doomdev_surf_ioctl_draw_background));

    flat_fd = fdget(kernel_arg.flat_fd);
    if (flat_fd.file->f_op != &doom_flat_texture_fops) {
        // OOPS - the file is incorrect
        return -EINVAL;
    }

    texture = flat_fd.file->private_data;

    mutex_lock(&frame->context->dev->surface_lock);
    send_command(frame->context, HARDDOOM_CMD_SURF_DST_PT(frame->pt_dma_addr));
    send_command(frame->context, HARDDOOM_CMD_SURF_DIMS(frame->width, frame->height));
    send_command(frame->context, HARDDOOM_CMD_FLAT_ADDR(texture->ptr_dma));
    send_command(frame->context, HARDDOOM_CMD_DRAW_BACKGROUND);
    mutex_unlock(&frame->context->dev->surface_lock);

    return 0;
}

int doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns *arg)
{
    return 0;
}

int doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans *arg)
{
    return 0;
}