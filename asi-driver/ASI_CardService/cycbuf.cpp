#include <string.h>

#include "cycbuf.h"

CycBuf::CycBuf(unsigned int buf_size)
    :_buf_size(buf_size)
{
    _buf = new unsigned char[_buf_size+1];
    _buf_end = _buf + _buf_size+1;
    _start = _buf;
    _end = _buf;
}

CycBuf::~CycBuf()
{
    delete _buf;
}


unsigned int CycBuf::AddData(unsigned char* buf,int size)
{
    int ret,ret1,ret2;
    ret1 = 0;
    ret2 = 0;
    ret = 0;
    if( _start <= _end ){
        ret1 = _buf_end - _end;
        ret2 = _start - _buf;
        ret = ret1 + ret2 -1;
        if( ret < 1 ){//满
            return 0;
        }
        if( ret1 > size ){
            ret = size;
            memcpy(_end,buf,ret);
            _end += ret;
        }
        else{
            if( ret > size ){
                ret = size;
            }
            memcpy(_end,buf,ret1);
            memcpy(_buf,buf+ret1,ret - ret1);
            _end = _buf + (ret - ret1);

        }
    }
    else{
        ret = _start - _end - 1;
        if( ret < 1 ){
            return 0;
        }
        if( ret > size ){
            ret = size;
        }
        memcpy(_end,buf,ret);
        _end += ret;
    }
    return ret;
}

unsigned int CycBuf::GetData(unsigned char* buf,int size)
{
    int ret,ret1,ret2;
    ret1 = 0;
    ret2 = 0;
    ret = 0;
    if( _start <= _end ){
        ret = _end - _start -1;
        if( ret < 1){
            return 0;
        }
        if( ret > size ){
            ret = size;
        }
        memcpy(buf,_start,ret);
        _start += ret;
    }
    else{
        ret1 = _buf_end - _start;
        ret2 = _end - _buf;
        ret = ret1 + ret2 - 1;
        if( ret < 1){
            return 0;
        }
        if( ret1 > size ){
            ret = ret1;
            memcpy(buf,_start,ret);
            _start += ret;
        }
        else{
            if( ret > size ){
                ret = size;
            }
            memcpy(buf,_start,ret1);
            memcpy(buf+ret1,_buf,ret - ret1);
            _start = _buf + ret - ret1;
        }
    }
    return ret;
}
