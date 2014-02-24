#include <unistd.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include <SDL2/SDL.h>  
#include <SDL2/SDL_thread.h>

#include <DawnPlayer.h>

#include <iostream>

DawnPlayer *player = NULL;
SDL_Texture    *bmp = NULL;  
SDL_Window     *screen = NULL;  
SDL_Rect        rect;
SDL_Renderer *renderer;
void quit( int code )    
{    
    SDL_Quit( );    
    /* Exit program. */    
    exit( code );    
}  

void resizeGL(int width,int height)
{
}

void handleKeyEvent( SDL_Keysym* keysym )    
{    
    switch( keysym->sym )    
    {    
    case SDLK_ESCAPE:    
        quit( 0 );    
        break;    
    case SDLK_RIGHT:
        player->Seek(-1000,1,SEEK_CUR);
        break;
    case SDLK_LEFT:
        player->Seek(1000,1,SEEK_CUR);
        break;
    case SDLK_SPACE:
        static PlaySpeed speed = X1SPEED;
        std::cout<<"Space"<<std::endl;  
        if( speed == X1SPEED ){
            player->SetPlaySpeed(PAUSE);
        }
        else{
            player->SetPlaySpeed(X1SPEED);
        }
        
        break;
    /*case :
        break;
    case :
        break;
    case :
        break;*/
    default:    
        break;    
    }    
}   
  
void handleEvents()    
{    
    // Our SDL event placeholder.    
    SDL_Event event;    
    //Grab all the events off the queue.    
    //while( SDL_PollEvent( &event ) ) {
    SDL_PollEvent( &event );
    {
        switch( event.type ) {    
        case SDL_KEYDOWN:    
            // Handle key Event    
            handleKeyEvent( &event.key.keysym );    
            break;    
        case SDL_QUIT:    
            // Handle quit requests (like Ctrl-c).    
            quit( 0 );    
            break;    
        case SDL_WINDOWEVENT:    
            if(event.window.event == SDL_WINDOWEVENT_RESIZED)  
            {  
                if ( screen )    
                {    
                    int tmpX,tmpY;  
                    SDL_GetWindowSize(screen,&tmpX,&tmpY);  
                    resizeGL(tmpX, tmpY);   
                      
                }    
            }  
            
            break;    
        }    
    }    
}


void VideoPlay(void* prv_data,AVFrame* frame){
    static bool sdl_init = false;
    if( sdl_init == false ){
        if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
            fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());  
            exit(1);  
        }
        screen = SDL_CreateWindow("My Game Window",  
                              SDL_WINDOWPOS_UNDEFINED,  
                              SDL_WINDOWPOS_UNDEFINED,  
                              frame->width,  frame->height,  
                              SDL_WINDOW_OPENGL);  
        renderer = SDL_CreateRenderer(screen, -1, 0);  
      
      
        if(!screen) {  
            fprintf(stderr, "SDL: could not set video mode - exiting\n");  
            exit(1);  
        }  
        bmp = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,
				SDL_TEXTUREACCESS_STREAMING,
				frame->width,frame->height);
        sdl_init = true;
    }

    rect.x = 0;  
    rect.y = 0;  
    rect.w = frame->width;  
    rect.h = frame->height;

    handleEvents();
    
    SDL_UpdateTexture( bmp, &rect, frame->data[0], frame->linesize[0] );
    SDL_RenderClear( renderer );  
    SDL_RenderCopy( renderer, bmp, &rect, &rect );  
    SDL_RenderPresent( renderer );

    
}


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
  av_register_all();

  int error;
  pa_simple *s;
  pa_sample_spec ss;
  pa_channel_map pacmap; 
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
   
    memset(&parameter,0,sizeof(parameter)); 
    player->GetAudioParameter(&parameter);
    ss.format = PA_SAMPLE_S16LE;//PA_SAMPLE_FLOAT32LE(晓松说用)
    ss.channels = parameter._channels;
    ss.rate = parameter._sample_rate;
    if( ss.channels > 0 ){
        pa_channel_map_init_auto(&pacmap, parameter._channels,
                                        PA_CHANNEL_MAP_WAVEEX);

        s = pa_simple_new(NULL,               // Use the default server.
                  argv[0],           // Our application's name.
                  PA_STREAM_PLAYBACK,
                  NULL,               // Use the default device.
                  "Audio",            // Description of our stream.
                  &ss,                // Our sample format.
                  &pacmap,               // Use default channel map
                  NULL,               // Use default buffering attributes.
                  &error               // Ignore error code.
                  );

        if( !s ){
            printf("error pulseaudio = %s\n",pa_strerror(error));
            return 0;
        }
        player->SetAudioCallBack(s,AudioPlay);
    }
    /////////////////////////////////////////////////////
    //
    VideoParameter videoparameter;
    player->GetVideoParameter(&videoparameter);
    player->SetVideoCallBack(NULL,VideoPlay);    
    player->Play();
    player->SetPlaySpeed(X4SPEED);
    sleep(2000);
    SDL_DestroyTexture(bmp); 
    if (s){
        pa_simple_free(s); 
    }
    
 
}

