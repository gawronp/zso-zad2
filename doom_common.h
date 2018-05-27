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

#define DOOM_BUFFER_SIZE (16 * 4096)
#define DOOM_ASYNC_FREQ (DOOM_BUFFER_SIZE / 16)
#define BATCH_SIZE 0x100

#define DOOM_BUFFER_CRIT_LOW_SIZE (BATCH_SIZE + 5)

typedef uint32_t doom_dma_ptr_t;
typedef uint32_t doom_command_t;

static DEFINE_IDR(global_fence);
static DEFINE_SPINLOCK(global_fence_spinlock);

struct doom_device {
    struct cdev cdev;
    struct pci_dev *pdev;
    struct device *dev;
    int minor;
//    struct dma_pool *dma_pool;
    struct mutex surface_lock;
    wait_queue_head_t read_sync_wait;
    spinlock_t read_flag_spinlock;
    int read_flag;
    spinlock_t mmio_lock;
    int fifo_ping_remaining;
    wait_queue_head_t pong_async_wait;

    spinlock_t tasklet_spinlock;
    struct tasklet_struct tasklet_ping_sync;
    struct tasklet_struct tasklet_ping_async;
    struct completion *ping_sync_event;
    struct completion *ping_async_event;

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

//    atomic_t commands_sent_since_last_ping_async;
//    atomic_t commands_space_left;

    int commands_sent_since_last_ping_async;
    int commands_space_left;
};

struct doom_context {
    struct doom_device *dev;
};

struct doom_frame {
    struct doom_context *context;

    uint16_t width;
    uint16_t height;
    void **pt_virt;
    doom_dma_ptr_t *pt_dma;
    doom_dma_ptr_t pt_dma_addr;
//    doom_ptr_t *pages_dma;
    int pages_count;
    size_t page_table_size;

//    rwlock_t frame_rw_lock;
    int last_fence;
    spinlock_t last_fence_spinlock;
};

struct doom_col_texture {
    struct doom_context *context;

    uint16_t height;

    int is_page_table_on_last_page;

    void **pt_virt;
    doom_dma_ptr_t *pt_dma;
    doom_dma_ptr_t pt_dma_addr;

    int pages_count;
    size_t page_table_size;
    size_t texture_size;
    size_t rounded_texture_size;

    int last_fence;
    spinlock_t last_fence_spinlock;
};

struct doom_flat_texture {
    struct doom_context *context;

    void *ptr_virt;
    doom_dma_ptr_t ptr_dma;

    int last_fence;
    spinlock_t last_fence_spinlock;
};

struct doom_colormaps {
    struct doom_context *context;

    void *ptr_virt;
    doom_dma_ptr_t ptr_dma;

    int count;

    int last_fence;
    spinlock_t last_fence_spinlock;
};

#endif