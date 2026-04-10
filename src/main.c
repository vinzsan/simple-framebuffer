#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "../include/bitframe_context.h"

static void second_box_instance(BitFrame_Context *ctx,BitFrame_Rectangle2D *box_first,BitFrame_Rectangle2D *box_second,int *point){
  if(bit_frame_check_collision(box_first,box_second)){
    box_second->x = rand() % (ctx->vscreeninfo.xres - box_second->width);
    box_second->y = rand() % (ctx->vscreeninfo.yres - box_second->height);
    *point += 1;
  }
}
/* configure test */
static void auto_box_instance(BitFrame_Context *ctx,BitFrame_Rectangle2D *box_first,BitFrame_Rectangle2D *box_second,int *point){
  if(bit_frame_check_collision(box_first,box_second)){
    box_second->x = rand() % (ctx->vscreeninfo.xres - box_second->width);
    box_second->y = rand() % (ctx->vscreeninfo.yres - box_second->height);
    *point += 1;
  }
}

int main(int argc,char *argv[]){
  BitFrame_Context fbcontext = {0};
  init_bit_frame_context(&fbcontext);
  fbcontext.fbmem = alloc_bit_frame_buffer(&fbcontext);

  size_t stride = fbcontext.fscreeninfo.line_length / sizeof(uint32_t);
  size_t screen_size = fbcontext.vscreeninfo.yres * stride;

  BitFrame_Vector2D start_pos = {100,100};
  BitFrame_Vector2D end_pos = {300,300};

  struct timeval timestart_t,timenow_t = {0};
  int frames = 0;

  srand(time(NULL));
  gettimeofday(&timestart_t,NULL);

  for(int i = 0;i < screen_size;i++){
      fbcontext.fbcached[i] = make_rgba_color(&fbcontext,0,0,200,255);
  }

  BitFrame_TerminalSettings term = {0};
  term.save_fd = STDIN_FILENO;
  bit_frame_set_raw_terminal(&term);

  BitFrame_Rectangle2D dst_box = {100,100,100,100};
  BitFrame_Rectangle2D dst_second_box = {
    .x = rand() % (fbcontext.vscreeninfo.xres - 100),
    .y = rand() % (fbcontext.vscreeninfo.yres - 100),
    .width = 100,
    .height = 100
  };
  BitFrame_Rectangle2D dst_third_box = {
    .x = rand() % (fbcontext.vscreeninfo.xres - 200),
    .y = rand() % (fbcontext.vscreeninfo.yres - 200),
    .width = 200,
    .height = 200,
  };

  BitFrame_FontInstance font_context = {0};
  if (bit_frame_init_load_font_instance(&fbcontext,&font_context,"./maple.ttf",30))
  {
    fprintf(stderr,"ERR: failed to load font\n");
    return -1;
  }

  bit_frame_load_font_to_cached(&font_context);

  float speed = 250.0f;
  fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL,0) | O_NONBLOCK);

  BitFrame_Vector2D frame_size = {
    .x = fbcontext.vscreeninfo.xres,
    .y = fbcontext.vscreeninfo.yres
  };

  BitFrame_Rectangle2D border_size = {
    .x = 100,
    .y = 100,
    .width = frame_size.x - (100 << 1),
    .height = frame_size.y - (100 << 1),
  };

  int point = 0;

  while(1){
    gettimeofday(&timenow_t,NULL);
    double delta = (timenow_t.tv_sec - timestart_t.tv_sec) + 
      (timenow_t.tv_usec - timestart_t.tv_usec) / 1e6;

    timestart_t = timenow_t;

    for(int i = 0;i < screen_size;i++){
      fbcontext.fbcached[i] = make_rgba_color(&fbcontext,0,0,200,0);
    }

    bit_frame_draw_rect2d(&fbcontext, border_size.x,border_size.y,border_size.width,border_size.height,
      make_rgba_color(&fbcontext, 0,0,100,0));

    bit_frame_draw_rect2d(&fbcontext, dst_second_box.x,dst_second_box.y,dst_second_box.width,dst_second_box.height,
        make_rgba_color(&fbcontext,100,100,100,0));

    bit_frame_draw_rect2d(&fbcontext, dst_third_box.x,dst_third_box.y,dst_third_box.width,dst_third_box.height,
        make_rgba_color(&fbcontext,0,0,0,0));

    bit_frame_draw_rect2d(&fbcontext, dst_box.x,dst_box.y,dst_box.width,dst_box.height,
      make_rgba_color(&fbcontext,255,0,0,0));
    uint32_t line_color = make_rgba_color(&fbcontext, 255,0,0,100);

    bit_frame_draw_line2d(&fbcontext,&start_pos,&end_pos,0,line_color);

    bit_frame_draw_text(&fbcontext,&font_context,100,100,"Test collision ke box",make_rgba_color(
      &fbcontext,0,0,0,0));

    char buffer_text[32];
    snprintf(buffer_text,sizeof(buffer_text),"Score kamu adalah : %d",point);
    bit_frame_draw_text(&fbcontext,&font_context,10,50,buffer_text,
        make_rgba_color(&fbcontext,0,0,0,0));

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

    auto_box_instance(&fbcontext, &dst_box,&dst_second_box,&point);
    second_box_instance(&fbcontext,&dst_box,&dst_third_box,&point);

    bit_frame_context_present(&fbcontext);
    usleep(16666);
  }

  bit_frame_reset_terminal(&term);
  bit_frame_context_cleanup(&fbcontext);
  return 0;
}
