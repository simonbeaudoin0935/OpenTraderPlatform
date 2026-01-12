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

    // Get the current application time (real or replay)
    static QDateTime currentAppReplayTime;

    static bool isInReplayMode;

    static QDateTime getCurrentAppTime();

private:
    TSClient* tradeStationClient;
    MainAlgo*    mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
