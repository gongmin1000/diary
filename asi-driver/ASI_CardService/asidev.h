#ifndef ASIDEV_H
#define ASIDEV_H
#include <stdint.h>

#include <boost/circular_buffer.hpp>


struct PlayLoad{
    uint32_t chan_num;
    uint8_t ts_data[188];
};

class AsiDev
{
public:
    AsiDev();
    bool Init();
    int Write(int chan_num,char* buf,int size);
    int Read(int chan_num,char* buf,int size);
    void Run();
private:
    int _fd;
#define PLAY_LOAD_NUM 4096
    struct PlayLoad _rbuf[PLAY_LOAD_NUM];
    struct PlayLoad _wbuf[PLAY_LOAD_NUM];
    boost::circular_buffer_space_optimized<struct PlayLoad> _buf0;
    boost::circular_buffer_space_optimized<struct PlayLoad> _buf1;
    boost::circular_buffer_space_optimized<struct PlayLoad> _buf2;
    boost::circular_buffer_space_optimized<struct PlayLoad> _buf3;

};

#endif // ASIDEV_H
