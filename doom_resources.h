#ifndef DOOMDEV_H
#define DOOMDEV_H

#include "doomdev.h"
#include "doom_common.h"

static int create_frame_buffer(struct doomdev_ioctl_create_surface * ptr);
static int create_column_texture(struct doomdev_ioctl_create_texture * ptr);
static int create_flat_texture(struct doomdev_ioctl_create_flat * ptr);
static int create_colormaps_array(struct doomdev_ioctl_create_colormaps * ptr);

static int doom_frame_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int doom_frame_release(struct inode *ino, struct file *filep);
static ssize_t doom_frame_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);

static struct file_operations doom_frame_fops = {
    .owner = THIS_MODULE,
    .release = doom_frame_release,
    .read = doom_frame_read,
    .unlocked_ioctl = doom_frame_ioctl,
    .compat_ioctl = doom_frame_ioctl,
};

static int doom_col_texture_release(struct inode *ino, struct file *filep);

static struct file_operations doom_col_texture_fops = {
    .owner = THIS_MODULE,
    .release = doom_col_texture_release,
};

static int doom_flat_texture_release(struct inode *ino, struct file *filep);

static struct file_operations doom_flat_texture_fops = {
    .owner = THIS_MODULE,
    .release = doom_flat_texture_release,
};

static int doom_colormaps_release(struct inode *ino, struct file *filep);

static struct file_operations doom_colormaps_fops = {
    .owner = THIS_MODULE,
    .release = doom_colormaps_release,
};

static int doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects *arg);
static int doom_frame_fill_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_fill_rects *arg);
static int doom_frame_draw_line(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines *arg);
static int doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background *arg);
static int doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns *arg);
static int doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans *arg);



#endif