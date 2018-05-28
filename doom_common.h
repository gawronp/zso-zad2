#ifndef DOOM_COMMON_H
#define DOOM_COMMON_H

#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>

#define DEVNAME "doom"

#define DOOMDEV_SURFACE_MIN_WIDTH 64
#define DOOMDEV_SURFACE_MAX_WIDTH 2048
#define DOOMDEV_SURFACE_WIDTH_DIVISIBLE 64
#define DOOMDEV_SURFACE_MIN_HEIGHT 1
#define DOOMDEV_SURFACE_MAX_HEIGHT 2048

#define DOOMDEV_PT_ALIGN 64
#define DOOMDEV_COL_TEXTURE_MEM_ALIGN 256

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
    void __iomem *bar0;

    struct kobject kobj; // for counting references
    struct semaphore kobj_semaphore; // for waiting untill references count = 0

    // device-wide lock, used to prevent race conditions
    struct mutex device_lock;

    // Following are used to wait for and notify about ping aysnc events.
    struct tasklet_struct tasklet_ping_async;
    wait_queue_head_t pong_async_wait;
    int commands_sent_since_last_ping_async;

    // Following are used to synchronize on fences - much faster than ping sync.
    struct tasklet_struct tasklet_fence;
    wait_queue_head_t fence_waitqueue;
    atomic64_t op_counter;
    spinlock_t fence_spinlock;

    // Doom device commands buffer.
    doom_command_t *buffer;
    doom_dma_ptr_t dma_buffer;
    int commands_space_left;
    int doom_buffer_pos_write;

    // Driver internal commands buffer, used to batch commands for performance.
    doom_command_t batch_buffer[BATCH_SIZE];
    int batch_size;

    /*
     * kept to optimize *_PT operations for frames, since they may rarely change
     * destination / source frame, let's not force device to reload
     * its caches / TLBs all the time
     */
    doom_dma_ptr_t last_dst_frame;
    doom_dma_ptr_t last_src_frame;
};

/*
 * context the HardDoom device was opened in, currently serves no purpose,
 * makes it easier to expand the driver with additional functionality
 */
struct doom_context {
    struct doom_device *dev;
};

// reperesents frame buffer file
struct doom_frame {
    struct doom_context *context;
    struct kobject *kobj;

    uint16_t width;
    uint16_t height;

    void **pt_virt; // virtual pointer to page table of virtual addresses
    doom_dma_ptr_t *pt_dma; // virtual pointer to page table of dma addresses
    doom_dma_ptr_t pt_dma_addr; // dma pointer to page table of dma addresses
    int pages_count;
    size_t page_table_size; // in bytes, should be aligned to 64 B

    int last_fence; // numer of last fence that frame was used before
};

// reperesents column texture file
struct doom_col_texture {
    struct doom_context *context;
    struct kobject *kobj;

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

// reperesents flat texture file
struct doom_flat_texture {
    struct doom_context *context;
    struct kobject *kobj;

    void *ptr_virt; // virtual pointer to texture
    doom_dma_ptr_t ptr_dma; // dma pointer to texture

    int last_fence;
};

// reperesents colormaps file
struct doom_colormaps {
    struct doom_context *context;
    struct kobject *kobj;

    int count;

    void *ptr_virt; // virtual pointer to colormaps
    doom_dma_ptr_t ptr_dma; // dma pointer to colormaps

    int last_fence;
};

#endif