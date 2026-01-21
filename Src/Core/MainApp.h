#pragma once

#include "TSClient.h"
#include "FrontEnd.h"
#include "MainAlgo.h"
#include "MemoryMonitor.h"

class MainApp
{
  public:
    // Singleton: Instance getter
    static MainApp* getInstance();

    // Delete copy/move constructors and assignment operators
    MainApp(const MainApp&) = delete;
    MainApp(MainApp&&) = delete;
    MainApp& operator=(const MainApp&) = delete;
    MainApp& operator=(MainApp&&) = delete;

    void start();
    void shutdown();

    // Get the current application time (real or replay)
    static QDateTime currentAppReplayTime;

    static bool isInReplayMode;

    static QDateTime getCurrentAppTime();

  private:
    MainApp();
    ~MainApp();

    static MainApp* m_instance;

    TSClient* tradeStationClient;
    MainAlgo* mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
