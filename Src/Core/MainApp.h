#pragma once

#include <memory>

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
    TSClient* tradeStationClient;  // Singleton, not owned
    MainAlgo* mainAlgo;  // Singleton, not owned
    std::unique_ptr<FrontEnd> m_appFrontend;
    MemoryMonitor memoryMonitor;
};
