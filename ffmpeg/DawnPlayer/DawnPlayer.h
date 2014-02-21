#ifndef __DAWN_PLAYER_H__
#define __DAWN_PLAYER_H__
#include <unistd.h>
#include <pthread.h>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avstring.h>
#include <libswresample/swresample.h>
#include <libavcodec/avfft.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

#include <list>
//#include <queue>
typedef void (*AudioCallBack)(void* prv_data,unsigned char* buf,int len);
typedef void (*VideoCallBack)(void* prv_data,AVFrame* frame);


struct AudioParameter{
	int _channels;
	int _sample_rate;
	int64_t _channel_layout;
	enum AVSampleFormat _fmt;

};
struct VideoParameter{
	int _width;
	int _height;
	
};

class DawnPlayer{
public:
  DawnPlayer();
  ~DawnPlayer();
  static void *VideoThreadRun(void* arg);
  static void *PicShowThreadRun(void* arg);
  static void *AudioThreadRun(void* arg);
  static void *SubThreadRun(void* arg);
  static void *ReadPacketThreadRun(void* arg);


  bool Init(char* Path);
  void GetAudioParameter(AudioParameter* parameter);
  void GetVideoParameter(VideoParameter* parameter);
  void SetAudioCallBack(void* data,AudioCallBack callback);
  void SetVideoCallBack(void* data,VideoCallBack callback);
  void Play();
protected:
  void VideoDecode();
  void PicShow();
  void AudioDecode();
  void SubtitleDecode();
  void ReadPacket();
private:
  static int DecodeInterruptCB(void *ctx);

  //////////////////////////////////////////////////
  //
  AVFormatContext *_FormatCtx;
  pthread_t       _ReadPacketThread;
  pthread_attr_t  _ReadPacketThreadAttr;
  pthread_mutex_t _ReadVideoPacketCondMutex;
  pthread_cond_t  _ReadVideoPacketCond;
  pthread_mutex_t _ReadAudioPacketCondMutex;
  pthread_cond_t  _ReadAudioPacketCond;
  double          _StartPlayTime;
  int             _MaxPacketListLen;
 
  /////////////////////////////////////////////////
  //视频解码相关方法
  //////////////////////////////////////////////////
  double          _NexFrameTime;
  int             _Fps;
  int             _FrameStepTime;
  VideoCallBack   _VideoCallBack;
  void            *_VideoCallBackPrvData;
  int             _VideoStream;
  AVCodecContext  *_VideoCodecCtx;
  AVCodec         *_VideoCodec;

  AVFrame         *_FrameRgb;
  int             _RgbNumBytes;
  uint8_t         *_RgbBuffer;
  struct SwsContext *_RgbConvertCtx;
  void            VideoConvertRgb(AVFrame *src_frame);
  AVFrame         *_FrameYuv;
  int             _YuvNumBytes;
  uint8_t         *_YuvBuffer;
  struct SwsContext *_YuvConvertCtx;
  void            VideoConvertYuv(AVFrame *src_frame);

  std::list<AVPacket>  _VideoPacketList;
  pthread_mutex_t _VideoPacketListMutex;
  pthread_t       _VideoThread;
  pthread_attr_t  _VideoThreadAttr;
  pthread_mutex_t _VideoThreadCondMutex;
  pthread_cond_t  _VideoThreadCond;

  double          _FrameVideoPts;
  double          _LastFrameVideoPts;
  double          _VideoClock;
  double          _CurVideoPts;

  /////////////////////////////////////////////////
  //视频显示部分
  ///////////////////////////////////////////////// 
  pthread_mutex_t _FreeFrameListMutex;
  std::list<AVFrame*> _FreeFrameList;
  pthread_mutex_t _ShowFrameListMutex;
  std::list<AVFrame*> _ShowFrameList;
  pthread_t       _PicShowThread;
  pthread_attr_t  _PicShowThreadAttr;
  pthread_mutex_t _PicShowThreadCondMutex;
  pthread_cond_t  _PicShowThreadCond;

  //////////////////////////////////////////////
  //声音相关方法 
  ///////////////////////////////////////////////
  AudioCallBack   _AudioCallBack;
  void            *_AudioCallBackPrvData;
  //返回声音同步率
  int SynchronizeAudio(int nb_samples);
  //计算声音pts
  void AudioPts();
  int64_t _AudioFrameNextPts;
  //将声音转化成硬件可支持格式
  void AudioConvert(unsigned char** buf,int &len);
  int             _AudioStream;
  AVCodecContext  *_AudioCodecCtx;
  AVCodec         *_AudioCodec;
  AVFrame         *_AudioFrame;
  AudioParameter  _AudioParameterSrc;
  AudioParameter  _AudioTgt;
  SwrContext      *_SwrCtx;
  unsigned char   *_AudioSwrBuf;
  unsigned int    _AudioSwrBufSize;
  std::list<AVPacket>  _AudioPacketList;
  pthread_mutex_t _AudioPacketListMutex;
  pthread_t       _AudioThread;
  pthread_attr_t  _AudioThreadAttr;
  pthread_mutex_t _AudioThreadCondMutex;
  pthread_cond_t  _AudioThreadCond;
  double          _FrameAudioPts;
  double          _AudioClock;
  double          _CurAudioPts;
 
  /////////////////////////////////////////////////
  //字幕相关方法
  ////////////////////////////////////////////////// 
  int             _SubtitleStream;
  AVCodecContext  *_SubtitleCodecCtx;
  AVCodec         *_SubtitleCodec;
  std::list<AVPacket>  _SubPacketList;
  pthread_mutex_t _SubPacketListMutex;
  pthread_t       _SubThread;
  pthread_attr_t  _SubThreadAttr;
  pthread_mutex_t _SubThreadCondMutex;
  pthread_cond_t  _SubThreadCond;

  
  int vframe_index;
  int aframe_index;
  int sframe_index;
public:
  
};

#endif //__DAWN_PLAYER_H__
