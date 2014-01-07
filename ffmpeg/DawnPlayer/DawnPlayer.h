#ifndef __DAWN_PLAYER_H__
#define __DAWN_PLAYER_H__

#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avstring.h>
#include <libswresample/swresample.h>
#include <libavcodec/avfft.h>
#include <libswscale/swscale.h>
}

class DawnPlayer{
public:
  DawnPlayer();
  ~DawnPlayer();
  bool Init(char* Path);
  void Run();
protected:
  void VideoDecode();
  void AudioDecode();
  void SubtitleDecode();
private:
  static int DecodeInterruptCB(void *ctx);
  AVFormatContext *pFormatCtx;
  
  int             videoStream;
  AVCodecContext  *_videoCodecCtx;
  AVCodec         *_videoCodec;
  AVFrame         *_VideoFrame;
  AVFrame         *pFrameRGB;
  
  int             audioStream;
  AVCodecContext  *_audioCodecCtx;
  AVCodec         *_audioCodec;
  AVFrame         *_AudioFrame;
  
  
  int             subtitleStream;
  AVCodecContext  *_subtitleCodecCtx;
  AVCodec         *_subtitleCodec;
  


  AVPacket        packet;

  int             frameFinished;

  int             numBytes;

  uint8_t         *buffer;
  
  
  int vframe_index;
  int aframe_index;
  int sframe_index;
public:
  boost::thread _thr;
  
};

#endif //__DAWN_PLAYER_H__
