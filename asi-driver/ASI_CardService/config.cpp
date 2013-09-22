#include <iostream>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>


#include "config.h"

#include "tinyxml2.h"


using namespace boost;

Config::Config(void)
{

}


Config::~Config(void)
{

}

bool Config::Analyse(const char* config_path ,AsiDev& dev,std::vector<AsiPort*>& AsiPorts)
{
    uint8_t num;
    std::string direction;
    boost::asio::ip::address_v4 ip;
    uint16_t ip_port;
    AsiPort* asi_port;
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError error = doc.LoadFile( config_path );
    if(  error != tinyxml2::XML_SUCCESS ){
        return false;
    }
    tinyxml2::XMLElement *ASI_PORTS = doc.RootElement();
    if( ASI_PORTS == NULL ){
        return false;
    }
    tinyxml2::XMLElement *port = ASI_PORTS->FirstChildElement("port");
    while( port ){
        if( asi_port != NULL ){
            std::stringstream num_str(port->GetText());
            num_str >> num;
            std::cout << "///////////////////////////////////" << std::endl;
            std::cout << num << std::endl;


            const tinyxml2::XMLAttribute *attributeOfPort = port->FirstAttribute();
            while ( attributeOfPort != NULL ){
                if( strcmp("direction" , attributeOfPort->Name() ) == 0  ){
                    if( strcmp("send" , attributeOfPort->Value() ) == 0){
                        direction = attributeOfPort->Value();
                    }
                    else if( strcmp("rev" , attributeOfPort->Value() ) == 0){
                        direction = attributeOfPort->Value();
                    }
                    else{
                            std::cout << "direction标签错误" << std::endl;
                            return false;
                    }

                    std::cout << "direction=" << direction << std::endl;
                }
                else if( strcmp("ip" , attributeOfPort->Name() ) == 0  ){
                    ip = boost::asio::ip::address_v4::from_string(attributeOfPort->Value());
                    std::cout << "ip = " << ip.to_string() << std::endl;
                }
                else if(  strcmp("ip_port" , attributeOfPort->Name() ) == 0  ){
                    std::stringstream ss(attributeOfPort->Value());
                    ss >> ip_port;
                    std::cout << "ip_port = " << ip_port << std::endl;
                }
                attributeOfPort = attributeOfPort->Next();
            }

        }

        asi_port = new AsiPort(num,direction,ip,ip_port,dev);
        AsiPorts.push_back(asi_port);

        port = port->NextSiblingElement();
    }

    for(int i = 0; i < AsiPorts.size(); i++ ){
        if( AsiPorts[i]->Init() == false ){
            return false;
        }
    }


    return true;
}
