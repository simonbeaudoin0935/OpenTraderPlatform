#pragma once

#include "FMPClient.h"
#include "TSClient.h"
#include "AppFrontend.h"
#include "MainAlgo.h"
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
