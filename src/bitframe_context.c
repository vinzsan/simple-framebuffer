#include <fcntl.h>
#include <linux/input-event-codes.h>
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
#include <math.h>
#include <linux/input.h>

#define STB_TRUETYPE_IMPLEMENTATION
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
  void *shared_instance;
  uint32_t flags_shared;
} BitFrame_SharedInstance;

typedef struct BitFrame_Glyps {
  uint8_t *bitmap;
  int width;
  int height;

  int x_offset;
  int y_offset;

  int advance;
} BitFrame_Glyps;

typedef struct BitFrame_TextMetrics {
  int width;
  int height;
} BitFrame_TextMetrics;

typedef struct BitFrame_FontInstance {
  FILE *ttf_file;
  size_t length_file;

  uint8_t *ttf_buffer;
  stbtt_fontinfo font;
  float scale_font;

  BitFrame_Glyps glyph_cached[128];
} BitFrame_FontInstance;

typedef struct BitFrame_MouseEvent {
  int x;
  int y;
  int left;
  int right;
  int middle;
  int fd;
} BitFrame_MouseEvent;

void init_bit_frame_context(BitFrame_Context *fbctx);
void *alloc_bit_frame_buffer(BitFrame_Context *fctx);
void bit_frame_context_cleanup(BitFrame_Context *ctx);

uint32_t make_rgba_color(BitFrame_Context *ctx,uint8_t red,uint8_t green,uint8_t blue,uint8_t alpha);
void bit_frame_draw_rect2d(BitFrame_Context *ctx,int x,int y,int width,int height,uint32_t color);
void bit_frame_draw_line2d(BitFrame_Context *ctx,BitFrame_Vector2D *start_pos,BitFrame_Vector2D *end_pos,
  int size,uint32_t color);
void bit_frame_fill_color(BitFrame_Context *ctx,uint32_t color);
BitFrame_TextMetrics bit_frame_measure_text(BitFrame_FontInstance *font,const char *__restrict__ text);
void bit_frame_close_font(BitFrame_FontInstance *font);

int bit_frame_set_raw_terminal(BitFrame_TerminalSettings *fb_term);
int bit_frame_reset_terminal(BitFrame_TerminalSettings *fb_term);

void bit_frame_context_present(BitFrame_Context *ctx);
int bit_frame_check_collision(BitFrame_Rectangle2D *a, BitFrame_Rectangle2D *b);

int bit_frame_init_mouse(const char *dev_name);
void bit_frame_poll_mouse(BitFrame_MouseEvent *mpack,int screen_w,int screen_h);

void init_bit_frame_context(BitFrame_Context *ctx){
  ctx->fd = open("/dev/fb0",O_RDWR);
  if(ctx->fd < 0) {
    fprintf(stderr,"ERR: failed open /dev/fb0\n");
    abort();
  }

  if(ioctl(ctx->fd,FBIOGET_VSCREENINFO,&ctx->vscreeninfo) < 0){
    fprintf(stderr,"ERR: failed get var screeninfo\n");
    abort();
  }

  if(ioctl(ctx->fd,FBIOGET_FSCREENINFO,&ctx->fscreeninfo) < 0){
    fprintf(stderr,"ERR: failed get fix screeninfo\n");
    abort();
  }

  ctx->screen_size = ctx->vscreeninfo.xres * ctx->vscreeninfo.yres *
    (ctx->vscreeninfo.bits_per_pixel >> 3);
  ctx->fill_size = ctx->fscreeninfo.line_length / sizeof(uint32_t)
    * ctx->vscreeninfo.yres;
}

void *alloc_bit_frame_buffer(BitFrame_Context *fctx){
  fctx->fbmem = (uint32_t *)mmap(NULL,fctx->screen_size,PROT_READ | PROT_WRITE,
      MAP_SHARED,fctx->fd,0);
  assert(fctx->fbmem != MAP_FAILED);

  fctx->fbcached = malloc(fctx->screen_size);
  assert(fctx->fbcached != NULL);
  return fctx->fbmem;
}

void bit_frame_context_cleanup(BitFrame_Context *ctx){
  if(ctx->fbcached) free(ctx->fbcached);
  if(ctx->fbmem){ munmap(ctx->fbmem,ctx->screen_size);}
  close(ctx->fd);
}

