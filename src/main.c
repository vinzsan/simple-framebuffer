#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../include/bitframe_context.h"

enum SceneSwitch_index {
  MAIN,
  LOAD_INTEGRITY,
  SEGFAULT,
  NONE,
};

static void main_screen_instance(BitFrame_SharedInstance *ctx,BitFrame_FontInstance *font){
  uint32_t color = make_rgba_color(ctx->shared_context_ref,0,100,100,0);
  uint32_t color_black = make_rgba_color(ctx->shared_context_ref,0,0,0,0);

  for(int i = 0;i < ctx->shared_context_ref->fill_size;i++){
    ctx->shared_context_ref->fbcached[i] = color;
  } 
  bit_frame_draw_text(ctx->shared_context_ref,font,10,25,"Hello from main instance",color_black);
  
  struct {
    BitFrame_Vector2D *ref_1;
    BitFrame_Vector2D *ref_2;
  } shared = {ctx->shared_instance[0],ctx->shared_instance[1]};
  bit_frame_draw_rect2d(ctx->shared_context_ref,shared.ref_1->x,shared.ref_1->y,100,100,color_black);
  bit_frame_draw_rect2d(ctx->shared_context_ref,shared.ref_2->x,shared.ref_2->y,50,50,color_black);
}

static void load_screen_integrity(BitFrame_SharedInstance *ctx,BitFrame_FontInstance *font){
  uint32_t color = make_rgba_color(ctx->shared_context_ref,0,0,200,0);
  uint32_t color_black = make_rgba_color(ctx->shared_context_ref,0,0,0,0);
  uint32_t border_color = make_rgba_color(ctx->shared_context_ref,0,0,100,0);

  for(int i = 0;i < ctx->shared_context_ref->fill_size;i++){
    ctx->shared_context_ref->fbcached[i] = color;
  }
  int width = ctx->shared_context_ref->vscreeninfo.xres;
  int height = ctx->shared_context_ref->vscreeninfo.yres;

  int border_size = 50;
  int y_border = 70;

  BitFrame_Rectangle2D border = {
    .x = border_size,
    .y = y_border,
    .width = width - (2 * border_size),
    .height = height - y_border,
  };

  BitFrame_TextMetrics measure_text = bit_frame_measure_text(font, "Auto Transaction Machine");

  BitFrame_Vector2D text_pos = {
    (width - measure_text.width) / 2,
    y_border
  };
  bit_frame_draw_rect2d(ctx->shared_context_ref,border.x,border.y,border.width,border.height,border_color);
  bit_frame_draw_text(ctx->shared_context_ref,font,text_pos.x,text_pos.y,"Auto Transaction Machine",color_black);
}

static void segfault_screen(BitFrame_SharedInstance *sctx,BitFrame_FontInstance *font){
  uint32_t color = make_rgba_color(sctx->shared_context_ref,255,0,0,0);
  uint32_t black = make_rgba_color(sctx->shared_context_ref,0,0,0,0);

  for(int i = 0;i < sctx->shared_context_ref->fill_size;i++){
    sctx->shared_context_ref->fbcached[i] = color;
  }

  bit_frame_draw_text(sctx->shared_context_ref,font,100,100,"Segmentation fault [core dumped] :(",black);
}

