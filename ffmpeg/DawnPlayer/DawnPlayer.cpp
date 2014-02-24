 //g++ -g -o DawnPlayer -D__STDC_CONSTANT_MACROS   DawnPlayer.cpp -I/home/dongrui/program/ffmpeg/include -L/home/dongrui/program/ffmpeg/lib   -lavformat -lavcodec  -lavdevice  -lavfilter  -lavutil  -lswresample  -lswscale -lz -lpthread -lboost_thread-mt -lSDL2 
//-fpermissive /usr/lib/x86_64-linux-gnu/libpulse-simple.so /usr/lib/x86_64-linux-gnu/libpulse.so
#include <DawnPlayer.h>
static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)

{

  FILE *pFile;

  char szFilename[32];

  int y;

  

  sprintf(szFilename, "frame%d.ppm", iFrame);

  pFile = fopen(szFilename, "wb");

  if( !pFile )

    return;

  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  

  for( y = 0; y < height; y++ )

    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  

  fclose(pFile);

}

void print_error(const char *filename, int err)
{       
	char errbuf[128];
	const char *errbuf_ptr = errbuf;
 
	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
		errbuf_ptr = strerror(AVUNERROR(err));
		av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

DawnPlayer::DawnPlayer():
  _FormatCtx(NULL),_FrameVideoPts(0),_CurVideoPts(0),
  _FrameRgb(NULL),_RgbBuffer(NULL),_RgbConvertCtx(NULL),
  _FrameYuv(NULL),_YuvBuffer(NULL),_YuvConvertCtx(NULL),
  _VideoCodecCtx(NULL),_VideoCodec(NULL),
  _AudioCodecCtx(NULL),_AudioCodec(NULL),
  _SubtitleCodecCtx(NULL),_SubtitleCodec(NULL),
  _AudioFrame(NULL),_StartPlayTime(0),_FrameAudioPts(0),_CurAudioPts(0),
  _AudioCallBack(NULL),_AudioCallBackPrvData(NULL),
  _VideoCallBack(NULL),_VideoCallBackPrvData(NULL),
  _MaxPacketListLen(20),_VideoClock(0),_Fps(1000000),_FrameStepTime(40000),
  vframe_index(0),aframe_index(0),sframe_index(0){

    
}

DawnPlayer::~DawnPlayer(){
  
  if(_FrameRgb)
    av_frame_free(&_FrameRgb);

  if(_RgbBuffer)
    av_free(_RgbBuffer);

  if(_FrameYuv)
    av_frame_free(&_FrameYuv);

  if(_YuvBuffer)
    av_free(_YuvBuffer);

  if(_AudioFrame)
    av_frame_free(&_AudioFrame);

  if(_VideoCodecCtx)
    avcodec_close(_VideoCodecCtx);

  if(_FormatCtx)
    avformat_close_input(&_FormatCtx);
}

int DawnPlayer::DecodeInterruptCB(void *ctx){
  DawnPlayer *dp = (DawnPlayer*)ctx;
  //printf("DecodeInterrupt CallBack\n");
  return 0;
}

void DawnPlayer::GetAudioParameter(AudioParameter* parameter)
{
	if( !_AudioCodecCtx ){
		return ;
	}
	parameter->_channels = _AudioCodecCtx->channels;
	parameter->_sample_rate = _AudioCodecCtx->sample_rate;
	parameter->_channel_layout = _AudioCodecCtx->channel_layout;
	
	_AudioTgt._channels = _AudioCodecCtx->channels;
	_AudioTgt._sample_rate = _AudioCodecCtx->sample_rate;
	_AudioTgt._channel_layout = _AudioCodecCtx->channel_layout;
	_AudioTgt._fmt = AV_SAMPLE_FMT_S16;
}

void DawnPlayer::SetAudioCallBack(void* data,AudioCallBack callback)
{
  _AudioCallBack = callback;
  _AudioCallBackPrvData = data;
}

void DawnPlayer::GetVideoParameter(VideoParameter* parameter)
{
	parameter->_width = _VideoCodecCtx->width;
	parameter->_height = _VideoCodecCtx->height;
}

void DawnPlayer::SetVideoCallBack(void* data,VideoCallBack callback)
{
  _VideoCallBack = callback;
  _VideoCallBackPrvData = data;
}

void *DawnPlayer::VideoThreadRun(void* arg){
	DawnPlayer* s = (DawnPlayer*)arg;
	s->VideoDecode();
}

void *DawnPlayer::PicShowThreadRun(void* arg){
	DawnPlayer* s = (DawnPlayer*)arg;
	s->PicShow();
}

void *DawnPlayer::AudioThreadRun(void* arg){
	DawnPlayer* s = (DawnPlayer*)arg;
	s->AudioDecode();
}

void *DawnPlayer::SubThreadRun(void* arg){
	DawnPlayer* s = (DawnPlayer*)arg;
	s->SubtitleDecode();
}

void *DawnPlayer::ReadPacketThreadRun(void* arg){
	DawnPlayer* s = (DawnPlayer*)arg;
	s->ReadPacket();
}

bool DawnPlayer::Init(char* Path){
  printf("Path = %s\n",Path);


  _FormatCtx = avformat_alloc_context();
  if( !_FormatCtx ){
    return false;
  }
  _FormatCtx->interrupt_callback.callback = DawnPlayer::DecodeInterruptCB;
  _FormatCtx->interrupt_callback.opaque = this;

  if( avformat_open_input(&_FormatCtx, Path, NULL, NULL) != 0 ){
    return false;
  }

  if( avformat_find_stream_info(_FormatCtx, NULL ) < 0 ){
    return false;
  }
  /*_audioStream =
            av_find_best_stream(_FormatCtx, AVMEDIA_TYPE_AUDIO,
                                -1,
                                _audioStream,
                                NULL, 0);
  _videoStream =
            av_find_best_stream(_FormatCtx, AVMEDIA_TYPE_AUDIO,
                                -1,
                                _videoStream,
                                NULL, 0); 
  _subtitleStream =
	    av_find_best_stream(_FormatCtx, AVMEDIA_TYPE_SUBTITLE,
                                -1,
                                (_audioStream >= 0 ?
                                _audioStream :
                                _videoStream ),
                                NULL, 0);*/

  av_dump_format(_FormatCtx, -1, Path, 0);

  _VideoStream = -1;
  _AudioStream = -1;
  _SubtitleStream = -1;

  for( int i = 0; i < _FormatCtx->nb_streams; i++ ){

    switch( _FormatCtx->streams[i]->codec->codec_type){
      case AVMEDIA_TYPE_VIDEO:
	_VideoStream = i;
	break;
      case AVMEDIA_TYPE_AUDIO:
	_AudioStream = i;
	break;
      case AVMEDIA_TYPE_SUBTITLE:
	_SubtitleStream = i;
	break;
    }
  }

  if( _VideoStream == -1 ){
    printf("没有发现视频流\n");
    //return false;
  }
  else{
    if(_FormatCtx->streams[_VideoStream]->avg_frame_rate.den && 
             _FormatCtx->streams[_VideoStream]->avg_frame_rate.num){
        _Fps = av_q2d(_FormatCtx->streams[_VideoStream]->avg_frame_rate);
        _FrameStepTime = 1000000/_Fps;
    }
    //初始化视频解码器
    _VideoCodecCtx = _FormatCtx->streams[_VideoStream]->codec;

    _VideoCodec = avcodec_find_decoder(_VideoCodecCtx->codec_id);

    if( _VideoCodec == NULL ){
      printf("没有发现视频解码器\n");
      return false;
    }

    if( avcodec_open2(_VideoCodecCtx, _VideoCodec, NULL) < 0 ){
      return false;
    }
 
    //分配空闲帧队列 
    for( int i = 0; i < _MaxPacketListLen ; i++ ){
        AVFrame* frame = avcodec_alloc_frame();
        if( frame == NULL ){
            return false;
        }
        _YuvNumBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, 
				  _VideoCodecCtx->width,
				  _VideoCodecCtx->height);
  
        _YuvBuffer = (uint8_t*)av_malloc(_YuvNumBytes);

        avpicture_fill( (AVPicture *)frame, _YuvBuffer, AV_PIX_FMT_YUV420P,
                      _VideoCodecCtx->width, _VideoCodecCtx->height);
        frame->width = _VideoCodecCtx->width;
        frame->height = _VideoCodecCtx->height;

        _FreeFrameList.push_back(frame);
    }

    //分配RGB色彩域_
    _FrameRgb = avcodec_alloc_frame();

    if( _FrameRgb == NULL ){

      return false;
    }
    else{
  
      _RgbNumBytes = avpicture_get_size(AV_PIX_FMT_RGB24, 
				  _VideoCodecCtx->width,
				  _VideoCodecCtx->height);
  
      _RgbBuffer = (uint8_t*)av_malloc(_RgbNumBytes);

      avpicture_fill( (AVPicture *)_FrameRgb, _RgbBuffer, AV_PIX_FMT_RGB24,
                      _VideoCodecCtx->width, _VideoCodecCtx->height);
      _FrameRgb->width = _VideoCodecCtx->width;
      _FrameRgb->height = _VideoCodecCtx->height;
    }
    //分配YUV色彩域
    _FrameYuv = avcodec_alloc_frame();
    if( _FrameYuv == NULL ){
       return false;
    }
    else{
      _YuvNumBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, 
				  _VideoCodecCtx->width,
				  _VideoCodecCtx->height);
  
      _YuvBuffer = (uint8_t*)av_malloc(_YuvNumBytes);

      avpicture_fill( (AVPicture *)_FrameYuv, _YuvBuffer, AV_PIX_FMT_YUV420P,
                      _VideoCodecCtx->width, _VideoCodecCtx->height);
      _FrameYuv->width = _VideoCodecCtx->width;
      _FrameYuv->height = _VideoCodecCtx->height;
    }
  }
  
  
  //初始化音频解码器
  if( _AudioStream == -1 ){
    printf("没有发现音频流\n");
    //return false;
  }
  else{
    _AudioCodecCtx = _FormatCtx->streams[_AudioStream]->codec;

    _AudioCodec = avcodec_find_decoder(_AudioCodecCtx->codec_id);
    
    if( _AudioCodec == NULL ){
      printf("没有发现音频解码器\n");
      return false;
    }
    
    if (avcodec_open2(_AudioCodecCtx, _AudioCodec, NULL) < 0){
      return false;
    }
    _AudioFrame = avcodec_alloc_frame();

    if( _AudioFrame == NULL ){
      return false;
    }
  }
  
  //初始化字幕解码器
  if( _SubtitleStream == -1){
    printf("没有发现字幕流\n");
    //return false;
  }
  else{
    _SubtitleCodecCtx = _FormatCtx->streams[_SubtitleStream]->codec;

    _SubtitleCodec = avcodec_find_decoder(_SubtitleCodecCtx->codec_id);
    if( _SubtitleCodec == NULL ){
      printf("没有发现字幕解码器\n");
      return false;
    }
  }

  

  if( _VideoStream == -1 &&
      _AudioStream == -1 &&
     _SubtitleStream == -1 ){
     return false;
  } 
  return true;
}

