#ifndef BITFRAME_CONTEXT_H
#define BITFRAME_CONTEXT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include <sys/time.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbit.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "../include/stb_truetype.h"

typedef struct BitFrame_Context {
  uint32_t *fbmem;
  uint32_t *fbcached;
  int fd;

  struct fb_var_screeninfo vscreeninfo;
  struct fb_fix_screeninfo fscreeninfo;

  size_t screen_size;
  size_t fill_size;
} BitFrame_Context;

typedef struct BitFrame_Vector2D {
  int x;
  int y;
} BitFrame_Vector2D;

typedef struct BitFrame_TerminalSettings {
  struct termios old;
  struct termios new;

  int save_fd;
} BitFrame_TerminalSettings;

typedef struct BitFrame_Rectangle2D {
  int x;
  int y;
  int width;
  int height;
} BitFrame_Rectangle2D;

typedef struct BitFrame_SharedInstance {
  BitFrame_Context *shared_context_ref;

  /* saved = runtime,dll */
  uint32_t flags_shared;
} BitFrame_SharedInstance;

typedef struct BitFrame_Glyps
{
  uint8_t *bitmap;
  int width;
  int height;

  int x_offset;
  int y_offset;

  int advance;
} BitFrame_Glyps;

typedef struct BitFrame_FontInstance
{
  FILE *ttf_file;
  size_t length_file;

  uint8_t *ttf_buffer;
  stbtt_fontinfo font;
  float scale_font;

  BitFrame_Glyps glyph_cached[128];
} BitFrame_FontInstance;

extern void init_bit_frame_context(BitFrame_Context *fbctx);
extern void *alloc_bit_frame_buffer(BitFrame_Context *fctx);
extern void bit_frame_context_cleanup(BitFrame_Context *ctx);

extern uint32_t make_rgba_color(BitFrame_Context *ctx,uint8_t red,uint8_t green,uint8_t blue,uint8_t alpha);
extern void bit_frame_draw_rect2d(BitFrame_Context *ctx,int x,int y,int width,int height,uint32_t color);
extern void bit_frame_draw_line2d(BitFrame_Context *ctx,BitFrame_Vector2D *start_pos,BitFrame_Vector2D *end_pos,
    int size,uint32_t color);
extern void bit_frame_fill_color(BitFrame_Context *ctx,uint32_t color);

extern int bit_frame_set_raw_terminal(BitFrame_TerminalSettings *fb_term);
extern int bit_frame_reset_terminal(BitFrame_TerminalSettings *fb_term);

extern void bit_frame_context_present(BitFrame_Context *ctx);
extern int bit_frame_check_collision(BitFrame_Rectangle2D *a, BitFrame_Rectangle2D *b);

int bit_frame_init_load_font_instance(BitFrame_Context *ctx,BitFrame_FontInstance *bfont,const char *ttf_path,
  float ttf_size);
void bit_frame_load_font_to_cached(BitFrame_FontInstance *font_instance);
void bit_frame_draw_text(BitFrame_Context *fctx,BitFrame_FontInstance *font_instance,int x,int y,
  const char *text,uint32_t color);

#endif //BITFRAME_CONTEXT_H
