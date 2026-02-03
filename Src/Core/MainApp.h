#pragma once

#include "TSClient.h"
#include "FrontEnd.h"
#include "MainAlgo.h"
#include "MemoryMonitor.h"
#include "Core/Replay/ReplayEngine.h"

class MainApp
{
  public:
    // Singleton: Instance getter
    static MainApp* getInstance();

    // Singleton: Destroy instance (for cleanup)
    static void destroyInstance();

    // Cleanup all singletons (call before exiting for proper cleanup)
    static void cleanupSingletons();

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

    /**
     * @brief Enter replay mode for the specified date and time
     * @param p_date Date to replay
     * @param p_startTime Start time within the day
     * @param p_speed Playback speed
     *
     * Coordinates: TSClient mode switch, MainAlgo stream pause, ReplayEngine start.
     * Called from GUI thread (e.g., from ChartToolbar play button).
     */
    void enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

    /**
     * @brief Exit replay mode and resume live operation
     *
     * Coordinates: ReplayEngine stop, MainAlgo stream resume, TSClient mode switch.
     * Called from GUI thread.
     */
    void exitReplayMode();

  private:
    MainApp();
    ~MainApp();

    static MainApp* m_instance;

    TSClient* tradeStationClient;
    MainAlgo* mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
