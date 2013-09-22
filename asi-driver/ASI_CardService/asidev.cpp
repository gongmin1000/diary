#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <iostream>

#include "asidev.h"

AsiDev::AsiDev()
{
    close(_fd);
}

bool AsiDev::Init()
{
    _fd = open("/dev/asi0",S_IRUSR|S_IWUSR);
    if( _fd < 0 ){
        std::cout << "/dev/asi0不能打开" << std::endl;
        return false;
    }

    _buf0 = boost::circular_buffer_space_optimized<struct PlayLoad>(4096);
    _buf1 = boost::circular_buffer_space_optimized<struct PlayLoad>(4096);
    _buf2 = boost::circular_buffer_space_optimized<struct PlayLoad>(4096);
    _buf3 = boost::circular_buffer_space_optimized<struct PlayLoad>(4096);
    return true;
}

int AsiDev::Write(int chan_num,char* buf,int size)
{
    int ret;
    int i,k;
    for(i = 0, k = 0 ; (i < PLAY_LOAD_NUM) && ( k < size ) ; i++ , k += 188 ){
        _wbuf[i].chan_num = chan_num;
        memcpy(_wbuf[i].ts_data,buf+k,188);
    }
    ret = 0;
    while(i){
        ret = 0;
        ret = write(_fd,&_wbuf[PLAY_LOAD_NUM - i],i);
        i -= ret;
    }
    return k;
}


int AsiDev::Read(int chan_num,char* buf,int size)
{
    boost::circular_buffer_space_optimized<struct PlayLoad> *tsbuf;

    switch (chan_num) {
    case 0:
        tsbuf = &_buf0;
        break;
    case 1:
        tsbuf = &_buf1;
        break;
    case 2:
        tsbuf = &_buf2;
        break;
    case 3:
        tsbuf = &_buf3;
        break;
    default:
        break;
    }
    int i;
    for(i = 0 ; (i < size) && (tsbuf->empty() == false) ; i += 188 ){
        memcpy(buf+i,&tsbuf->front().ts_data[0],188);
        tsbuf->pop_front();
    }

    return i;
}

void AsiDev::Run(){
    int ret;
    while(1){
        ret = 0;
        ret = read(_fd,_rbuf,PLAY_LOAD_NUM);
        for(int i = 0 ; i < ret ; i++ ){
            switch(_rbuf[i].chan_num){
            case 0:
                _buf0.push_back(_rbuf[i]);
                break;
            case 1:
                _buf1.push_back(_rbuf[i]);
                break;
            case 2:
                _buf2.push_back(_rbuf[i]);
                break;
            case 3:
                _buf3.push_back(_rbuf[i]);
                break;
            }

        }
    }
}
