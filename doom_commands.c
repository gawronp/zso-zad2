#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/sched.h>

#include "doom_commands.h"

#define BITS_PER_COMMAND 32

void send_command(struct doom_context *context, doom_command_t comm)
{
    uint32_t free_fifo_space;
    uint32_t enabled_interrupts;
    unsigned long flags;


    spin_lock_irqsave(&context->dev->mmio_lock, flags);
    while(!ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE)) {
    }
//    free_fifo_space = ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE);
//    if (!free_fifo_space) {
//        iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
//        enabled_interrupts = ioread32(context->dev->bar0 + HARDDOOM_INTR_ENABLE);
//        while (!(ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE))) {
//            iowrite32(enabled_interrupts | HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR_ENABLE);
//            spin_unlock_irqrestore(&context->dev->mmio_lock, flags);
//            wait_event_interruptible(context->dev->pong_async_wait, free_fifo_space > 0);
//            spin_lock_irqsave(&context->dev->mmio_lock, flags);
//        }
//    }

    iowrite32(comm, context->dev->bar0 + HARDDOOM_FIFO_SEND);

//    context->dev->fifo_ping_remaining = context->dev->fifo_ping_remaining - 1;
//    if (!context->dev->fifo_ping_remaining) {
//        context->dev->fifo_ping_remaining = PING_ASYNC_MMIO_COMMANDS_SPAN;
//        iowrite32(HARDDOOM_CMD_PING_ASYNC, context->dev->bar0 + HARDDOOM_FIFO_SEND);
//    }
    spin_unlock_irqrestore(&context->dev->mmio_lock, flags);
}