void DawnPlayer::Play(){
   if( _VideoStream != -1 ){
     pthread_mutex_init(&_FreeFrameListMutex,NULL);
     pthread_mutex_init(&_ShowFrameListMutex,NULL);
     pthread_attr_init(&_PicShowThreadAttr);
     pthread_mutex_init(&_PicShowThreadCondMutex,NULL);
     pthread_cond_init(&_PicShowThreadCond,NULL);



     pthread_mutex_init(&_VideoPacketListMutex,NULL);
     pthread_attr_init(&_VideoThreadAttr);
     pthread_mutex_init(&_VideoThreadCondMutex,NULL);
     pthread_cond_init(&_VideoThreadCond,NULL);
     pthread_create(&_VideoThread,&_VideoThreadAttr,DawnPlayer::VideoThreadRun,(void*)this);

     pthread_create(&_PicShowThread,&_PicShowThreadAttr,DawnPlayer::PicShowThreadRun,(void*)this);
     
   }
   else{
     printf("没有发现视频流\n");
   }


   if( _SubtitleStream != -1){
      pthread_mutex_init(&_SubPacketListMutex,NULL);
      pthread_attr_init(&_SubThreadAttr);
      pthread_mutex_init(&_SubThreadCondMutex,NULL);
      pthread_cond_init(&_SubThreadCond,NULL);
      pthread_create(&_SubThread,&_SubThreadAttr,DawnPlayer::SubThreadRun,(void*)this);
   }
   else{
      printf("没有发现字幕流\n");
   }

   if( _AudioStream != -1 ){
      pthread_mutex_init(&_AudioPacketListMutex,NULL);
      pthread_attr_init(&_AudioThreadAttr);
      pthread_mutex_init(&_AudioThreadCondMutex,NULL);
      pthread_cond_init(&_AudioThreadCond,NULL);
      pthread_create(&_AudioThread,&_AudioThreadAttr,DawnPlayer::AudioThreadRun,(void*)this);
   }
   else{
      printf("没有发现音频流\n");
   }

   _StartPlayTime = (double)av_gettime() / 1000000.0;
   pthread_mutex_init(&_ReadAudioPacketCondMutex,NULL);
   pthread_cond_init(&_ReadAudioPacketCond,NULL);
   pthread_mutex_init(&_ReadVideoPacketCondMutex,NULL);
   pthread_cond_init(&_ReadVideoPacketCond,NULL);

   pthread_attr_init(&_ReadPacketThreadAttr);
   pthread_create(&_ReadPacketThread,&_ReadPacketThreadAttr,
			DawnPlayer::ReadPacketThreadRun,(void*)this);
   

}

