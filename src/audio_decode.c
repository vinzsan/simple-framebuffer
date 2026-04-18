#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/mman.h>

#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>

typedef struct BitFrame_AudioDecode {
  AVFormatContext *base_fmt;
  const AVCodec *codec;

  AVCodecContext codec_ctx;
} BitFrame_AudioDecode ;

int main(int argc,char *argv[]){
  return 0;
}
