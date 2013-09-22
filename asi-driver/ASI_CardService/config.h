#ifndef __CONFIG_H__
#define __CONFIG_H__
#include <stdint.h>
#include <vector>

#include "asidev.h"
#include "asiport.h"

/*
<ASI_PORTS>
    <port direction="send" ip="224.0.0.0" ip_port="9000">0</port>
    <port direction="rev" ip="224.0.0.1" ip_port="9000">1</port>
    <port direction="send" ip="224.0.0.2" ip_port="9000">2</port>
    <port direction="rev" ip="224.0.0.3" ip_port="9000">3</port>
</ASI_PORTS>
*/

class Config
{
public:
	Config(void);
	~Config(void);
    bool Analyse(const char* config_path ,AsiDev& dev,std::vector<AsiPort*>& AsiPorts);

};
#endif