void DawnPlayer::PicShow(){
    if( _VideoCallBack == NULL ){
        return ;
    }
    while(1){
        pthread_mutex_lock(&_ShowFrameListMutex);
        printf("_ShowFrameList.size  = %ld\n",_ShowFrameList.size());
        if(_ShowFrameList.size() < 1 ){
            pthread_mutex_unlock(&_ShowFrameListMutex);

            pthread_mutex_lock(&_PicShowThreadCondMutex);
            pthread_cond_wait(&_PicShowThreadCond,&_PicShowThreadCondMutex);
            pthread_mutex_unlock(&_PicShowThreadCondMutex);
        }
        else{
            pthread_mutex_unlock(&_ShowFrameListMutex);
	}

        pthread_mutex_lock(&_ShowFrameListMutex);
        AVFrame* frame = _ShowFrameList.front();
        _ShowFrameList.pop_front();
        pthread_mutex_unlock(&_ShowFrameListMutex);

        pthread_mutex_lock(&_ReadVideoPacketCondMutex);
        pthread_cond_signal(&_ReadVideoPacketCond);
        pthread_mutex_unlock(&_ReadVideoPacketCondMutex);

        /////////////////////////////////////////////////////////
        //显示视频
        vframe_index++;
        if( _VideoCallBack ){
            printf("VideoCallBack\n");
            printf("frame->width = %d,frame->height = %d\n",frame->width,frame->height);
	   
            printf("frame->format = %d\n",frame->format);
            printf("frame->pts = %ld\n",frame->pts);
            printf("frame->pkt_pts = %ld\n",frame->pkt_pts);
            printf("frame->pkt_dts = %ld\n",frame->pkt_dts);
            printf("_StartPlayTime = %f\n",_StartPlayTime);

	    
            //VideoConvertYuv(frame);
            VideoConvertRgb(frame);

            ///////////////////////////////////////////////////////
            //计算pts
            double dpts = NAN;
	    frame->pts = frame->pkt_dts;
            printf("frame->pts2 = %ld\n",frame->pkt_dts);
	    _FrameVideoPts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(_FormatCtx->streams[_VideoStream]->time_base);
	    printf("vqpts = %f\n",_FrameVideoPts);
            //同步视频时钟
            double frame_delay;
            if( _FrameVideoPts != 0  ){
	        _VideoClock = _FrameVideoPts;
            }    
	    else{
	        _FrameVideoPts = _VideoClock;
	    }
	    frame_delay = av_q2d(_FormatCtx->streams[_VideoStream]->codec->time_base);
            frame_delay += frame->repeat_pict * (frame_delay * 0.5);  
            _VideoClock += frame_delay;  
            printf("_VideoClock = %f\n",_VideoClock);

            _LastFrameVideoPts = frame->pkt_pts;

            ///////////////////////////////////////////
	    //计算延时
            double delay0 = 0;
            if( _AudioClock != 0 ){
                delay0 = (_FrameVideoPts - _AudioClock)*100000;
            }
            printf("delay0 = %f,%ld\n",delay0,_ShowFrameList.size());
            double time = av_gettime();
            double delay1 = _NexFrameTime - time;
            printf("delay1 = %f\n",delay1);
            if( delay0 > 0 ){
                if( delay1 > 0 ) {
                    if( delay1 > delay0 ){
                        usleep(delay1);
                    }
		    else{
                        usleep(delay0);
		    }
                } 
	    }
            time = av_gettime();

            printf("frame_step_time = %f\n",time - _PreFrameTime);
            _PreFrameTime = time;



            _NexFrameTime = time + _FrameStepTime;
	    ////////////////////////////////////////////

            //_VideoCallBack(_VideoCallBackPrvData,_FrameYuv);
            _VideoCallBack(_VideoCallBackPrvData,_FrameRgb);
            //SaveFrame(_FrameRgb, frame->width, frame->height, frame->pkt_pts);


        }

      
        //////////////////////////////////////////
        //frame 用完换回_FreeFrameList列表
        pthread_mutex_lock(&_FreeFrameListMutex);
        _FreeFrameList.push_back(frame);
        pthread_mutex_unlock(&_FreeFrameListMutex);
    }
}

