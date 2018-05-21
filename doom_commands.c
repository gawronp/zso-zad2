#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/sched.h>

#include "doom_commands.h"

#define BITS_PER_COMMAND 32

doom_command_t get_draw_params(uint8_t is_fuzz, uint8_t is_translate, uint8_t is_colormap)
{
    return HARDDOOM_CMD_DRAW_PARAMS(is_fuzz | (is_translate << 1) | (is_colormap << 2));
}

doom_command_t get_uv_start()
{
    return 0;
}

doom_command_t get_uv_step()
{
    return 0;
}

doom_command_t get_draw_column()
{
    return 0;
}

doom_command_t get_draw_span()
{
    return 0;
}

void send_command(struct doom_context *context, doom_command_t comm)
{
    uint32_t free_fifo_space;
    uint32_t enabled_interrupts;

    spin_lock(&context->dev->mmio_lock);
    free_fifo_space = ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE);
    if (!free_fifo_space) {
        iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
        enabled_interrupts = ioread32(context->dev->bar0 + HARDDOOM_INTR_ENABLE);
        while (!(free_fifo_space = ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE))) {
            iowrite32(enabled_interrupts | HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
            spin_unlock(&context->dev->mmio_lock);
            wait_event_interruptible(context->dev->pong_async_wait, free_fifo_space > 0);
            spin_lock(&context->dev->mmio_lock);
        }
    }
    iowrite32(comm, context->dev->bar0 + HARDDOOM_FIFO_SEND);

    context->dev->fifo_ping_remaining = context->dev->fifo_ping_remaining - 1;
    if (!context->dev->fifo_ping_remaining) {
        context->dev->fifo_ping_remaining = PING_ASYNC_MMIO_COMMANDS_SPAN;
        spin_unlock(&context->dev->mmio_lock);
        send_command(context, HARDDOOM_CMD_PING_ASYNC);
    } else {
        spin_unlock(&context->dev->mmio_lock);
    }
}