#pragma once

#include "FMPClient.h"
#include "TSClient.h"
#include "AppFrontend.h"
#include "MainAlgo.h"
#include "MemoryMonitor.h"

class MainApp
{
public:
    MainApp();

    void start();

private:
    FMPClient*   fmpClient;
    TSClient* tradeStationClient;
    MainAlgo*    mainAlgo;
    AppFrontend* appFrontend;
    MemoryMonitor memoryMonitor;
};