void DawnPlayer::VideoConvertRgb(AVFrame *src_frame){

  _RgbConvertCtx = sws_getCachedContext(_RgbConvertCtx, 
		src_frame->width,src_frame->height, 
                (AVPixelFormat)src_frame->format,
                 src_frame->width, src_frame->height,
                 AV_PIX_FMT_RGB24, SWS_BICUBIC,
                 NULL, NULL, NULL);

  if( !_RgbConvertCtx ) {

      fprintf(stderr, "Cannot initialize sws conversion context\n");

      exit(1);

      
  }

  sws_scale(_RgbConvertCtx, (const uint8_t* const*)src_frame->data,
	      src_frame->linesize, 0, src_frame->height, 
	      _FrameRgb->data,_FrameRgb->linesize);

  printf("_FrameRgb->width = %d,_FrameRgb->height = %d\n",_FrameRgb->width,_FrameRgb->height);
  printf("_FrameRgb->linesize[0] = %d\n",_FrameRgb->linesize[0]);
}

void  DawnPlayer::VideoConvertYuv(AVFrame *src_frame)
{
  _YuvConvertCtx = sws_getCachedContext(_YuvConvertCtx, 
  //_YuvConvertCtx = sws_getContext( 
		src_frame->width,

                 src_frame->height, (AVPixelFormat)src_frame->format,

                 src_frame->width, src_frame->height,

                 AV_PIX_FMT_YUV420P, SWS_BILINEAR,

                 NULL, NULL, NULL);

  if( !_YuvConvertCtx ) {

      fprintf(stderr, "Cannot initialize sws conversion context\n");

      exit(1);

      
  }

  sws_scale(_YuvConvertCtx, (const uint8_t* const*)src_frame->data,
	      src_frame->linesize, 0, src_frame->height, 
	      _FrameYuv->data,_FrameYuv->linesize);
  printf("_FrameYuv->width = %d,_FrameYuv->height = %d\n",_FrameYuv->width,_FrameYuv->height);
  printf("_FrameYuv->linesize[0] = %d\n",_FrameYuv->linesize[0]);
}

