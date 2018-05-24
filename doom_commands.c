#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include "doom_commands.h"

#define BITS_PER_COMMAND 32

int get_free_buff_size(struct doom_context *context) {
    size_t read;
    size_t write;

    read = ioread32(context->dev->bar0 + HARDDOOM_CMD_READ_PTR);
    write = ioread32(context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);

    if (read < write) {
        return DOOM_BUFFER_SIZE - ((write - read) / sizeof(doom_command_t));
    } else if (read == write) {
        return DOOM_BUFFER_SIZE;
    } else {
        return (read - write) / sizeof(doom_command_t);
    }
}

void send_command(struct doom_context *context, doom_command_t comm)
{
    /*uint32_t free_fifo_space;
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
    spin_unlock_irqrestore(&context->dev->mmio_lock, flags);*/


//    unsigned long flags;
//    spin_lock_irqsave(&context->dev->mmio_lock, flags);
//    while(ioread32(context->dev->bar0 + HARDDOOM_FIFO_FREE) < 5) {
//    }
//    iowrite32(comm, context->dev->bar0 + HARDDOOM_FIFO_SEND);
//    spin_unlock_irqrestore(&context->dev->mmio_lock, flags);



/*
    // NEW VERSION - WITH COMMAND READING BLOCK:
    unsigned long flags;
    struct completion ping_sync_event;
    doom_command_t to_enable;

    pr_err("Putting command: %x read: %x write: %x dma_addr: %x\n", comm,
           ioread32(context->dev->bar0 + HARDDOOM_CMD_READ_PTR),
           ioread32(context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR),
           context->dev->dma_buffer);
    pr_err("MAYBE? %x\n", *((doom_command_t *) context->dev->dma_buffer));
    spin_lock_irqsave(&context->dev->buffer_spinlock, flags);
    context->dev->buffer[sizeof(doom_command_t) * context->dev->doom_buffer_pos_write] = comm;
    pr_err("COMM SET: %x\n", context->dev->buffer[sizeof(doom_command_t) * context->dev->doom_buffer_pos_write]);
    context->dev->doom_buffer_pos_write += 1;
    if (context->dev->doom_buffer_pos_write >= DOOM_BUFFER_SIZE - 5) {
        init_completion(&ping_sync_event);
        context->dev->ping_sync_event = &ping_sync_event;

        context->dev->buffer[sizeof(doom_command_t) * context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_SYNC;
        context->dev->doom_buffer_pos_write += 1;
        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
        wait_for_completion(&ping_sync_event);

        to_enable = ioread32(context->dev->bar0 + HARDDOOM_ENABLE);
        iowrite32(to_enable & (~HARDDOOM_ENABLE_FETCH_CMD), context->dev->bar0 + HARDDOOM_ENABLE);

        context->dev->buffer[sizeof(doom_command_t) * context->dev->doom_buffer_pos_write] =
                HARDDOOM_CMD_JUMP(context->dev->dma_buffer);
        context->dev->doom_buffer_pos_write = 0;
        iowrite32(context->dev->dma_buffer, context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);

        iowrite32(to_enable | HARDDOOM_ENABLE_FETCH_CMD, context->dev->bar0 + HARDDOOM_ENABLE);

    } else {
        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
    }
    spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
    msleep(500);*/



    unsigned long flags;
    struct completion ping_async_event;
    doom_command_t to_enable;

//    pr_err("Putting command: %x read: %x write: %x dma_addr: %x\n", comm,
//           ioread32(context->dev->bar0 + HARDDOOM_CMD_READ_PTR),
//           ioread32(context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR),
//           context->dev->dma_buffer);
//    spin_lock_irqsave(&context->dev->buffer_spinlock, flags);

    if (context->dev->doom_buffer_pos_write % (DOOM_BUFFER_SIZE / 4) == 0) {
        context->dev->buffer[context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_ASYNC;
        context->dev->doom_buffer_pos_write += 1;
        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
    }

    if (get_free_buff_size(context) <= DOOM_BUFFER_CRIT_LOW_SIZE) {
        init_completion(&ping_async_event);
        // TODO
        iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
        if (get_free_buff_size(context) <+ DOOM_BUFFER_CRIT_LOW_SIZE) {
            context->dev->ping_async_event = &ping_async_event;
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR_ENABLE);
//            while(!completion_done(&ping_async_event))
//                try_wait_for_completion(&ping_async_event);
            wait_for_completion(&ping_async_event);
        }
    }

    context->dev->buffer[context->dev->doom_buffer_pos_write] = comm;
//    pr_err("COMM SET: %x\n", context->dev->buffer[context->dev->doom_buffer_pos_write]);
    context->dev->doom_buffer_pos_write += 1;
    if (context->dev->doom_buffer_pos_write >= DOOM_BUFFER_SIZE - 5) {
//        init_completion(&ping_sync_event);
//        context->dev->ping_sync_event = &ping_sync_event;

//        context->dev->buffer[context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_SYNC;
//        context->dev->doom_buffer_pos_write += 1;
//        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
//                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
//        wait_for_completion(&ping_sync_event);

        to_enable = ioread32(context->dev->bar0 + HARDDOOM_ENABLE);
        iowrite32(to_enable & (~HARDDOOM_ENABLE_FETCH_CMD), context->dev->bar0 + HARDDOOM_ENABLE);

        context->dev->buffer[context->dev->doom_buffer_pos_write] =
                HARDDOOM_CMD_JUMP(context->dev->dma_buffer);
        context->dev->doom_buffer_pos_write = 0;
        iowrite32(context->dev->dma_buffer, context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);

        iowrite32(to_enable | HARDDOOM_ENABLE_FETCH_CMD, context->dev->bar0 + HARDDOOM_ENABLE);

    } else {
        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
    }
//    spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
}