int main(int argc,char *argv[]){
  BitFrame_Context fbcontext = {0};
  init_bit_frame_context(&fbcontext);

  fbcontext.fbmem = alloc_bit_frame_buffer(&fbcontext); 

  BitFrame_Vector2D start_pos = {100,100};
  BitFrame_Vector2D end_pos = {300,300};

  struct timeval timestart_t,timenow_t = {0};
  int frames = 0;

  srand(time(NULL));
  gettimeofday(&timestart_t,NULL);

  for(int i = 0;i < fbcontext.fill_size;i++){
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
    .x = 50,
    .y = 50,
    .width = frame_size.x - (50 << 1),
    .height = frame_size.y - (50 << 1),
  };

  uint32_t black_color = make_rgba_color(&fbcontext,0,0,0,0);
  uint32_t blue_color = make_rgba_color(&fbcontext,0,0,200,0);

  BitFrame_SharedInstance shared_runtime_pack = {0};
  shared_runtime_pack.shared_context_ref = &fbcontext;

  shared_runtime_pack.flags_shared &= ~(1 << 0);
  shared_runtime_pack.flags_shared |= (1 << 0);

  void (*packed[NONE])(BitFrame_SharedInstance *,BitFrame_FontInstance *) = {
    main_screen_instance,load_screen_integrity,segfault_screen
  };

  BitFrame_Vector2D vector_main_instance = {100,100};
  BitFrame_Vector2D vector_other_box = {200,400};

  shared_runtime_pack.shared_instance = malloc(sizeof(void*) * 5);
  assert(shared_runtime_pack.shared_instance != NULL && "ERR: failed alloc");

  shared_runtime_pack.shared_instance[0] = &vector_main_instance;
  shared_runtime_pack.shared_instance[1] = &vector_other_box;

  double default_speed = 5.0f;

  // disini 
  shared_runtime_pack.flags_shared &= ~(0xFF << 8);
  shared_runtime_pack.flags_shared |= ((uint32_t)MAIN << 8);

  BitFrame_MouseEvent mouse_event = {
    .x = fbcontext.vscreeninfo.xres / 2,
    .y = fbcontext.vscreeninfo.yres / 2,
  };
  mouse_event.fd = bit_frame_init_mouse("/dev/input/event4");

  int point = 0;

  while(shared_runtime_pack.flags_shared & (1 << 0)){
    gettimeofday(&timenow_t,NULL);
    double delta = (timenow_t.tv_sec - timestart_t.tv_sec) + 
      (timenow_t.tv_usec - timestart_t.tv_usec) / 1e6;

    timestart_t = timenow_t;

    for(int i = 0;i < fbcontext.fill_size;i++){
      fbcontext.fbcached[i] = blue_color;
    }

    bit_frame_draw_rect2d(&fbcontext, border_size.x,border_size.y,border_size.width,border_size.height,
      make_rgba_color(&fbcontext, 0,0,100,0));

    char event_key[64] = {0};

    int bytes = read(STDIN_FILENO,event_key,sizeof(event_key));

    BitFrame_Vector2D *ref = (BitFrame_Vector2D *)shared_runtime_pack.shared_instance[0];
    bit_frame_poll_mouse(&mouse_event,fbcontext.vscreeninfo.xres,fbcontext.vscreeninfo.yres);

    for(int i = 0;i < sizeof(event_key);i++){
      switch(event_key[i]){
        case 'q' : {
          shared_runtime_pack.flags_shared &= ~(1 << 0);
          break;
        }
        case '1' : {
          shared_runtime_pack.flags_shared &= ~(0xFF << 8);
          shared_runtime_pack.flags_shared |= ((uint32_t)MAIN << 8);
          break;
        }
        case '2' : {
          shared_runtime_pack.flags_shared &= ~(0xFF << 8);
          shared_runtime_pack.flags_shared |= ((uint32_t)LOAD_INTEGRITY << 8);
          break;
        }
        case '3' : {
          shared_runtime_pack.flags_shared &= ~(0xFF << 8);
          shared_runtime_pack.flags_shared |= ((uint32_t)SEGFAULT << 8);
          break;
        }
        // DO NOT use pollin keypressed here.
        default:
          break;
      } 
      if(((shared_runtime_pack.flags_shared >> 8) & 0xFF) == MAIN) {
        if(event_key[i] == 'w'){
          ref->y -= (int)(delta * speed);
        } else if(event_key[i] == 's'){
          ref->y += (int)(delta * speed);
        } else if(event_key[i] == 'd'){
          ref->x += (int)(delta * speed);
        } else if(event_key[i] == 'a'){
          ref->x -= (int)(delta * speed);
        }
  
      }
    }
 
    uint8_t index = (shared_runtime_pack.flags_shared >> 8) & 0xFF;
    packed[index](&shared_runtime_pack,&font_context);

    bit_frame_draw_rect2d(&fbcontext,mouse_event.x,mouse_event.y,50,50,black_color);
    if(mouse_event.left) bit_frame_draw_text(&fbcontext,&font_context,20,20,"Clicked",black_color);

    int zero = 0;
    ioctl(fbcontext.fd,FBIO_WAITFORVSYNC,&zero);

    bit_frame_context_present(&fbcontext);
    usleep(16666);
  }

  free(shared_runtime_pack.shared_instance);
  bit_frame_reset_terminal(&term);
  bit_frame_close_font(&font_context);
  bit_frame_context_cleanup(&fbcontext);
  return 0;
}
