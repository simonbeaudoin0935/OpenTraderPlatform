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

/**
 * @brief Market trading session
 *
 * Represents the current trading session based on time of day (ET).
 */
enum class TradingSession : quint8
{
    EarlyPreMarket, ///< Early pre-market: 4:01 AM - 6:00 AM ET
    PreMarket,      ///< Pre-market: 6:01 AM - 9:30 AM ET
    Regular,        ///< Regular trading hours: 9:31 AM - 4:00 PM ET
    AfterHours,     ///< After-hours: 4:01 PM - 8:00 PM ET
    Closed          ///< Market closed: 8:01 PM - 4:00 AM ET
};

/**
 * @brief Data source mode for the application
 *
 * Indicates whether data comes from live TradeStation streams or recorded replay data.
 * Replay mode has sub-states (stopped/playing/paused) managed by ReplayEngine.
 */
enum class DataSourceMode : quint8
{
    Live,  ///< Connected to live TradeStation streams
    Replay ///< Using recorded replay data
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

    /**
     * @brief Check if app is currently in replay mode
     * @return true if data source is Replay (regardless of playback state)
     */
    [[nodiscard]] static bool isInReplayMode();

    /**
     * @brief Get the current data source mode
     * @return Current DataSourceMode (Live or Replay)
     */
    [[nodiscard]] static DataSourceMode getDataSourceMode();

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
     * @brief Restart the application using execv()
     *
     * Replaces the current process with a fresh instance of the same executable.
     * This is used after changing trading mode to pick up the new API endpoint.
     * Does not return - the current process is replaced.
     */
    static void restartApplication();

    /**
     * @brief Get the current trading session based on current app time
     * @return Current TradingSession
     *
     * Works in both live mode (uses real time) and replay mode (uses replay time).
     */
    [[nodiscard]] static TradingSession getCurrentSession();

    /**
     * @brief Enter replay mode (switch data source from Live to Replay)
     *
     * Switches data source mode, emits first bar to populate chart, then pauses.
     * User sees populated chart in paused state, ready to play.
     * @param p_date Date to replay
     * @param p_startTime Start time within the day
     * @param p_speed Playback speed for when user clicks play
     */
    void enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

    /**
     * @brief Exit replay mode and resume live operation
     *
     * Coordinates: ReplayEngine stop, MainAlgo stream resume, TSClient mode switch.
     * Called from GUI thread.
     */
    void exitReplayMode();

    /**
     * @brief Start replay playback from specified date and time
     * @param p_date Date to replay
     * @param p_startTime Start time within the day
     * @param p_speed Playback speed
     *
     * Must be in replay mode first (call enterReplayMode()).
     * Starts the ReplayEngine which loads data and begins emission.
     */
    void startReplayPlayback(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

    /**
     * @brief Pause replay playback
     *
     * Pauses data emission but stays in replay mode. Time freezes.
     */
    void pauseReplayPlayback();

    /**
     * @brief Resume replay playback from paused state
     *
     * Resumes data emission from where it was paused.
     */
    void resumeReplayPlayback();

    /**
     * @brief Set replay playback speed on the fly
     * @param p_speed The new playback speed
     *
     * Can be called while replay is playing or paused.
     */
    void setReplaySpeed(ReplayEngine::PlaybackSpeed p_speed);

    /**
     * @brief Check if replay is currently paused
     */
    [[nodiscard]] bool isReplayPaused() const;

    /**
     * @brief Preload chart with data from a different day/time while in paused state
     *
     * Used when user changes day or start time selection before pressing play.
     * Reloads the replay data for the new day/time and updates chart without
     * affecting the Playing/Paused state. Only works when in replay mode.
     *
     * @param p_date New date to preload
     * @param p_startTime New start time within the day
     * @param p_speed Current playback speed setting
     */
    void preloadChartForReplay(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed);

  private:
    MainApp();
    ~MainApp();

    static MainApp* m_instance;
    static TradingMode m_tradingMode;
    static DataSourceMode m_dataSourceMode;

    TSClient* tradeStationClient;
    MainAlgo* mainAlgo;
    FrontEnd* appFrontend;
    MemoryMonitor memoryMonitor;
};
