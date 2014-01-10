 //g++ -g -o DawnPlayer -D__STDC_CONSTANT_MACROS   DawnPlayer.cpp -I/home/dongrui/program/ffmpeg/include -L/home/dongrui/program/ffmpeg/lib   -lavformat -lavcodec  -lavdevice  -lavfilter  -lavutil  -lswresample  -lswscale -lz -lpthread -lboost_thread-mt -lSDL2 
//-fpermissive /usr/lib/x86_64-linux-gnu/libpulse-simple.so /usr/lib/x86_64-linux-gnu/libpulse.so
#include "DawnPlayer.h"
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
  _FormatCtx(NULL),_VideoFrame(NULL),
  _FrameRgb(NULL),_RgbBuffer(NULL),
  _FrameYuv(NULL),_YuvBuffer(NULL),
  _videoCodecCtx(NULL),_videoCodec(NULL),
  _audioCodecCtx(NULL),_audioCodec(NULL),
  _subtitleCodecCtx(NULL),_subtitleCodec(NULL),
  _AudioFrame(NULL),
  _AudioCallBack(NULL),_AudioCallBackPrvData(NULL),
  _VideoCallBack(NULL),_VideoCallBackPrvData(NULL),
  vframe_index(0),aframe_index(0),sframe_index(0){

    
}

DawnPlayer::~DawnPlayer(){
  
  if(_FrameRgb)
    av_free(_FrameRgb);

  if(_RgbBuffer)
    av_free(_RgbBuffer);

  if(_FrameYuv)
    av_free(_FrameYuv);

  if(_YuvBuffer)
    av_free(_YuvBuffer);

  if(_VideoFrame)
    av_free(_VideoFrame);

  if(_AudioFrame)
    av_free(_AudioFrame);

  if(_videoCodecCtx)
    avcodec_close(_videoCodecCtx);

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
	if( !_audioCodecCtx ){
		return ;
	}
	parameter->_channels = _audioCodecCtx->channels;
	parameter->_sample_rate = _audioCodecCtx->sample_rate;
	parameter->_channel_layout = _audioCodecCtx->channel_layout;
	//parameter->_fmt = _audioCodecCtx->;
	_AudioTgt._channels = _audioCodecCtx->channels;
	_AudioTgt._sample_rate = _audioCodecCtx->sample_rate;
	_AudioTgt._channel_layout = _audioCodecCtx->channel_layout;
	_AudioTgt._fmt = SAMPLE_FMT_S16;
}

void DawnPlayer::SetAudioCallBack(void* data,AudioCallBack callback)
{
  _AudioCallBack = callback;
  _AudioCallBackPrvData = data;
}

void DawnPlayer::GetVideoParameter(VideoParameter* parameter)
{
	parameter->_width = _videoCodecCtx->width;
	parameter->_height = _videoCodecCtx->height;
}

