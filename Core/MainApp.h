#ifndef MAINAPP_H
#define MAINAPP_H

#include "../Clients/FMPClient/FMPClient.h"
#include "../Clients/TSClient/TSClient.h"
#include "AppFrontend.h"
#include "../Algo/MainAlgo.h"
#include "MemoryMonitor.h"

class MainApp
{
public:
    MainApp(AppFrontend* appFrontend);

    void start();

private:
    AppFrontend* appFrontend;
    FMPClient*   fmpClient;
    TSClient* tradeStationClient;
    MainAlgo*    mainAlgo;
    MemoryMonitor memoryMonitor;
};

#endif // MAINAPP_H
