#ifndef DOOM_COMMON_H
#define DOOM_COMMON_H

#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/interrupt.h>

#define DEVNAME "doom"

#define DOOMDEV_SURFACE_MIN_WIDTH 64
#define DOOMDEV_SURFACE_MAX_WIDTH 2048
#define DOOMDEV_SURFACE_WIDTH_DIVISIBLE 64
#define DOOMDEV_SURFACE_MIN_HEIGHT 1
#define DOOMDEV_SURFACE_MAX_HEIGHT 2048

#define DOOMDEV_PT_ALIGN 64
#define DOOMDEV_COL_TEXTURE_MEM_ALIGN 256

#define PING_ASYNC_MMIO_COMMANDS_SPAN 512/4

#define DOOM_BUFFER_SIZE 0x10000
#define DOOM_ASYNC_FREQ (DOOM_BUFFER_SIZE / 16)
#define BATCH_SIZE 0x100

#define DOOM_BUFFER_CRIT_LOW_SIZE (BATCH_SIZE + 5)

typedef uint32_t doom_dma_ptr_t;
typedef uint32_t doom_command_t;

struct doom_device {
    struct cdev cdev;
    struct pci_dev *pdev;
    struct device *dev;
    int minor;
    struct mutex device_lock;

    wait_queue_head_t pong_async_wait;

    spinlock_t tasklet_spinlock;
    struct tasklet_struct tasklet_ping_async;

    doom_command_t *buffer;
    doom_dma_ptr_t dma_buffer;
    int doom_buffer_pos_write;
    spinlock_t buffer_spinlock;

    void __iomem *bar0;

    uint64_t fence_last;
    atomic64_t op_counter;
    spinlock_t fence_spinlock;
    wait_queue_head_t fence_waitqueue;
    struct tasklet_struct tasklet_fence;

    doom_command_t batch_buffer[BATCH_SIZE];
    int batch_size;

    int commands_sent_since_last_ping_async;
    int commands_space_left;

    doom_dma_ptr_t last_dst_frame;
    doom_dma_ptr_t last_src_frame;
};

struct doom_context {
    struct doom_device *dev;
};

struct doom_frame {
    struct doom_context *context;

    uint16_t width;
    uint16_t height;

    void **pt_virt; // virtual pointer to page table of virtual addresses
    doom_dma_ptr_t *pt_dma; // virtual pointer to page table of dma addresses
    doom_dma_ptr_t pt_dma_addr; // dma pointer to page table of dma addresses
    int pages_count;
    size_t page_table_size; // in bytes, should be aligned to 64 B

    int last_fence; // numer of last fence that frame was used before
};

struct doom_col_texture {
    struct doom_context *context;

    uint16_t height;
    size_t texture_size;

    size_t rounded_texture_size; // rounded to 256 B, requirement of device
    int pages_count;
    size_t page_table_size;
    int is_page_table_on_last_page; // 1 if page table is placed right after last byte on last page, 0 if on another page
    doom_dma_ptr_t *pt_dma;
    doom_dma_ptr_t pt_dma_addr;
    void **pt_virt;

    int last_fence;
};

struct doom_flat_texture {
    struct doom_context *context;

    void *ptr_virt; // virtual pointer to texture
    doom_dma_ptr_t ptr_dma; // dma pointer to texture

    int last_fence;
};

struct doom_colormaps {
    struct doom_context *context;

    int count;

    void *ptr_virt; // virtual pointer to colormaps
    doom_dma_ptr_t ptr_dma; // dma pointer to colormaps

    int last_fence;
};

#endif