#pragma once

#include "TSClient.h"
#include "FrontEnd.h"
#include "MainAlgo.h"
#include "MemoryMonitor.h"
#include "Core/Replay/ReplayEngine.h"

/**
 * @brief Trading mode for TradeStation API connection
 *
 * Determines which API endpoint is used:
 * - Sim: Uses sim-api.tradestation.com (simulated/paper trading)
 * - Live: Uses api.tradestation.com (real money trading)
 */
enum class TradingMode : quint8
{
    Sim, ///< Simulated trading (paper money)
    Live ///< Live trading (real money)
};

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
     * @brief Get the current trading mode (Sim or Live)
     * @return Current TradingMode
     */
    [[nodiscard]] static TradingMode getTradingMode();

    /**
     * @brief Set the trading mode and persist to settings
     * @param p_mode New trading mode
     *
     * This saves to AppState.ini. A restart is required for the change to take effect.
     */
    static void setTradingMode(TradingMode p_mode);

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
    static TradingMode m_tradingMode;

    TSClient* tradeStationClient;
    MainAlgo* mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