void DawnPlayer::VideoDecode(){
  AVFrame* frame = avcodec_alloc_frame();
  AVFrame* free_frame;
  int VideoFrameFinished;//视频帧结束标志
  int ret = 0;

  pthread_mutex_lock(&_FreeFrameListMutex);
  free_frame = _FreeFrameList.front();
  _FreeFrameList.pop_front();
  pthread_mutex_unlock(&_FreeFrameListMutex);

  while(1){
      pthread_mutex_lock(&_VideoPacketListMutex);
      printf("_VideoPacketList.size = %ld\n",_VideoPacketList.size());
      if( _VideoPacketList.size() < 1 ){
          pthread_mutex_unlock(&_VideoPacketListMutex);

          pthread_mutex_lock(&_VideoThreadCondMutex);
          pthread_cond_wait(&_VideoThreadCond,&_VideoThreadCondMutex);
          pthread_mutex_unlock(&_VideoThreadCondMutex);
      }
      else{
          pthread_mutex_unlock(&_VideoPacketListMutex);
      }

      pthread_mutex_lock(&_VideoPacketListMutex);
      AVPacket packet = _VideoPacketList.front();
      _VideoPacketList.pop_front();
      pthread_mutex_unlock(&_VideoPacketListMutex);

      pthread_mutex_lock(&_ReadVideoPacketCondMutex);
      pthread_cond_signal(&_ReadVideoPacketCond);
      pthread_mutex_unlock(&_ReadVideoPacketCondMutex);

      //////////////////////////////////////////////////////////
      //解码
      avcodec_get_frame_defaults(frame);
      ret = avcodec_decode_video2(_VideoCodecCtx, frame, &VideoFrameFinished, &packet);
      if( ret < 0 ){
          printf("avcodec_decode_video2 error ret = %d\n",ret);
          av_free_packet(&packet);
	  continue;
      }
      if( VideoFrameFinished ) {
         ///////////////////////////////////////////////////////
         //拷贝到free_frame
         av_picture_copy((AVPicture *)free_frame,(const AVPicture *)frame,
                         (AVPixelFormat)frame->format, frame->width, frame->height);
         free_frame->format = frame->format;
         free_frame->width = frame->width;
         free_frame->height = frame->height;
         free_frame->pkt_dts = frame->pkt_dts;
         free_frame->pts = frame->pts;

	 ////////////////////////////////////////////

         pthread_mutex_lock(&_ShowFrameListMutex);
         _ShowFrameList.push_back(free_frame);
         pthread_mutex_unlock(&_ShowFrameListMutex);

         pthread_mutex_lock(&_PicShowThreadCondMutex);
         pthread_cond_signal(&_PicShowThreadCond);
         pthread_mutex_unlock(&_PicShowThreadCondMutex);
    
         
	 while( 1 ){
             pthread_mutex_lock(&_FreeFrameListMutex);
             //printf("_FreeFrameList.size = %ld\n",_FreeFrameList.size());
             if( _FreeFrameList.size() < 1 ){
             	 pthread_mutex_unlock(&_FreeFrameListMutex);
		 usleep(5000);
             }
	     else{
                 free_frame = _FreeFrameList.front();
                 _FreeFrameList.pop_front();
                 pthread_mutex_unlock(&_FreeFrameListMutex);
 		 break;
	     }
	 }
      }


      av_free_packet(&packet);

  }
  av_frame_free(&frame);
}