__attribute__((hot))
uint32_t make_rgba_color(BitFrame_Context *ctx,uint8_t red,uint8_t green,uint8_t blue,uint8_t alpha){
  return (red << ctx->vscreeninfo.red.offset) | (green << ctx->vscreeninfo.green.offset) |
    (blue << ctx->vscreeninfo.blue.offset) | (alpha << ctx->vscreeninfo.transp.offset);
}

void bit_frame_fill_color(BitFrame_Context *ctx,uint32_t color){
  memset(ctx->fbcached,color,ctx->screen_size);
}

__attribute__((hot))
void bit_frame_draw_rect2d(BitFrame_Context *ctx,int x,int y,int width,int height,uint32_t color){
  int stride = ctx->fscreeninfo.line_length / sizeof(uint32_t);

  for(int j = 0;j < height;j++){
    int pos_y = y + j;
    if(pos_y < 0 || pos_y >= ctx->vscreeninfo.yres) continue;
    for(int i = 0;i < width;i++){
      int pos_x = x + i;
      if(pos_x < 0 || pos_x >= ctx->vscreeninfo.xres) continue;

      ctx->fbcached[pos_y * stride + pos_x] = color;
    }
  }
}

void bit_frame_draw_line2d(BitFrame_Context *ctx,BitFrame_Vector2D *start_pos,BitFrame_Vector2D *end_pos,
    int size,uint32_t color){

  int stride = ctx->fscreeninfo.line_length / sizeof(uint32_t);

  int start_x_pos = start_pos->x;
  int start_y_pos = start_pos->y;

  int end_x_pos = end_pos->x;
  int end_y_pos = end_pos->y;

  int dx = abs(end_x_pos - start_x_pos);
  int dy = abs(end_y_pos - start_y_pos);

  int sx = (start_x_pos < end_x_pos) ? 1 : -1;
  int sy = (start_y_pos < end_y_pos) ? 1 : -1;
  int err = dx - dy;

  while(1){
    if(start_x_pos >= 0 && start_x_pos < ctx->vscreeninfo.xres && start_y_pos >= 0 &&
        start_y_pos < ctx->vscreeninfo.yres){
      int size = 10;
      ctx->fbcached[start_y_pos * stride + start_x_pos] = color;
    }

    if(start_x_pos == end_x_pos && start_y_pos == end_y_pos) break;
    int e2 = 2 * err;
    if(e2 > -dy) { err -=dy; start_x_pos += sx; }
    if(e2 < dx) { err += dx; start_y_pos += sy; }
  }
}

int bit_frame_set_raw_terminal(BitFrame_TerminalSettings *fb_term){
  write(STDOUT_FILENO, "\e[?25l", 6);
  if(tcgetattr(fb_term->save_fd,&fb_term->old) < 0) return -1;
  fb_term->new = fb_term->old;

  fb_term->new.c_lflag &= ~(ECHO | ICANON);
  fb_term->new.c_oflag &= ~(OPOST);

  fb_term->new.c_cc[VMIN] = 0;
  fb_term->new.c_cc[VTIME] = 0;
  return tcsetattr(fb_term->save_fd,TCSANOW,&fb_term->new);
}

int bit_frame_reset_terminal(BitFrame_TerminalSettings *fb_term){
  write(STDOUT_FILENO, "\e[?25h", 6);
  return tcsetattr(fb_term->save_fd,TCSANOW,&fb_term->old);
}

void bit_frame_context_present(BitFrame_Context *ctx){
  if(ctx->fbcached)
    memcpy(ctx->fbmem,ctx->fbcached,ctx->screen_size);
}

int bit_frame_check_collision(BitFrame_Rectangle2D *a, BitFrame_Rectangle2D *b){
  return (
    a->x < b->x + b->width &&
    a->x + a->width > b->x &&
    a->y < b->y + b->height &&
    a->y + a->height > b->y
  );
}

