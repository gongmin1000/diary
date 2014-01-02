//g++ -g -o DawnPlayer -D__STDC_CONSTANT_MACROS   DawnPlayer.cpp -I/home/dongrui/program/ffmpeg/include -L/home/dongrui/program/ffmpeg/lib   -lavformat -lavcodec  -lavdevice  -lavfilter  -lavutil  -lswresample  -lswscale -lz -lpthread -lboost_thread-mt -lSDL
//-fpermissive
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
  pFormatCtx(NULL),pFrame(NULL),
  _videoCodecCtx(NULL),_videoCodec(NULL),
  _audioCodecCtx(NULL),_audioCodec(NULL),
  _subtitleCodecCtx(NULL),_subtitleCodec(NULL),
  pFrameRGB(NULL),buffer(NULL),
  vframe_index(0),aframe_index(0),sframe_index(0){

    
}

DawnPlayer::~DawnPlayer(){
  if(buffer)
    av_free(buffer);
  
  if(pFrameRGB)
    av_free(pFrameRGB);

  if(pFrame)
    av_free(pFrame);

  if(_videoCodecCtx)
    avcodec_close(_videoCodecCtx);

  if(pFormatCtx)
    avformat_close_input(&pFormatCtx);
}

bool DawnPlayer::Init(char* Path){
  printf("Path = %s\n",Path);
  if( avformat_open_input(&pFormatCtx, Path, NULL, NULL) != 0 ){
    return false;
  }

  if( avformat_find_stream_info(pFormatCtx, NULL ) < 0 ){
    return false;
  }
  
  av_dump_format(pFormatCtx, -1, Path, 0);

  videoStream = -1;
  audioStream = -1;
  subtitleStream = -1;

  for( int i = 0; i < pFormatCtx->nb_streams; i++ ){

    switch( pFormatCtx->streams[i]->codec->codec_type){
      case AVMEDIA_TYPE_VIDEO:
	videoStream = i;
	break;
      case AVMEDIA_TYPE_AUDIO:
	audioStream = i;
	break;
      case AVMEDIA_TYPE_SUBTITLE:
	subtitleStream = i;
	break;
    }
  }

  if( videoStream == -1 ){
    printf("没有发现视频流\n");
    //return false;
  }
  else{
    //初始化视频解码器
    _videoCodecCtx = pFormatCtx->streams[videoStream]->codec;

    _videoCodec = avcodec_find_decoder(_videoCodecCtx->codec_id);

    if( _videoCodec == NULL ){
      printf("没有发现视频解码器\n");
      return false;
    }

    if( avcodec_open2(_videoCodecCtx, _videoCodec, NULL) < 0 ){
      return false;
    }
  }

  pFrame = avcodec_alloc_frame();

  if( pFrame == NULL ){
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
  if( audioStream == -1 ){
    printf("没有发现音频流\n");
    //return false;
  }
  else{
    _audioCodecCtx = pFormatCtx->streams[audioStream]->codec;

    _audioCodec = avcodec_find_decoder(_audioCodecCtx->codec_id);
    
    if( _audioCodec == NULL ){
      printf("没有发现音频解码器\n");
      return false;
    }
    
    if (avcodec_open2(_audioCodecCtx, _audioCodec, NULL) < 0){
      return false;
    }
  }
  
  //初始化字幕解码器
  if( subtitleStream == -1){
    printf("没有发现字幕流\n");
    //return false;
  }
  else{
    _subtitleCodecCtx = pFormatCtx->streams[subtitleStream]->codec;

    _subtitleCodec = avcodec_find_decoder(_subtitleCodecCtx->codec_id);
    if( _subtitleCodec == NULL ){
      printf("没有发现字幕解码器\n");
      return false;
    }
  }
  
  _thr = boost::thread(boost::bind(&DawnPlayer::Run,this));

  return true;
}

void DawnPlayer::VideoDecode(){
  int ret = 0;
  ret = avcodec_decode_video2(_videoCodecCtx, pFrame, &frameFinished, &packet);
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

    sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data,
	      pFrame->linesize, 0, _videoCodecCtx->height, 
	      pFrameRGB->data,pFrameRGB->linesize);

    //if( frame_index++ < 50 ){
      vframe_index++;
      //SaveFrame(pFrameRGB, _videoCodecCtx->width, _videoCodecCtx->height, frame_index);
    //}
  }
}

void DawnPlayer::AudioDecode(){
  int ret = 0;
  ret = avcodec_decode_audio4(_audioCodecCtx, pFrame, &frameFinished, &packet);
  if ( ret < 0) {
    // if error, we skip the frame 
    packet.size = 0;
  }
  else{
    aframe_index++;
    
  }
}

void DawnPlayer::SubtitleDecode(){
  int ret = 0;
  AVSubtitle sub;
  avcodec_decode_subtitle2(_subtitleCodecCtx, &sub,&frameFinished, &packet);
  avsubtitle_free(&sub);
  sframe_index++;
}

void DawnPlayer::Run(){
  int ret = 0;
  int eof = 0;

  while( 1 ) {
    ret = av_read_frame(pFormatCtx, &packet);
    if (ret < 0) {
      print_error(NULL,ret);
      if (ret == AVERROR_EOF || url_feof(pFormatCtx->pb)){
	eof = 1;
	break;
	
      }
      
      /*if (_ic->pb && _ic->pb->error)
	break;*/
      continue;
    }
    
    avcodec_get_frame_defaults(pFrame);
    //printf("packet.stream_index = %d\n",packet.stream_index);
    
    if( _videoCodecCtx && packet.stream_index == videoStream ) {
      VideoDecode();
    }
    else if( _audioCodecCtx && packet.stream_index == audioStream ){
      AudioDecode();
    }
    else if( _subtitleCodecCtx && packet.stream_index == subtitleStream ){
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



int main(int argc,char* argv[]){
  char buf[1024];
  DawnPlayer *player = NULL;
  av_register_all();
  
  
  for(int i = 0 ; i < 100 ; i++ ){
    /*if( player ){
      delete player;
    }*/
    player = new DawnPlayer();
    sprintf(buf,"%s%d%s","/home/dongrui/test",1,".avi");
    if( !player->Init(buf) ){
      return 0;
      
    }
    sleep(10);
    //player->Run();
  }
  player->_thr.join();
}