void DawnPlayer::AudioPts()
{
  AVRational tb;
  tb = (AVRational){1, _AudioFrame->sample_rate};
  if (_AudioFrame->pts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrame->pts, _AudioCodecCtx->time_base, tb);
  else if (_AudioFrame->pkt_pts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrame->pkt_pts, 
					_FormatCtx->streams[_AudioStream]->time_base, tb);
  else if (_AudioFrameNextPts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrameNextPts, 
                                    (AVRational){1, _AudioParameterSrc._sample_rate}, tb);
  if( _AudioFrame->pts != AV_NOPTS_VALUE ){
     _AudioFrameNextPts = _AudioFrame->pts + _AudioFrame->nb_samples;
  }
  _FrameAudioPts = (_AudioFrame->pts == AV_NOPTS_VALUE) ? NAN : _AudioFrame->pts * av_q2d(_FormatCtx->streams[_AudioStream]->time_base);
  printf("aqpts = %f\n",_FrameAudioPts);

  if (_AudioFrame->pts != AV_NOPTS_VALUE)
      _AudioClock = _AudioFrame->pts * av_q2d(tb) + (double) _AudioFrame->nb_samples / _AudioFrame->sample_rate;
  else
      _AudioClock = NAN;
  printf("_AudioClock = %f\n",_AudioClock);

}

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
int DawnPlayer::SynchronizeAudio(int nb_samples)
{//根据后续开发决定是否增加
    int wanted_nb_samples = nb_samples;

    // if not master, then we try to remove or add samples to correct the clock 
    /*if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&is->audclk) - get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                //not enough measures to have a correct estimate
                is->audio_diff_avg_count++;
            } else {
                //estimate the A-V difference 
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
                av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            //too big difference : may be initial PTS errors, so reset A-V filter 
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }*/

    return wanted_nb_samples;
}

void DawnPlayer::AudioConvert(unsigned char** buf,int &len)
{
/*
*将流内解压后的音频转化成音频设备可识别格式
*/
   int64_t dec_channel_layout;
   int wanted_nb_samples;
   int resampled_data_size;
   dec_channel_layout =
          (_AudioFrame->channel_layout && av_frame_get_channels(_AudioFrame) 
		== av_get_channel_layout_nb_channels(_AudioFrame->channel_layout)) 
		? _AudioFrame->channel_layout 
		: av_get_default_channel_layout(av_frame_get_channels(_AudioFrame));
   printf("_AudioFrame->nb_samples = %d\n",_AudioFrame->nb_samples);
   wanted_nb_samples = SynchronizeAudio(_AudioFrame->nb_samples);
   printf("wanted_nb_samples = %d\n",wanted_nb_samples);
   if (_AudioFrame->format != _AudioParameterSrc._fmt            ||
                dec_channel_layout         != _AudioParameterSrc._channel_layout ||
                _AudioFrame->sample_rate   != _AudioParameterSrc._sample_rate    ||
                (wanted_nb_samples         != _AudioFrame->nb_samples && !_SwrCtx)) {
                swr_free(&_SwrCtx);

     _SwrCtx = swr_alloc_set_opts(NULL,_AudioTgt._channel_layout, 
				(AVSampleFormat)(_AudioTgt._fmt), _AudioTgt._sample_rate,
				dec_channel_layout,(AVSampleFormat)_AudioFrame->format, 
				_AudioFrame->sample_rate,0, NULL);
     printf("channel_layout %lld,%lld\n",(long long int)_AudioTgt._channel_layout,(long long int)dec_channel_layout);
     printf("output parameter fmt = %d,sample_rate = %d\n",_AudioTgt._fmt,_AudioTgt._sample_rate);
     printf("input parameter fmt = %d,sample_rate = %d\n",_AudioFrame->format,_AudioFrame->sample_rate);

     if (!_SwrCtx || swr_init(_SwrCtx) < 0) {
	av_log(NULL, AV_LOG_ERROR,"Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",_AudioFrame->sample_rate, 
		av_get_sample_fmt_name((AVSampleFormat)_AudioFrame->format), 
		av_frame_get_channels(_AudioFrame),_AudioTgt._sample_rate,
		av_get_sample_fmt_name((AVSampleFormat)_AudioTgt._fmt), _AudioTgt._channels);
                    return;
     }
     _AudioParameterSrc._channel_layout = dec_channel_layout;
     _AudioParameterSrc._channels       = av_frame_get_channels(_AudioFrame);
     _AudioParameterSrc._sample_rate = _AudioFrame->sample_rate;
     _AudioParameterSrc._fmt = (AVSampleFormat)_AudioFrame->format;
  }


  if (_SwrCtx) {
     const uint8_t **in = (const uint8_t **)_AudioFrame->extended_data;
     uint8_t **out = &_AudioSwrBuf;
     int out_count = (int64_t)wanted_nb_samples * _AudioTgt._sample_rate / _AudioFrame->sample_rate + 256;
     int out_size  = av_samples_get_buffer_size(NULL, _AudioTgt._channels, 
						out_count, (AVSampleFormat)_AudioTgt._fmt, 0);
     int len2;
     if (out_size < 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
        return;
     }
     if (wanted_nb_samples != _AudioFrame->nb_samples) {
       if (swr_set_compensation(_SwrCtx, 
		(wanted_nb_samples - _AudioFrame->nb_samples) * _AudioTgt._sample_rate / _AudioFrame->sample_rate,
                wanted_nb_samples * _AudioTgt._sample_rate / _AudioFrame->sample_rate) < 0) {
          av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
          return;
       }
     }
     av_fast_malloc(&_AudioSwrBuf, &_AudioSwrBufSize, out_size);
     printf("_AudioSwrBufSize = %d,out_size = %d\n",_AudioSwrBufSize, out_size);
     if (!_AudioSwrBuf)
        return ;//AVERROR(ENOMEM);
     len2 = swr_convert(_SwrCtx, out, out_count, in, _AudioFrame->nb_samples);
     if (len2 < 0) {
       av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
       return;
     }
     if (len2 == out_count) {
       av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
       swr_init(_SwrCtx);
     }
     *buf = _AudioSwrBuf;
     resampled_data_size = len2 * _AudioTgt._channels * av_get_bytes_per_sample((AVSampleFormat)_AudioTgt._fmt);
     printf("1 resampled_data_size = %d\n",resampled_data_size);
     len = resampled_data_size;
  } else {
     *buf = _AudioFrame->data[0];
     resampled_data_size = len;
     printf("2 resampled_data_size = %d\n",resampled_data_size);
     len = resampled_data_size;
  }
 


}

