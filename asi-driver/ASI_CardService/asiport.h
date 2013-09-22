#ifndef ASIPORT_H
#define ASIPORT_H
#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/thread.hpp>

#include "asidev.h"

class AsiPort
{
public:
    AsiPort(uint8_t num,std::string direction,
                boost::asio::ip::address_v4 ip,uint16_t ip_port,AsiDev& dev);
    bool Init();
    inline std::string Direction(){return  _direction;};
    inline uint8_t ChannNum(){return _num;};

private:
    bool MulticastAddress(boost::asio::ip::address_v4 ip);
    uint8_t _num;
    std::string _direction;
    boost::asio::ip::address_v4 _ip;
    uint16_t _ip_port;
    AsiDev _dev;
    boost::thread _thread;
    boost::condition_variable _cond;
    boost::mutex _mut;
    bool _init_succeful;
    void Run();
    void SendLoop();
    void RevLoop();
    #define PLAY_LOAD_BUF_SIZE (4096*188)
    char _wbuf[PLAY_LOAD_BUF_SIZE];
    char _rbuf[PLAY_LOAD_BUF_SIZE];
};

#endif // ASIPORT_H
