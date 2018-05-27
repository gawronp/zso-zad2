#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>

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

void flush_batch(struct doom_context *context) {
    unsigned long flags;
//    struct completion *ping_async_event;
    doom_command_t to_enable;
    int i;

    spin_lock_irqsave(&context->dev->buffer_spinlock, flags);
    if (context->dev->batch_size == 0) {
        spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
        return;
    }

    if (context->dev->commands_sent_since_last_ping_async >= DOOM_BUFFER_SIZE / 4) {
        context->dev->buffer[context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_ASYNC;
        context->dev->doom_buffer_pos_write += 1;
        context->dev->commands_space_left -= 1;
        context->dev->commands_sent_since_last_ping_async = 0;
    }

    if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {
//        ping_async_event = kmalloc(sizeof(struct completion), GFP_KERNEL);

        context->dev->commands_space_left = get_free_buff_size(context);
        spin_lock_bh(&context->dev->tasklet_spinlock);
//        init_completion(ping_async_event);
        if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {
            // TODO
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);

//            context->dev->ping_async_event = ping_async_event;
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR_ENABLE);
            spin_unlock_bh(&context->dev->tasklet_spinlock);
////            while(wait_for_completion_interruptible(ping_async_event) != 0) {
////                if (completion_done(ping_async_event))
////                    break;
////            }
//            while(!completion_done(ping_async_event))
//                try_wait_for_completion(ping_async_event);
//            wait_for_completion(ping_async_event);
//            while(wait_for_completion_interruptible(ping_async_event) != 0) {
//                if (completion_done(ping_async_event))
//                    break;
//            }

            while(get_free_buff_size(context) < DOOM_BUFFER_SIZE / 4) {
                wait_event_interruptible(context->dev->pong_async_wait,
                                         get_free_buff_size(context) >= DOOM_BUFFER_SIZE / 4);
            }

//            kfree(ping_async_event);
            context->dev->commands_space_left = get_free_buff_size(context);
        } else {
            spin_unlock_bh(&context->dev->tasklet_spinlock);
        }
    }

    if (context->dev->doom_buffer_pos_write >= DOOM_BUFFER_SIZE - DOOM_BUFFER_CRIT_LOW_SIZE) {
        to_enable = ioread32(context->dev->bar0 + HARDDOOM_ENABLE);
        iowrite32(to_enable & (~HARDDOOM_ENABLE_FETCH_CMD), context->dev->bar0 + HARDDOOM_ENABLE);

        pr_err("JUMP TO THE BEGGINING OF BUFFER\n");
        context->dev->buffer[context->dev->doom_buffer_pos_write] =
                HARDDOOM_CMD_JUMP(context->dev->dma_buffer);
        context->dev->doom_buffer_pos_write = 0;
        context->dev->commands_space_left -= 1;
        context->dev->commands_sent_since_last_ping_async += 1;
        iowrite32(context->dev->dma_buffer, context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
        iowrite32(to_enable, context->dev->bar0 + HARDDOOM_ENABLE);
    } else {
        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
    }

    for (i = 0; i < context->dev->batch_size; i++) {
        context->dev->buffer[context->dev->doom_buffer_pos_write + i] = context->dev->batch_buffer[i];
    }
    context->dev->doom_buffer_pos_write += context->dev->batch_size;
    context->dev->commands_space_left -= context->dev->batch_size;
    context->dev->commands_sent_since_last_ping_async += context->dev->batch_size;
    context->dev->batch_size = 0;
    iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
              context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);

    spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
}

void send_command(struct doom_context *context, doom_command_t comm) {
    unsigned long flags;

    spin_lock_irqsave(&context->dev->buffer_spinlock, flags);
    context->dev->batch_buffer[context->dev->batch_size] = comm;
    context->dev->batch_size += 1;

//    spin_lock_irqsave(&context->dev->buffer_spinlock, flags);
    if (context->dev->batch_size == BATCH_SIZE) {
        spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
        flush_batch(context);
    } else {
        spin_unlock_irqrestore(&context->dev->buffer_spinlock, flags);
    }
}

//void send_command(struct doom_context *context, doom_command_t comm)
//{
//    unsigned long flags;
//    struct completion *ping_async_event;
//    doom_command_t to_enable;
//    int free_space_check;
//
//    context->dev->buffer[context->dev->doom_buffer_pos_write] = comm;
//    context->dev->doom_buffer_pos_write += 1;
////    atomic_dec(&context->dev->commands_space_left);
//    context->dev->commands_space_left -= 1;
//    context->dev->commands_sent_since_last_ping_async += 1;
//
//    if (context->dev->commands_sent_since_last_ping_async >= DOOM_BUFFER_SIZE / 4) {
//        context->dev->buffer[context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_ASYNC;
//        context->dev->doom_buffer_pos_write += 1;
//        context->dev->commands_space_left -= 1;
////        atomic_set(&context->dev->commands_sent_since_last_ping_async, 0);
//        context->dev->commands_sent_since_last_ping_async = 0;
//    }
//
//    if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {
//        ping_async_event = kmalloc(sizeof(struct completion), GFP_KERNEL);
//        init_completion(ping_async_event);
//
//        context->dev->commands_space_left = get_free_buff_size(context);
//        spin_lock_irqsave(&context->dev->tasklet_spinlock, flags);
//        if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {
//            // TODO
//            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
//
//            context->dev->ping_async_event = ping_async_event;
//            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR_ENABLE);
//            spin_unlock_irqrestore(&context->dev->tasklet_spinlock, flags);
//////            while(wait_for_completion_interruptible(ping_async_event) != 0) {
//////                if (completion_done(ping_async_event))
//////                    break;
//////            }
////            while(!completion_done(ping_async_event))
////                try_wait_for_completion(ping_async_event);
////            wait_for_completion(ping_async_event);
//            while(wait_for_completion_interruptible(ping_async_event) != 0) {
//                if (completion_done(ping_async_event))
//                    break;
//            }
//
//            kfree(ping_async_event);
//            context->dev->commands_space_left = get_free_buff_size(context);
//        } else {
//            spin_unlock_irqrestore(&context->dev->tasklet_spinlock, flags);
//        }
//    }
//
//
//    if (context->dev->doom_buffer_pos_write >= DOOM_BUFFER_SIZE - 5) {
//        to_enable = ioread32(context->dev->bar0 + HARDDOOM_ENABLE);
//        iowrite32(to_enable & (~HARDDOOM_ENABLE_FETCH_CMD), context->dev->bar0 + HARDDOOM_ENABLE);
//
//        pr_err("JUMP TO THE BEGGINING OF BUFFER\n");
//        context->dev->buffer[context->dev->doom_buffer_pos_write] =
//                HARDDOOM_CMD_JUMP(context->dev->dma_buffer);
//        context->dev->doom_buffer_pos_write = 0;
//        context->dev->commands_space_left -= 1;
//        context->dev->commands_sent_since_last_ping_async += 1;
//        iowrite32(context->dev->dma_buffer, context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
//        iowrite32(to_enable, context->dev->bar0 + HARDDOOM_ENABLE);
//
//    } else {
//        iowrite32(context->dev->dma_buffer + sizeof(doom_command_t) * context->dev->doom_buffer_pos_write,
//                  context->dev->bar0 + HARDDOOM_CMD_WRITE_PTR);
//    }
//}