int bit_frame_init_load_font_instance(BitFrame_Context *ctx,BitFrame_FontInstance *bfont,const char *ttf_path,
  float ttf_size)
{
  bfont->ttf_file = fopen(ttf_path,"rb");
  assert(bfont->ttf_file);

  fseek(bfont->ttf_file,0,SEEK_END);
  bfont->length_file = ftell(bfont->ttf_file);
  fseek(bfont->ttf_file,0,SEEK_SET);

  bfont->ttf_buffer = malloc(bfont->length_file);
  if (!bfont->ttf_buffer)
  {
    fprintf(stderr,"ERR: failed to allocated ttf buffer\n");
    return -1;
  }

  size_t bytes_read = fread(bfont->ttf_buffer,sizeof(uint8_t),
    bfont->length_file,bfont->ttf_file);

  if (bfont->ttf_file) fclose(bfont->ttf_file);

  stbtt_InitFont(&bfont->font,bfont->ttf_buffer,0);

  bfont->scale_font = stbtt_ScaleForPixelHeight(&bfont->font,ttf_size);

  return 0;
}

void bit_frame_load_font_to_cached(BitFrame_FontInstance *font_instance)
{
  for (int ch = 32;ch < 128;ch++)
  {
    BitFrame_Glyps *current_glyph = &font_instance->glyph_cached[ch];
    current_glyph->bitmap = stbtt_GetCodepointBitmap(
      &font_instance->font,0,font_instance->scale_font,ch,&current_glyph->width,
      &current_glyph->height,&current_glyph->x_offset,&current_glyph->y_offset);

    int advance,lsb;
    stbtt_GetCodepointHMetrics(&font_instance->font,ch,&advance,&lsb);
    current_glyph->advance = (int)(advance * font_instance->scale_font);
  }
}

void bit_frame_draw_text(BitFrame_Context *fctx,BitFrame_FontInstance *font_instance,int x,int y,
  const char *text,uint32_t color)
{
  int _stride = fctx->fscreeninfo.line_length / sizeof(uint32_t);
  int cursor_x = x;

  for (int c = 0;text[c] != '\0';c++)
  {
    BitFrame_Glyps *g = &font_instance->glyph_cached[(int)text[c]];

    for (int j = 0;j < g->height;j++)
    {
      int pos_y = y + j + g->y_offset;
      if (pos_y < 0 || pos_y >= fctx->vscreeninfo.yres) continue;
      for (int i = 0;i < g->width;i++)
      {
        int pos_x = cursor_x + i + g->x_offset;
        if (pos_x < 0 || pos_x >= fctx->vscreeninfo.xres) continue;
        if (g->bitmap[j * g->width + i] > 0)
          fctx->fbcached[pos_y * _stride + pos_x] = color;
      }
    }
    cursor_x += g->advance;
  }
}

BitFrame_TextMetrics bit_frame_measure_text(BitFrame_FontInstance *font,const char *__restrict__ text){
  BitFrame_TextMetrics metrics = {0,0};
  for(int ch = 0;text[ch] != '\0';ch++){
    BitFrame_Glyps *g = &font->glyph_cached[(int)text[ch]];
    metrics.width += g->advance;
    int glyph_height = g->width + g->y_offset;
    if(glyph_height > metrics.height) metrics.height = glyph_height;
  }
  return metrics;
}

void bit_frame_close_font(BitFrame_FontInstance *font){
  if(font->ttf_buffer) free(font->ttf_buffer);
}

int bit_frame_init_mouse(const char *dev_name){
  int fd = open(dev_name,O_RDONLY | O_NONBLOCK);
  if(fd < 0){
    fprintf(stderr,"ERR: failed init mouse\n");
    return -1;
  }
  return fd;
}

void bit_frame_poll_mouse(BitFrame_MouseEvent *mpack,int screen_w,int screen_h){
  struct input_event ev;
  while(read(mpack->fd,&ev,sizeof(ev)) > 0){
    if(ev.type == EV_REL){
      if(ev.code == REL_X){
        mpack->x += ev.value;
        if(mpack->x < 0) mpack->x = 0;
        if(mpack->x >= screen_w) mpack->x = screen_w - 1;
      }
      if(ev.code == REL_Y){
        mpack->y += ev.value;
        if(mpack->y < 0) mpack->y = 0;
        if(mpack->y >= screen_h) mpack->y = screen_h - 1;
      }
    }

    if(ev.type == EV_KEY){
      int pressed = ev.value;
      if(ev.code == BTN_LEFT) mpack->left = pressed;
      if(ev.code == BTN_RIGHT) mpack->right = pressed;
      if(ev.code == BTN_MIDDLE) mpack->middle = pressed;
    }
  }
}
