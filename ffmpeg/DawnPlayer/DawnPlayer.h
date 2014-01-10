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
typedef void (*AudioCallBack)(void* prv_data,unsigned char* buf,int len);
typedef void (*VideoCallBack)(void* prv_data,AVFrame* frame);
enum {
	SYNC_AUDIO_MASTER, /* default choice */
	SYNC_VIDEO_MASTER,
	SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum SampleFormat {
	SAMPLE_FMT_NONE = -1,
	SAMPLE_FMT_U8,          ///< unsigned 8 bits
	SAMPLE_FMT_S16,         ///< signed 16 bits
	SAMPLE_FMT_S32,         ///< signed 32 bits
	SAMPLE_FMT_FLT,         ///< float
	SAMPLE_FMT_DBL,         ///< double

	SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
	SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
	SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
	SAMPLE_FMT_FLTP,        ///< float, planar
	SAMPLE_FMT_DBLP,        ///< double, planar

	SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
};

struct AudioParameter{
	int _channels;
	int _sample_rate;
	int64_t _channel_layout;
	enum SampleFormat _fmt;

};
struct VideoParameter{
	int _width;
	int _height;
	
};

class DawnPlayer{
public:
  DawnPlayer();
  ~DawnPlayer();
  bool Init(char* Pathi);
  void GetAudioParameter(AudioParameter* parameter);
  void GetVideoParameter(VideoParameter* parameter);
  void SetAudioCallBack(void* data,AudioCallBack callback);
  void SetVideoCallBack(void* data,VideoCallBack callback);
  void Run();
protected:
  void VideoDecode();
  void AudioDecode();
  void SubtitleDecode();
private:
  static int DecodeInterruptCB(void *ctx);


  AVFormatContext *_FormatCtx;
  
  /////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////
  VideoCallBack   _VideoCallBack;
  void            *_VideoCallBackPrvData;
  int             _videoStream;
  AVCodecContext  *_videoCodecCtx;
  AVCodec         *_videoCodec;
  AVFrame         *_VideoFrame;
  AVFrame         *_FrameRgb;
  int             _RgbNumBytes;
  uint8_t         *_RgbBuffer;
  void            SaveRgbImage();
  AVFrame         *_FrameYuv;
  int             _YuvNumBytes;
  uint8_t         *_YuvBuffer;
  void            VideoConvert();

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
  int             _audioStream;
  AVCodecContext  *_audioCodecCtx;
  AVCodec         *_audioCodec;
  AVFrame         *_AudioFrame;
  AudioParameter  _AudioParameterSrc;
  AudioParameter  _AudioTgt;
  SwrContext      *_SwrCtx;
  unsigned char   *_AudioSwrBuf;
  unsigned int    _AudioSwrBufSize;
  /*unsigned char   *_AudioOutBuf;
  unsigned int    _AudioOutBufSize;*/
  
  
  int             _subtitleStream;
  AVCodecContext  *_subtitleCodecCtx;
  AVCodec         *_subtitleCodec;
  


  AVPacket        packet;

  int             frameFinished;

  
  int vframe_index;
  int aframe_index;
  int sframe_index;
public:
  boost::thread _thr;
  
};

#endif //__DAWN_PLAYER_H__
