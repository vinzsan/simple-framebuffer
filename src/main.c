#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/time.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbit.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

typedef struct FrameBufferContext {
  uint32_t *fbmem;
  uint32_t *fbcached;
  int fd;
  
  struct fb_var_screeninfo vscreeninfo;
  struct fb_fix_screeninfo fscreeninfo;

  size_t screen_size;
} FrameBufferContext;

typedef struct FrameBufferVector2D {
  int x;
  int y;
} FrameBufferVector2D;

typedef struct FrameBufferTerminalSettings {
  struct termios old;
  struct termios new;

  int save_fd;
} FrameBufferTerminalSettings;

typedef struct FrameBufferRectangle {
  int x;
  int y;
  int width;
  int height;
} FrameBufferRectangle;

static void init_frame_context(FrameBufferContext *ctx){
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
}

static void *alloc_frame_buffer(FrameBufferContext *fctx){
  fctx->fbmem = (uint32_t *)mmap(NULL,fctx->screen_size,PROT_READ | PROT_WRITE,
      MAP_SHARED,fctx->fd,0);
  assert(fctx->fbmem != MAP_FAILED);

  fctx->fbcached = malloc(fctx->screen_size);
  assert(fctx->fbcached != NULL);
  return fctx->fbmem;
}

static void frame_context_cleanup(FrameBufferContext *ctx){
  if(ctx->fbcached) free(ctx->fbcached);
  if(ctx->fbmem){ munmap(ctx->fbmem,ctx->screen_size);}
  close(ctx->fd);
}

static uint32_t make_rgba_color(FrameBufferContext *ctx,uint8_t red,uint8_t green,uint8_t blue,uint8_t alpha){
  return (red << ctx->vscreeninfo.red.offset) | (green << ctx->vscreeninfo.green.offset) |
    (blue << ctx->vscreeninfo.blue.offset) | (alpha << ctx->vscreeninfo.transp.offset);
}

static void draw_rect2d(FrameBufferContext *ctx,int x,int y,int width,int heigt,uint32_t color){
  int stride = ctx->fscreeninfo.line_length / sizeof(uint32_t);

  for(int j = 0;j < heigt;j++){
    int pos_y = y + j;
    if(pos_y < 0 || pos_y >= ctx->vscreeninfo.yres) continue;
    for(int i = 0;i < width;i++){
      int pos_x = x + i;
      if(pos_x < 0 || pos_x >= ctx->vscreeninfo.xres) continue;

      ctx->fbcached[pos_y * stride + pos_x] = color;
    }
  }
}

static void draw_line2d(FrameBufferContext *ctx,FrameBufferVector2D *start_pos,FrameBufferVector2D *end_pos,
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

static int set_raw_terminal(FrameBufferTerminalSettings *fb_term){
  write(STDOUT_FILENO, "\e[?25l", 6);
  if(tcgetattr(fb_term->save_fd,&fb_term->old) < 0) return -1;
  fb_term->new = fb_term->old;

  fb_term->new.c_lflag &= ~(ECHO | ICANON);
  fb_term->new.c_oflag &= ~(OPOST);

  fb_term->new.c_cc[VMIN] = 0;
  fb_term->new.c_cc[VTIME] = 0;
  return tcsetattr(fb_term->save_fd,TCSANOW,&fb_term->new);
}

static int reset_terminal(FrameBufferTerminalSettings *fb_term){
  write(STDOUT_FILENO, "\e[?25h", 6);
  return tcsetattr(fb_term->save_fd,TCSANOW,&fb_term->old);
}

static void frame_context_present(FrameBufferContext *ctx){
  if(ctx->fbcached)
    memcpy(ctx->fbmem,ctx->fbcached,ctx->screen_size);
}

int main(int argc,char *argv[]){
  FrameBufferContext fbcontext = {0};
  init_frame_context(&fbcontext);
  fbcontext.fbmem = alloc_frame_buffer(&fbcontext);

  size_t stride = fbcontext.fscreeninfo.line_length / sizeof(uint32_t);
  size_t screen_size = fbcontext.vscreeninfo.yres * stride;

  FrameBufferVector2D start_pos = {100,100};
  FrameBufferVector2D end_pos = {300,300};

  struct timeval timestart_t,timenow_t = {0};
  int frames = 0;

  gettimeofday(&timestart_t,NULL);

  for(int i = 0;i < screen_size;i++){
      fbcontext.fbmem[i] = make_rgba_color(&fbcontext,0,0,200,255);
  }

  FrameBufferTerminalSettings term = {0};
  set_raw_terminal(&term);

  FrameBufferRectangle dst_box = {100,100,100,100};

  float speed = 150.0f;
  fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL,0) | O_NONBLOCK);

  while(1){
    gettimeofday(&timenow_t,NULL);
    double delta = (timenow_t.tv_sec - timestart_t.tv_sec) + 
      (timenow_t.tv_usec - timestart_t.tv_usec) / 1e6;

    timestart_t = timenow_t;

    for(int i = 0;i < screen_size;i++){
      fbcontext.fbcached[i] = make_rgba_color(&fbcontext,0,0,200,0);
    }

    draw_rect2d(&fbcontext, 100,100,800,600,make_rgba_color(&fbcontext, 0,150,100,0));
    draw_rect2d(&fbcontext, dst_box.x,dst_box.y,dst_box.width,dst_box.height,make_rgba_color(&fbcontext,255,0,0,0));
    uint32_t line_color = make_rgba_color(&fbcontext, 255,0,0,100);

    draw_line2d(&fbcontext,&start_pos,&end_pos,0,line_color);

    char event_key[2] = {0};
    read(STDIN_FILENO,event_key,sizeof(event_key));

    if(event_key[0] == 'q'){
      break;
    } else if(event_key[0] == 'w'){
      dst_box.y -= (int)(speed * delta);
    } else if(event_key[0] == 's'){
      dst_box.y += (int)(speed * delta);
    } else if(event_key[0] == 'd'){
      dst_box.x += (int)(speed * delta);
    } else if(event_key[0] == 'a') {
      dst_box.x -= (int)(speed * delta);
    }

    if(dst_box.x < 0) dst_box.x = 0;
    if(dst_box.x > fbcontext.vscreeninfo.xres - dst_box.width) dst_box.x = fbcontext.vscreeninfo.xres - dst_box.width;

    if(dst_box.y < 0) dst_box.y = 0;
    if(dst_box.y > fbcontext.vscreeninfo.yres - dst_box.height) dst_box.y = fbcontext.vscreeninfo.yres - dst_box.height;

    frame_context_present(&fbcontext);
    usleep(16666);
  }

  reset_terminal(&term);
  frame_context_cleanup(&fbcontext);
  return 0;
}
