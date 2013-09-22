#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "asiport.h"

AsiPort::AsiPort(uint8_t num,std::string direction,
                 boost::asio::ip::address_v4 ip,uint16_t ip_port,AsiDev& dev)
    :_num(num),_direction(direction),_ip(ip),_dev(dev),
      _ip_port(ip_port),_init_succeful(true)
{
}

bool AsiPort::MulticastAddress(boost::asio::ip::address_v4 ip)
{
    return true;
}

bool AsiPort::Init()
{
    boost::unique_lock<boost::mutex> lock(_mut);
    _thread = boost::thread( boost::bind(&AsiPort::Run,this));
    _cond.wait(lock);
    return _init_succeful;
}

void AsiPort::Run()
{
    if( _direction.compare("send") == 0 ){
        SendLoop();
    }
    else{
        RevLoop();
    }

}

void AsiPort::SendLoop()
{


    int sockfd;
    int i=1;
    socklen_t len = sizeof(i);
    struct sockaddr_in server_addr;
    // create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(MulticastAddress(_ip)){
        setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&i,len);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_ip_port);
    server_addr.sin_addr.s_addr = inet_addr(_ip.to_string().c_str());

    _cond.notify_all();
    if( _init_succeful == false ){
        return ;
    }

    size_t ret;
    while(1){
        ret = 0;

        ret = _dev.Read(_num,_rbuf,PLAY_LOAD_BUF_SIZE);

        sendto(sockfd,_rbuf,ret,0,
               (struct sockaddr*)&server_addr,
               (socklen_t)sizeof(struct sockaddr_in));

    }
    close(sockfd);
}

void AsiPort::RevLoop()
{


    int sockfd;

    struct sockaddr_in addr,client_addr;
    socklen_t addrlen;

    sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&addr,sizeof(struct sockaddr_in));

    addr.sin_family=AF_INET;

    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    addr.sin_port=htons(_ip_port);

    if(bind(sockfd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))<0){
            perror("bind");
            _init_succeful = false;
    }


    _cond.notify_all();
    if( _init_succeful == false ){
        return ;
    }


    size_t ret;
    size_t write_size;
    addrlen = sizeof(struct sockaddr_in);

    while (true)
    {

        ret = 0;
        ret = recvfrom(sockfd,_wbuf,PLAY_LOAD_BUF_SIZE,0,
                       (struct sockaddr*)&client_addr,
                       &addrlen);
        write_size = ret;
        while(1){
            ret = _dev.Write(_num,_wbuf,write_size);
            write_size -= ret;
        }
    }
}