void DawnPlayer::SetVideoCallBack(void* data,VideoCallBack callback)
{
  _VideoCallBack = callback;
  _VideoCallBackPrvData = data;
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

  _videoStream = -1;
  _audioStream = -1;
  _subtitleStream = -1;

  for( int i = 0; i < _FormatCtx->nb_streams; i++ ){

    switch( _FormatCtx->streams[i]->codec->codec_type){
      case AVMEDIA_TYPE_VIDEO:
	_videoStream = i;
	break;
      case AVMEDIA_TYPE_AUDIO:
	_audioStream = i;
	break;
      case AVMEDIA_TYPE_SUBTITLE:
	_subtitleStream = i;
	break;
    }
  }

  if( _videoStream == -1 ){
    printf("没有发现视频流\n");
    //return false;
  }
  else{
    //初始化视频解码器
    _videoCodecCtx = _FormatCtx->streams[_videoStream]->codec;

    _videoCodec = avcodec_find_decoder(_videoCodecCtx->codec_id);

    if( _videoCodec == NULL ){
      printf("没有发现视频解码器\n");
      return false;
    }

    if( avcodec_open2(_videoCodecCtx, _videoCodec, NULL) < 0 ){
      return false;
    }
  

    _VideoFrame = avcodec_alloc_frame();

    if( _VideoFrame == NULL ){
      return false;
    }

    //分配RGB色彩域_
    _FrameRgb = avcodec_alloc_frame();

    if( _FrameRgb == NULL ){

      return false;
    }
    else{
  
      _RgbNumBytes = avpicture_get_size(PIX_FMT_RGB24, 
				  _videoCodecCtx->width,
				  _videoCodecCtx->height);
  
      _RgbBuffer = (uint8_t*)av_malloc(_RgbNumBytes);

      avpicture_fill( (AVPicture *)_FrameRgb, _RgbBuffer, PIX_FMT_RGB24,
                      _videoCodecCtx->width, _videoCodecCtx->height);
    }

    //分配YUV色彩域
    _FrameYuv = avcodec_alloc_frame();
    if( _FrameYuv == NULL ){
       return false;
    }
    else{
      _YuvNumBytes = avpicture_get_size(PIX_FMT_YUV420P, 
				  _videoCodecCtx->width,
				  _videoCodecCtx->height);
  
      _YuvBuffer = (uint8_t*)av_malloc(_YuvNumBytes);

      avpicture_fill( (AVPicture *)_FrameYuv, _YuvBuffer, PIX_FMT_YUV420P,
                      _videoCodecCtx->width, _videoCodecCtx->height);
    }
  }
  
  //初始化音频解码器
  if( _audioStream == -1 ){
    printf("没有发现音频流\n");
    //return false;
  }
  else{
    _audioCodecCtx = _FormatCtx->streams[_audioStream]->codec;

    _audioCodec = avcodec_find_decoder(_audioCodecCtx->codec_id);
    
    if( _audioCodec == NULL ){
      printf("没有发现音频解码器\n");
      return false;
    }
    
    if (avcodec_open2(_audioCodecCtx, _audioCodec, NULL) < 0){
      return false;
    }
    _AudioFrame = avcodec_alloc_frame();

    if( _AudioFrame == NULL ){
      return false;
    }

  }
  
  //初始化字幕解码器
  if( _subtitleStream == -1){
    printf("没有发现字幕流\n");
    //return false;
  }
  else{
    _subtitleCodecCtx = _FormatCtx->streams[_subtitleStream]->codec;

    _subtitleCodec = avcodec_find_decoder(_subtitleCodecCtx->codec_id);
    if( _subtitleCodec == NULL ){
      printf("没有发现字幕解码器\n");
      return false;
    }
  }
  
  //_thr = boost::thread(boost::bind(&DawnPlayer::Run,this));

  return true;
}

void DawnPlayer::SaveRgbImage(){
  struct SwsContext *img_convert_ctx = NULL;

  img_convert_ctx = sws_getCachedContext(
		img_convert_ctx, _VideoFrame->width,

                 _VideoFrame->height, (AVPixelFormat)_VideoFrame->format,

                 _VideoFrame->width, _VideoFrame->height,

                 PIX_FMT_RGB24, SWS_BICUBIC,

                 NULL, NULL, NULL);

  if( !img_convert_ctx ) {

      fprintf(stderr, "Cannot initialize sws conversion context\n");

      exit(1);

      
  }

  sws_scale(img_convert_ctx, (const uint8_t* const*)_VideoFrame->data,
	      _VideoFrame->linesize, 0, _VideoFrame->height, 
	      _FrameRgb->data,_FrameRgb->linesize);

  //SaveFrame(pFrameRGB, _videoCodecCtx->width, _videoCodecCtx->height, vframe_index);
  SaveFrame(_FrameRgb, _VideoFrame->width, _VideoFrame->height, vframe_index);
}

void  DawnPlayer::VideoConvert()
{
  struct SwsContext *img_convert_ctx = NULL;

  img_convert_ctx = sws_getCachedContext(
		img_convert_ctx, _VideoFrame->width,

                 _VideoFrame->height, (AVPixelFormat)_VideoFrame->format,

                 _VideoFrame->width, _VideoFrame->height,

                 PIX_FMT_YUV420P, SWS_BILINEAR,

                 NULL, NULL, NULL);

  if( !img_convert_ctx ) {

      fprintf(stderr, "Cannot initialize sws conversion context\n");

      exit(1);

      
  }

  sws_scale(img_convert_ctx, (const uint8_t* const*)_VideoFrame->data,
	      _VideoFrame->linesize, 0, _VideoFrame->height, 
	      _FrameYuv->data,_FrameYuv->linesize);

}

