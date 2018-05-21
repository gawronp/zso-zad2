#ifndef DOOM_COMMANDS_H
#define DOOM_COMMANDS_H

#include "harddoom.h"

#include "doom_common.h"

doom_command_t get_draw_params(uint8_t is_fuzz, uint8_t is_translate, uint8_t is_colormap);
doom_command_t get_uv_start(void);
doom_command_t get_uv_step(void);
doom_command_t get_draw_column(void);
doom_command_t get_draw_span(void);

void send_command(struct doom_context *context, doom_command_t comm);

#endif