void DawnPlayer::AudioDecode(){
   int AudioFrameFinished;//音频帧结束标志 
   int ret = 0;
   while(1){
      pthread_mutex_lock(&_AudioPacketListMutex);
      if( _AudioPacketList.size() < 1 ){
          printf("_AudioPacketList.size = %ld\n",_AudioPacketList.size());
          pthread_mutex_unlock(&_AudioPacketListMutex);

          pthread_mutex_lock(&_AudioThreadCondMutex);
          pthread_cond_wait(&_AudioThreadCond,&_AudioThreadCondMutex);
          pthread_mutex_unlock(&_AudioThreadCondMutex);
      }
      else{
          pthread_mutex_unlock(&_AudioPacketListMutex);
      }

      pthread_mutex_lock(&_AudioPacketListMutex);
      AVPacket  packet = _AudioPacketList.front();
      _AudioPacketList.pop_front();
      pthread_mutex_unlock(&_AudioPacketListMutex);

      pthread_mutex_lock(&_ReadAudioPacketCondMutex);
      pthread_cond_signal(&_ReadAudioPacketCond);
      pthread_mutex_unlock(&_ReadAudioPacketCondMutex);

      /*if( packet.stream_index != _AudioStream ){
         printf("packet.stream_index = %d\n",packet.stream_index);
         continue;
      }*/
      avcodec_get_frame_defaults(_AudioFrame);
      ret = avcodec_decode_audio4(_AudioCodecCtx, _AudioFrame, &AudioFrameFinished, &packet);
      if ( ret < 0) {
         // if error, we skip the frame 
         packet.size = 0;
         printf("packet.size = 0\n");
      }
      packet.dts =
      packet.pts = AV_NOPTS_VALUE;
      packet.data += ret;
      packet.size -= ret;
      if (packet.data && packet.size <= 0 || !packet.data && !AudioFrameFinished ){
         packet.stream_index = -1;
      }
      if( AudioFrameFinished ){
         aframe_index++;
         AudioPts();
    
         if( !_AudioCallBack ){
            printf("!_AudioCallBack\n");
         }
         else{
	    int data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(_AudioFrame),
                                                    _AudioFrame->nb_samples,
                                                    (AVSampleFormat)_AudioFrame->format, 1);
	    printf("data_size = %d\n",data_size);
            unsigned char *buf;
            AudioConvert(&buf,data_size);
            _AudioCallBack(_AudioCallBackPrvData,buf,data_size);
            /*printf("_AudioFrame->format = %d\n",_AudioFrame->format);
            printf("_AudioFrame->pts = %ld\n",_AudioFrame->pts);
            printf("_AudioFrame->pkt_pts = %ld\n",_AudioFrame->pkt_pts);
            printf("_AudioFrame->pkt_dts = %ld\n",_AudioFrame->pkt_dts);
            printf("_StartPlayTime = %f\n",_StartPlayTime);*/
	    

         }
      }

      av_free_packet(&packet);

   }

}

