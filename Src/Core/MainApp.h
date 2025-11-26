#pragma once

#include "TSClient.h"
#include "FrontEnd.h"
#include "MainAlgo.h"
#include "MemoryMonitor.h"

class MainApp
{
public:
    MainApp();

    void start();

private:
    TSClient* tradeStationClient;
    MainAlgo*    mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
