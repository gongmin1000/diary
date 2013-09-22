#include <iostream>
#include <vector>

#include "asidev.h"
#include "asiport.h"
#include "config.h"

using namespace std;

int main()
{
    cout << "Hello World!" << endl;
    vector<AsiPort*> AsiPorts;
    Config config;
    AsiDev dev;

    if( dev.Init() == false ){
        std::cout << "设备不能打开" << std::endl;
        return 0;
    }


    config.Analyse("config.xml",dev, AsiPorts);

    dev.Run();


    return 0;
}

