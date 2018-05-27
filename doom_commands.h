#ifndef DOOM_COMMANDS_H
#define DOOM_COMMANDS_H

#include "harddoom.h"

#include "doom_common.h"

void flush_batch(struct doom_context *context);
void send_command(struct doom_context *context, doom_command_t comm);

#endif