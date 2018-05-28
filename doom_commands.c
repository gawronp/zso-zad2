#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "doom_commands.h"

/*
 * Returns devices' free buffer space for device from passed context.
 *
 * @context The context of device the free space should be checked for
 */
static int get_free_buff_size(struct doom_context *context)
{
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

/*
 * Should be used only when holding device lock.
 * Moves commands from internal (driver) buffer into device buffer
 * and sets appriopriate device mmio register relating to commands block.
 * If command buffer allocated for device is full, it does enable PONG_ASYNC
 * interrupt and waits for it. Also puts PING_ASYNC instruction into device
 * command buffer approximately each (1/16th * device buffer size) commands.
 * When reaching end of device buffer, it disables device command block,
 * places JUMP instruction at current position that moves device buffer
 * READ_PTR to the beginning and enables device command block again.
 *
 * @context The context of device the internal buffer should be flushed for
 */
void flush_batch(struct doom_context *context)
{
    doom_command_t to_enable;
    int i;

    if (context->dev->batch_size == 0) {
        return;
    }

    if (context->dev->commands_sent_since_last_ping_async >= DOOM_ASYNC_FREQ) {
        context->dev->buffer[context->dev->doom_buffer_pos_write] = HARDDOOM_CMD_PING_ASYNC;
        context->dev->doom_buffer_pos_write += 1;
        context->dev->commands_space_left -= 1;
        context->dev->commands_sent_since_last_ping_async = 0;
    }

    if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {

        context->dev->commands_space_left = get_free_buff_size(context);

        if (context->dev->commands_space_left <= DOOM_BUFFER_CRIT_LOW_SIZE) {
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR);
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, context->dev->bar0 + HARDDOOM_INTR_ENABLE);

            while(get_free_buff_size(context) < DOOM_ASYNC_FREQ) {
                wait_event_interruptible(context->dev->pong_async_wait,
                                         get_free_buff_size(context) >= DOOM_ASYNC_FREQ);
            }

            context->dev->commands_space_left = get_free_buff_size(context);
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
}

/*
 * Should be used only when holding device lock (device for passed context).
 * Puts command into internal (driver) commands buffer.
 * When this operation fills the buffer, it triggers flush_batch().
 *
 * @context context of device it should be run on
 * @comm command to put into internal buffer
 */
void send_command(struct doom_context *context, doom_command_t comm)
{
    context->dev->batch_buffer[context->dev->batch_size] = comm;
    context->dev->batch_size += 1;

    if (context->dev->batch_size == BATCH_SIZE) {
        flush_batch(context);
    }
}