void DawnPlayer::VideoDecode(){
  int ret = 0;
  avcodec_get_frame_defaults(_VideoFrame);
  ret = avcodec_decode_video2(_videoCodecCtx, _VideoFrame, &frameFinished, &packet);
  if( ret < 0 ){
    printf("avcodec_decode_video2 error ret = %d\n",ret);
  }
  if( frameFinished ) {
     vframe_index++;
     if( _VideoCallBack ){
        printf("VideoCallBack\n");
        _VideoCallBack(_VideoCallBackPrvData,_VideoFrame);
     }
    //SaveRgbImage();
  }
}


void DawnPlayer::AudioPts()
{
  AVRational tb;
  printf("0_AudioFrame->pts = %lld\n",(long long int)_AudioFrame->pts);
  tb = (AVRational){1, _AudioFrame->sample_rate};
  if (_AudioFrame->pts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrame->pts, _audioCodecCtx->time_base, tb);
  else if (_AudioFrame->pkt_pts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrame->pkt_pts, 
					_FormatCtx->streams[_audioStream]->time_base, tb);
  else if (_AudioFrameNextPts != AV_NOPTS_VALUE)
    _AudioFrame->pts = av_rescale_q(_AudioFrameNextPts, 
                                    (AVRational){1, _AudioParameterSrc._sample_rate}, tb);

  printf("1_AudioFrame->pts = %lld\n",(long long int)_AudioFrame->pts);
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
     _AudioParameterSrc._fmt = (SampleFormat)_AudioFrame->format;
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
  int ret = 0;
  while( packet.stream_index != -1 ){
    avcodec_get_frame_defaults(_AudioFrame);
    ret = avcodec_decode_audio4(_audioCodecCtx, _AudioFrame, &frameFinished, &packet);
    if ( ret < 0) {
      // if error, we skip the frame 
      packet.size = 0;
      return;
    }
    if( frameFinished ){
      aframe_index++;
    
      if( !_AudioCallBack ){
        return;
      }
      else{
	int data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(_AudioFrame),
                                                    _AudioFrame->nb_samples,
                                                    (AVSampleFormat)_AudioFrame->format, 1);
	printf("data_size = %d\n",data_size);
        unsigned char *buf;
        AudioConvert(&buf,data_size);
        //_AudioCallBack(_AudioCallBackData,_AudioFrame->data[0],data_size);
        _AudioCallBack(_AudioCallBackPrvData,buf,data_size);
        printf("format = %d\n",_AudioFrame->format);
      }
    }
    packet.dts =
    packet.pts = AV_NOPTS_VALUE;
    packet.data += ret;
    packet.size -= ret;
    if (packet.data && packet.size <= 0 || !packet.data && !frameFinished ){
	packet.stream_index = -1;
    }
    AudioPts();
  }
}

void DawnPlayer::SubtitleDecode(){
  int ret = 0;
  AVSubtitle sub;
  avcodec_decode_subtitle2(_subtitleCodecCtx, &sub,&frameFinished, &packet);
  avsubtitle_free(&sub);
  if( frameFinished ){
    sframe_index++;
  }
}

void DawnPlayer::Run(){
  int ret = 0;
  int eof = 0;

  while( 1 ) {
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
    
    if( _videoCodecCtx && packet.stream_index == _videoStream ) {
      VideoDecode();
    }
    else if( _audioCodecCtx && packet.stream_index == _audioStream ){
      AudioDecode();
    }
    else if( _subtitleCodecCtx && packet.stream_index == _subtitleStream ){
      SubtitleDecode();
    }

    av_free_packet(&packet);

  }

  /*av_free(buffer);

  av_free(pFrameRGB);

  av_free(pFrame);

  avcodec_close(pCodecCtx);

  avformat_close_input(&pFormatCtx);*/
  printf("vframe_index = %d\n",vframe_index);
  printf("aframe_index = %d\n",aframe_index);
  printf("sframe_index = %d\n",sframe_index);

  delete this;
}

