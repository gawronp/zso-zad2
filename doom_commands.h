#include "harddoom.h"

#include "doom_common.h"

static doom_command_t get_draw_params(uint8_t is_fuzz, uint8_t is_translate, uint8_t is_colormap);
static doom_command_t get_uv_start();
static doom_command_t get_uv_step();
static doom_command_t get_draw_column();
static doom_command_t get_draw_span();

static void send_command(doom_context *context, doom_command_t comm);