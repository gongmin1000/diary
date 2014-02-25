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
#include <stdint.h>
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
enum PlaySpeed{
   PAUSE = 0,
   X1SPEED,
   X2SPEED,
   X3SPEED,
   X4SPEED,
   X8SPEED,
   ONE_STEP_SPEED
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
    /*
    *offset_type,跳转类型，0为按帧，1为按ms
    *offset,跳转量
    *whence，跳转方向,SEEK_SET, SEEK_CUR,  SEEK_END
    */
    void Seek(long offset,int offset_type,int whence);
    /*
    *speed,播放速度
    *orientation,播放方向,0前进，1后退
    */
    void SetPlaySpeed(PlaySpeed speed);
protected:
    void VideoDecode();
    void PicShow();
    void AudioDecode();
    void SubtitleDecode();
    void ReadPacket();
private:
    static int DecodeInterruptCB(void *ctx);
    //////////////////////////////////////////////////
    //播放控制
    ///////////////////////////////////////////////////
    //暂停数据包读取
    /*pthread_mutex_t _ReadPacketThreadPauseCondMutex;
    pthread_cond_t  _ReadPacketThreadPauseCond;
    //暂停视频解码
    pthread_mutex_t _VideoThreadPauseCondMutex;
    pthread_cond_t  _VideoThreadPauseCond;*/
    //暂停显示
    pthread_mutex_t _ShowPicThreadPauseCondMutex;
    pthread_cond_t  _ShowPicThreadPauseCond;
    //暂停音频解码
    /*pthread_mutex_t _AudioThreadPauseCondMutex;
    pthread_cond_t  _AudioThreadPauseCond;
    //暂停字幕
    pthread_mutex_t _SubThreadPauseCondMutex;
    pthread_cond_t  _SubThreadPauseCond;*/

    PlaySpeed       _PlaySpeed;

    bool            _PlayerStop;

    //////////////////////////////////////////////////
    //
    AVFormatContext *_FormatCtx;
    pthread_mutex_t _FormatCtxMutex;
    pthread_t       _ReadPacketThread;
    pthread_attr_t  _ReadPacketThreadAttr;
    pthread_mutex_t _ReadPacketCondMutex;
    pthread_cond_t  _ReadPacketCond;
    double          _StartPlayTime;
    int             _MaxPacketListLen;
 
    /////////////////////////////////////////////////
    //视频解码相关方法
    //////////////////////////////////////////////////
    double          _NexFrameTime;
    double          _PreFrameTime;
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
  
};

#endif //__DAWN_PLAYER_H__