void DawnPlayer::SubtitleDecode(){
  int SubFrameFinished;
  int ret = 0;
  AVSubtitle sub;
  while(1){
     pthread_mutex_lock(&_SubPacketListMutex);
     if( _SubPacketList.size() < 1 ){
        pthread_mutex_unlock(&_SubPacketListMutex);

        pthread_mutex_lock(&_SubThreadCondMutex);
        pthread_cond_wait(&_SubThreadCond,&_SubThreadCondMutex);
        pthread_mutex_unlock(&_SubThreadCondMutex);
    
     }
     else{
        pthread_mutex_unlock(&_SubPacketListMutex);
     }

     pthread_mutex_lock(&_SubPacketListMutex);
     AVPacket packet = _SubPacketList.front();
     _SubPacketList.pop_front();
     pthread_mutex_unlock(&_SubPacketListMutex);

     avcodec_decode_subtitle2(_SubtitleCodecCtx, &sub,&SubFrameFinished, &packet);
     if( SubFrameFinished ){
        sframe_index++;
     }
     avsubtitle_free(&sub);


     av_free_packet(&packet);
  }
}

void DawnPlayer::ReadPacket(){
  int ret = 0;
  int eof = 0;
  AVPacket packet;

  while( 1 ) {
    bool is_wait;
    pthread_mutex_lock(&_AudioPacketListMutex);
    is_wait = _AudioPacketList.size() > _MaxPacketListLen;
    pthread_mutex_unlock(&_AudioPacketListMutex);
    if( is_wait ){
	pthread_mutex_lock(&_ReadAudioPacketCondMutex);
        pthread_cond_wait(&_ReadAudioPacketCond,&_ReadAudioPacketCondMutex);
	pthread_mutex_unlock(&_ReadAudioPacketCondMutex);
    }

    pthread_mutex_lock(&_ShowFrameListMutex);
    is_wait =  _ShowFrameList.size() > _MaxPacketListLen ;
    pthread_mutex_unlock(&_ShowFrameListMutex);
    if( is_wait ){
	pthread_mutex_lock(&_ReadVideoPacketCondMutex);
        pthread_cond_wait(&_ReadVideoPacketCond,&_ReadVideoPacketCondMutex);
	pthread_mutex_unlock(&_ReadVideoPacketCondMutex);
    }

    ret = av_read_frame(_FormatCtx, &packet);
    if (ret < 0) {
      print_error(NULL,ret);
      if (ret == AVERROR_EOF || url_feof(_FormatCtx->pb)){
	eof = 1;
	break;
	
      }
      
      /*if (_ic->pb && _ic->pb->error)
	break;*/
      continue;
    }
    
    //printf("packet.stream_index = %d\n",packet.stream_index);
 
    if( _VideoCodecCtx && packet.stream_index == _VideoStream ) {
      pthread_mutex_lock(&_VideoPacketListMutex);
      _VideoPacketList.push_back(packet);
      printf("add video packet,size = %ld\n",_VideoPacketList.size());
      pthread_mutex_unlock(&_VideoPacketListMutex);

      pthread_mutex_lock(&_VideoThreadCondMutex);
      pthread_cond_signal(&_VideoThreadCond);
      pthread_mutex_unlock(&_VideoThreadCondMutex);

    }
    else if( _AudioCodecCtx && packet.stream_index == _AudioStream ){
      pthread_mutex_lock(&_AudioPacketListMutex);
      _AudioPacketList.push_back(packet);
      printf("add audio packet,size = %ld\n",_AudioPacketList.size());
      pthread_mutex_unlock(&_AudioPacketListMutex);

      pthread_mutex_lock(&_AudioThreadCondMutex);
      pthread_cond_signal(&_AudioThreadCond);
      pthread_mutex_unlock(&_AudioThreadCondMutex);
    }
    else if( _SubtitleCodecCtx && packet.stream_index == _SubtitleStream ){
      pthread_mutex_lock(&_SubPacketListMutex);
      _SubPacketList.push_back(packet);
      //printf("add sub packet,size = %ld\n",_SubPacketList.size());
      pthread_mutex_unlock(&_SubPacketListMutex);
      
      pthread_mutex_lock(&_SubThreadCondMutex);
      pthread_cond_signal(&_SubThreadCond);
      pthread_mutex_unlock(&_SubThreadCondMutex);
    }
  }

  printf("vframe_index = %d\n",vframe_index);
  printf("aframe_index = %d\n",aframe_index);
  printf("sframe_index = %d\n",sframe_index);

}

