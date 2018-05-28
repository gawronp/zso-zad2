#ifndef DOOM_RESOURCES_H
#define DOOM_RESOURCES_H

#include <linux/fs.h>

#include "doomdev.h"
#include "doom_common.h"

long create_frame_buffer(struct doom_context * context, struct doomdev_ioctl_create_surface * ptr);
long create_column_texture(struct doom_context * context, struct doomdev_ioctl_create_texture * ptr);
long create_flat_texture(struct doom_context * context, struct doomdev_ioctl_create_flat * ptr);
long create_colormaps_array(struct doom_context * context, struct doomdev_ioctl_create_colormaps * ptr);

#endif