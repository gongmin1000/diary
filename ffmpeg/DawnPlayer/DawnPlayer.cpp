//g++ -g -o DawnPlayer -D__STDC_CONSTANT_MACROS   DawnPlayer.cpp -I/home/dongrui/program/ffmpeg/include -L/home/dongrui/program/ffmpeg/lib   -lavformat -lavcodec  -lavdevice  -lavfilter  -lavutil  -lswresample  -lswscale -lz -lpthread -lboost_thread-mt -lSDL
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
  _FormatCtx(NULL),_VideoFrame(NULL),pFrameRGB(NULL),
  _videoCodecCtx(NULL),_videoCodec(NULL),
  _audioCodecCtx(NULL),_audioCodec(NULL),
  _subtitleCodecCtx(NULL),_subtitleCodec(NULL),
  buffer(NULL),_AudioFrame(NULL),
  _AudioCallBack(NULL),
  vframe_index(0),aframe_index(0),sframe_index(0){

    
}

DawnPlayer::~DawnPlayer(){
  if(buffer)
    av_free(buffer);
  
  if(pFrameRGB)
    av_free(pFrameRGB);

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
}


void DawnPlayer::SetAudioCallBack(void* data,AudioCallBack callback)
{
  _AudioCallBack = callback;
  _AudioCallBackData = data;
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
  }

  _VideoFrame = avcodec_alloc_frame();

  if( _VideoFrame == NULL ){
    return false;
  }

  pFrameRGB = avcodec_alloc_frame();

  if( pFrameRGB == NULL ){

    return false;
  }
  
  numBytes = avpicture_get_size(PIX_FMT_RGB24, 
				  _videoCodecCtx->width,
				  _videoCodecCtx->height);
  
  buffer = (uint8_t*)av_malloc(numBytes);

  avpicture_fill( (AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,

          _videoCodecCtx->width, _videoCodecCtx->height);
  
  
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

void DawnPlayer::VideoDecode(){
  int ret = 0;
  avcodec_get_frame_defaults(_VideoFrame);
  ret = avcodec_decode_video2(_videoCodecCtx, _VideoFrame, &frameFinished, &packet);
  if( ret < 0 ){
    printf("avcodec_decode_video2 error ret = %d\n",ret);
  }
  if( frameFinished ) {
    struct SwsContext *img_convert_ctx = NULL;

    img_convert_ctx = sws_getCachedContext(
		img_convert_ctx, _videoCodecCtx->width,

                 _videoCodecCtx->height, _videoCodecCtx->pix_fmt,

                 _videoCodecCtx->width, _videoCodecCtx->height,

                 PIX_FMT_RGB24, SWS_BICUBIC,

                 NULL, NULL, NULL);

    if( !img_convert_ctx ) {

      fprintf(stderr, "Cannot initialize sws conversion context\n");

      exit(1);

      
    }

    sws_scale(img_convert_ctx, (const uint8_t* const*)_VideoFrame->data,
	      _VideoFrame->linesize, 0, _videoCodecCtx->height, 
	      pFrameRGB->data,pFrameRGB->linesize);

    //if( vframe_index++ < 50 ){
      vframe_index++;
      //SaveFrame(pFrameRGB, _videoCodecCtx->width, _videoCodecCtx->height, vframe_index);
    //}
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
        _AudioCallBack(_AudioCallBackData,_AudioFrame->data[0],data_size);
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
 
  for(int i = 0 ; i < 1 ; i++ ){
    /*if( player ){
      delete player;
    }*/
    player = new DawnPlayer();
    sprintf(buf,"%s%d%s","/home/dongrui/test_",i,".ts");
    //sprintf(buf,"%s%s","/home/dongrui/","7907.flv");//PA_SAMPLE_S16NE
    //sprintf(buf,"%s%s","/home/dongrui/","晓松说.ts");//PA_SAMPLE_FLOAT32LE
    if( !player->Init(buf) ){
      return 0;
      
    }
    //sleep(10);
    player->GetAudioParameter(&parameter);
    ss.format = PA_SAMPLE_S16BE;//PA_SAMPLE_FLOAT32LE;//PA_SAMPLE_S16NE;
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
    player->Run();
    if (s){
        pa_simple_free(s); 
    }
  }
  player->_thr.join();
 
}
