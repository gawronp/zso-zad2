#ifndef DOOM_RESOURCES_H
#define DOOM_RESOURCES_H

#include <linux/fs.h>

#include "doomdev.h"
#include "doom_common.h"

int create_frame_buffer(struct doom_context * context, struct doomdev_ioctl_create_surface * ptr);
int create_column_texture(struct doom_context * context, struct doomdev_ioctl_create_texture * ptr);
int create_flat_texture(struct doom_context * context, struct doomdev_ioctl_create_flat * ptr);
int create_colormaps_array(struct doom_context * context, struct doomdev_ioctl_create_colormaps * ptr);

long doom_frame_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int doom_frame_release(struct inode *ino, struct file *filep);
ssize_t doom_frame_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);

int doom_col_texture_release(struct inode *ino, struct file *filep);

int doom_flat_texture_release(struct inode *ino, struct file *filep);

int doom_colormaps_release(struct inode *ino, struct file *filep);

int doom_frame_copy_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_copy_rects *arg);
int doom_frame_fill_rects(struct doom_frame *frame, struct doomdev_surf_ioctl_fill_rects *arg);
int doom_frame_draw_line(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_lines *arg);
int doom_frame_draw_background(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_background *arg);
int doom_frame_draw_columns(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_columns *arg);
int doom_frame_draw_spans(struct doom_frame *frame, struct doomdev_surf_ioctl_draw_spans *arg);

#endif