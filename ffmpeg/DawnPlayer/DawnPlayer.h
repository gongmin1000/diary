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
/*enum SampleFormat {
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

	AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
};*/

struct AudioParameter{
	//int _freq;
	int _channels;
	int _sample_rate;
	int64_t _channel_layout;
	//enum SampleFormat _fmt;

};

class DawnPlayer{
public:
  DawnPlayer();
  ~DawnPlayer();
  bool Init(char* Pathi);
  void GetAudioParameter(AudioParameter* parameter);
  void SetAudioCallBack(void* data,AudioCallBack callback);
  void Run();
protected:
  void VideoDecode();
  void AudioDecode();
  void SubtitleDecode();
private:
  static int DecodeInterruptCB(void *ctx);
  AudioCallBack   _AudioCallBack;
  void            *_AudioCallBackData;


  AVFormatContext *_FormatCtx;
  
  int             _videoStream;
  AVCodecContext  *_videoCodecCtx;
  AVCodec         *_videoCodec;
  AVFrame         *_VideoFrame;
  AVFrame         *pFrameRGB;
  
  int             _audioStream;
  AVCodecContext  *_audioCodecCtx;
  AVCodec         *_audioCodec;
  AVFrame         *_AudioFrame;
  
  
  int             _subtitleStream;
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