#include <SDL2/SDL.h>  
#include <SDL2/SDL_thread.h>
SDL_Renderer *renderer;
void VideoPlay(void* prv_data,AVFrame* frame){
  SDL_Rect        rect;
  SDL_Texture    *bmp = (SDL_Texture*)prv_data;	
  rect.x = 0;  
  rect.y = 0;  
  rect.w = frame->width;  
  rect.h = frame->height; 

  SDL_UpdateTexture( bmp, &rect, frame->data[0], frame->linesize[0] );
  SDL_RenderClear( renderer );  
  SDL_RenderCopy( renderer, bmp, &rect, &rect );  
  SDL_RenderPresent( renderer );
}

#include <pulse/simple.h>
#include <pulse/error.h>
void AudioPlay(void* prv_data,unsigned char* buf,int len){
	int error;
	if (pa_simple_write((pa_simple*)prv_data, buf, (size_t) len, &error) < 0) {
            printf("pa_simple_write() failed: %s\n", pa_strerror(error));
            return ;
        }	
}


int main(int argc,char* argv[]){
  char buf[1024];
  AudioParameter parameter;
  DawnPlayer *player = NULL;
  av_register_all();

  int error;
  pa_simple *s;
  pa_sample_spec ss;
 
    /*if( player ){
      delete player;
    }*/
    player = new DawnPlayer();
    sprintf(buf,"%s",argv[1]);
    //sprintf(buf,"%s%d%s","/home/dongrui/test_",i,".ts");
    //sprintf(buf,"%s%s","/home/dongrui/","7907.flv");//PA_SAMPLE_S16NE
    //sprintf(buf,"%s%s","/home/dongrui/","晓松说.ts");//PA_SAMPLE_FLOAT32LE
    if( !player->Init(buf) ){
      return 0;
      
    }
    
    //sleep(10);
    player->GetAudioParameter(&parameter);
    ss.format = PA_SAMPLE_S16LE;//PA_SAMPLE_FLOAT32LE(晓松说用)
    ss.channels = parameter._channels;
    ss.rate = parameter._sample_rate;
    s = pa_simple_new(NULL,               // Use the default server.
                  argv[0],           // Our application's name.
                  PA_STREAM_PLAYBACK,
                  NULL,               // Use the default device.
                  "Audio",            // Description of our stream.
                  &ss,                // Our sample format.
                  NULL,               // Use default channel map
                  NULL,               // Use default buffering attributes.
                  &error               // Ignore error code.
                  );

    if( !s ){
      printf("error pulseaudio = %s\n",pa_strerror(error));
      return 0;
    }
    player->SetAudioCallBack(s,AudioPlay);

    /////////////////////////////////////////////////////
    //
    VideoParameter videoparameter;
    player->GetVideoParameter(&videoparameter);
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());  
        exit(1);  
    }
    SDL_Texture    *bmp = NULL;  
    SDL_Window     *screen = NULL;  
    SDL_Rect        rect;
    screen = SDL_CreateWindow("My Game Window",  
                              SDL_WINDOWPOS_UNDEFINED,  
                              SDL_WINDOWPOS_UNDEFINED,  
                              videoparameter._width,  videoparameter._height,  
                              SDL_WINDOW_OPENGL);  
    renderer = SDL_CreateRenderer(screen, -1, 0);  
      
      
    if(!screen) {  
        fprintf(stderr, "SDL: could not set video mode - exiting\n");  
        exit(1);  
    }  
    bmp = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_YV12,
			SDL_TEXTUREACCESS_STREAMING,
				videoparameter._width,videoparameter._height);
    player->SetVideoCallBack(bmp,VideoPlay);    
    player->Run();
    SDL_DestroyTexture(bmp); 
    if (s){
        pa_simple_free(s); 
    }
    
 
}
