#define DEVNAME "DoomDev"

#define DOOMDEV_SURFACE_MIN_WIDTH 64
#define DOOMDEV_SURFACE_MAX_WIDTH 2048
#define DOOMDEV_SURFACE_WIDTH_DIVISIBLE 64
#define DOOMDEV_SURFACE_MIN_HEIGHT 1
#define DOOMDEV_SURFACE_MAX_HEIGHT 2048

#define DOOMDEV_PAGE_SIZE 1<<12
#define DOOMDEV_PT_ALIGN 64

#define PING_ASYNC_MMIO_COMMANDS_SPAN 512/4

typedef uint32_t doom_dma_ptr_t;
typedef uint32_t doom_command_t;

struct doom_device {
    struct cdev cdev;
    struct pci_dev *pdev;
    struct device *dev;
    int minor;
//    struct dma_pool *dma_pool;
    spinlock_t surface_lock;
    spinlock_t mmio_lock;
    int fifo_ping_remaining;
    DECLARE_WAIT_QUEUE_HEAD(pong_async_wait);

    void __iomem *bar0;
};

struct doom_context {
    struct doom_device *dev;
};

struct doom_frame {
    doom_context *context;

    uint16_t width;
    uint16_t height;
    void **pt_virt;
    doom_dma_ptr_t *pt_dma;
    doom_dma_ptr_t pt_dma_addr;
//    doom_ptr_t *pages_dma;
    int pages_count;
};

struct doom_col_texture {
    doom_context *context;

};

struct doom_flat_texture {
    doom_context *context;

    void *ptr_virt;
    doom_dma_ptr_t ptr_dma;
};

struct doom_colormaps {
    doom_context *context;

    void *ptr_virt;
    doom_dma_ptr_t ptr_dma;

    int count;
};