#include "MainApp.h"
#include "Assume.h"
#include "DatabaseThread.h"
#include "Logging.h"
#include "Settings.h"
#include "Stream.h"
#include "CONSTANTS.h"
#include <QCoreApplication>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#ifdef GUI_ENABLED
#include "GUIFrontend.h"
#else
#include "TUIFrontend.h"
#endif

QDateTime MainApp::currentAppReplayTime = QDateTime::fromSecsSinceEpoch(0);

// Initialize static members
MainApp* MainApp::m_instance = nullptr;
TradingMode MainApp::m_tradingMode = TradingMode::Sim; // Default to Sim for safety
DataSourceMode MainApp::m_dataSourceMode = DataSourceMode::Live;

MainApp* MainApp::getInstance()
{
    if (m_instance == nullptr)
    {
        // Load trading mode from settings before creating instance
        // (TSClient needs this during construction)
        Q_CHECK_PTR(appStateSettings);
        int savedMode = appStateSettings->value("Trading/Mode", static_cast<int>(TradingMode::Sim)).toInt();
        m_tradingMode = static_cast<TradingMode>(savedMode);
        qInfo() << "Trading mode loaded:" << (m_tradingMode == TradingMode::Sim ? "SIM" : "LIVE");

        qInfo() << "MainApp singleton instance created";
        m_instance = new MainApp();
    }
    return m_instance;
}

void MainApp::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);
    qInfo() << "Destroying MainApp singleton instance";
    delete m_instance;
    m_instance = nullptr;
}

bool MainApp::isInReplayMode()
{
    return m_dataSourceMode == DataSourceMode::Replay;
}

DataSourceMode MainApp::getDataSourceMode()
{
    return m_dataSourceMode;
}

// Get the current application time (real or replay)
QDateTime MainApp::getCurrentAppTime()
{
    if (isInReplayMode())
    {
        return currentAppReplayTime;
    }
    return QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE);
}

TradingMode MainApp::getTradingMode()
{
    return m_tradingMode;
}

void MainApp::setTradingMode(TradingMode p_mode)
{
    m_tradingMode = p_mode;

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("Trading/Mode", static_cast<int>(p_mode));
    appStateSettings->sync();

    qInfo() << "Trading mode set to" << (p_mode == TradingMode::Sim ? "SIM" : "LIVE") << "(requires restart)";
}

void MainApp::restartApplication()
{
    // Get the executable path
    QString executablePath = QCoreApplication::applicationFilePath();

    // Get command line arguments (excluding the first which is the program name)
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // Remove program name

    // Convert to C-style arrays for execv()
    QByteArrayList argsByteArrays;
    argsByteArrays.reserve(args.size() + 2); // +2 for program name and null terminator

    // Add program name
    argsByteArrays.append(executablePath.toLocal8Bit());

    // Add other arguments
    for (const QString& arg: args)
    {
        argsByteArrays.append(arg.toLocal8Bit());
    }

    // Build argv array (must be null-terminated)
    std::vector<char*> argv;
    argv.reserve(argsByteArrays.size() + 1);

    for (QByteArray& ba: argsByteArrays)
    {
        argv.push_back(ba.data());
    }
    argv.push_back(nullptr); // Null terminator required by execv()

    qInfo() << "Restarting application via execv()";

    // execv() replaces the current process - doesn't return on success
    execv(executablePath.toLocal8Bit().constData(), argv.data());

    // If we reach here, execv() failed
    qCritical() << "execv() failed:" << strerror(errno);
    QCoreApplication::exit(1);
}

TradingSession MainApp::getCurrentSession()
{
    QTime currentTime = getCurrentAppTime().time();

    // Check each session in order
    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        return TradingSession::EarlyPreMarket;
    }

    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION)
    {
        return TradingSession::PreMarket;
    }

    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_REGULAR_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_REGULAR_SESSION)
    {
        return TradingSession::Regular;
    }

    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return TradingSession::AfterHours;
    }

    // Default: market closed
    return TradingSession::Closed;
}

MainApp::MainApp() : tradeStationClient(TSClient::getInstance()), mainAlgo(MainAlgo::getInstance())
{
#ifdef GUI_ENABLED
    appFrontend = new GUIFrontend(mainAlgo);
#else
    appFrontend = new TUIFrontend(mainAlgo);
#endif
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &FrontEnd::onMemoryUsageUpdate);

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    QObject::connect(tradeStationClient,
                     &TSClient::authStateChanged,
                     appFrontend,
                     &FrontEnd::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient,
                     &TSClient::authStateChanged,
                     mainAlgo,
                     &MainAlgo::onTradeStationAuthStateChanged);

    QObject::connect(mainAlgo,
                     &MainAlgo::tradeStationAccountsReceived,
                     appFrontend,
                     &FrontEnd::onTradeStationAccountsReceived);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient,
                     &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend,
                     &FrontEnd::onTSClientDataUsageUpdate);

    // Connect TradeStation stream count updates to frontend
    QObject::connect(tradeStationClient,
                     &TSClient::openStreamCountChanged,
                     appFrontend,
                     &FrontEnd::onStreamCountUpdate);

    QObject::connect(mainAlgo,
                     &MainAlgo::displayedStockReceivedNewBar,
                     appFrontend,
                     &FrontEnd::onCurrentHighlightedStockBarReceived);

    QObject::connect(mainAlgo,
                     &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
                     appFrontend,
                     &FrontEnd::onCurrentHighlightedReceivedNewMarketDepthQuote);


    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition, appFrontend, &FrontEnd::onNewPositionReceived);

    QObject::connect(mainAlgo, &MainAlgo::positionDeleted, appFrontend, &FrontEnd::onPositionDeleted);

    QObject::connect(mainAlgo, &MainAlgo::receivedNewOrder, appFrontend, &FrontEnd::onNewOrderReceived);

    QObject::connect(mainAlgo, &MainAlgo::balanceUpdated, appFrontend, &FrontEnd::onBalanceUpdated);

    // Replay mode signals
    QObject::connect(mainAlgo, &MainAlgo::replayTimeUpdated, appFrontend, &FrontEnd::onReplayTimeUpdated);
}

MainApp::~MainApp()
{
    qInfo() << "MainApp destructor - cleaning up";

    // Stop memory monitoring first to avoid cross-thread timer warnings
    memoryMonitor.stopMonitoring();

    // Delete the frontend first
    delete appFrontend;
    appFrontend = nullptr;

    // The singletons will be cleaned up by their own destructors
}

void MainApp::start()
{
    // Start the database thread first (other threads may depend on it)
    DatabaseThread::getInstance()->start();

    // start the other threads
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);

#ifndef GUI_ENABLED
    // Initialize TUI after everything is set up
    static_cast<TUIFrontend*>(appFrontend)->initialize();
#endif
}

void MainApp::shutdown()
{
    qInfo() << "MainApp shutdown initiated - stopping all threads and cleaning up";

    // Stop memory monitoring first
    memoryMonitor.stopMonitoring();
    qInfo() << "Memory monitor stopped";

    // Stop threads in reverse order of startup
    // MainAlgo should stop before TSClient since it depends on it
    if (mainAlgo)
    {
        // stopBalancePolling must be called on MainAlgo's thread
        QMetaObject::invokeMethod(mainAlgo, &MainAlgo::stopBalancePolling, Qt::BlockingQueuedConnection);
        qInfo() << "MainAlgo balance polling stopped";
    }

    // Now quit the application - destructors will be called automatically
    // when the MainApp object goes out of scope in main()
    QCoreApplication::quit();
}

void MainApp::cleanupSingletons()
{
    qInfo() << "Cleaning up singletons";

    // Set shutdown flag to prevent Stream destructors from finishing promises
    Stream::setShuttingDown(true);

    // Delete MainApp singleton first (which will delete the frontend)
    MainApp::destroyInstance();

    // Delete MainAlgo singleton (which will stop its thread)
    MainAlgo::destroyInstance();

    // Delete TSClient singleton (which will stop its thread)
    TSClient::destroyInstance();

    // Delete DatabaseThread singleton (which will stop its thread)
    DatabaseThread::destroyInstance();

    qInfo() << "All singletons cleaned up";
}

void MainApp::enterReplayMode(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Live && "enterReplayMode called when already in replay mode");

    qInfo() << "Entering replay mode for" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString("hh:mm:ss");

    // 1. Capture currently displayed symbol before we delete everything
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();
    if (displayedSymbol.isEmpty())
    {
        qWarning() << "No displayed symbol, using default AAPL";
        displayedSymbol = "AAPL";
    }

    // 2. Set data source mode
    m_dataSourceMode = DataSourceMode::Replay;

    // 3. Initialize replay time to start time (will be refined when first bar emits)
    currentAppReplayTime = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE);

    // 4. Switch TSClient to replay mode (blocking to ensure mode is set before streams open)
    QMetaObject::invokeMethod(
        tradeStationClient,
        [this]() { tradeStationClient->setMode(TSClient::Mode::Replay); },
        Qt::BlockingQueuedConnection);

    // 5. Clean slate: stop everything and recreate fresh, then start replay paused (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol, p_date, p_startTime, p_speed]()
        {
            // Stop all running strategies
            mainAlgo->stopAllStrategies();

            // Close positions/orders streams
            mainAlgo->pauseLiveStreams();

            // Delete all stock instruments (and their streams)
            mainAlgo->deleteAllStockInstruments();

            // Create fresh stock instrument with mock-backed streams
            mainAlgo->createAndSetDisplayedStockInstrument(displayedSymbol);

            // Start replay in paused state - emits first bar to populate chart
            mainAlgo->enterReplayModePaused(p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    // 6. Update UI
    appFrontend->onReplayModeEntered();

    qInfo() << "Replay mode entered with chart pre-populated";
}

void MainApp::exitReplayMode()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "exitReplayMode called when not in replay mode");

    qInfo() << "Exiting replay mode";

    // 1. Capture currently displayed symbol before we delete everything
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();
    if (displayedSymbol.isEmpty())
    {
        qWarning() << "No displayed symbol, using default AAPL";
        displayedSymbol = "AAPL";
    }

    // 2. Tell MainAlgo to stop replay and clean up (blocking to ensure clean stop)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this]()
        {
            // Stop replay engine if running
            mainAlgo->exitReplayMode();

            // Stop all replay strategies
            mainAlgo->stopAllStrategies();

            // Delete all replay stock instruments
            mainAlgo->deleteAllStockInstruments();
        },
        Qt::BlockingQueuedConnection);

    // 3. Reset data source mode
    m_dataSourceMode = DataSourceMode::Live;

    // 4. Switch TSClient back to live mode (blocking)
    QMetaObject::invokeMethod(
        tradeStationClient,
        [this]() { tradeStationClient->setMode(TSClient::Mode::Live); },
        Qt::BlockingQueuedConnection);

    // 5. Recreate fresh live instruments and resume streams (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol]()
        {
            // Reopen positions/orders streams
            mainAlgo->resumeLiveStreams();

            // Create fresh stock instrument with live streams
            mainAlgo->createAndSetDisplayedStockInstrument(displayedSymbol);
        },
        Qt::QueuedConnection);

    // 6. Update UI
    appFrontend->onReplayModeExited();

    qInfo() << "Replay mode exited, live mode resumed";
}

void MainApp::startReplayPlayback(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "startReplayPlayback called when not in replay mode");

    qInfo() << "Starting replay playback for" << p_date.toString(Qt::ISODate) << "at"
            << p_startTime.toString("hh:mm:ss");

    // Tell MainAlgo to start replay (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, p_date, p_startTime, p_speed]() { mainAlgo->enterReplayMode(p_date, p_startTime, p_speed); },
        Qt::QueuedConnection);

    qInfo() << "Replay playback start initiated";
}

void MainApp::pauseReplayPlayback()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "pauseReplayPlayback called when not in replay mode");

    qInfo() << "Pausing replay playback";

    QMetaObject::invokeMethod(mainAlgo, [this]() { mainAlgo->pauseReplay(); }, Qt::QueuedConnection);
}

void MainApp::resumeReplayPlayback()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "resumeReplayPlayback called when not in replay mode");

    qInfo() << "Resuming replay playback";

    QMetaObject::invokeMethod(mainAlgo, [this]() { mainAlgo->resumeReplay(); }, Qt::QueuedConnection);
}

void MainApp::setReplaySpeed(ReplayEngine::PlaybackSpeed p_speed)
{
    QMetaObject::invokeMethod(mainAlgo, [this, p_speed]() { mainAlgo->setReplaySpeed(p_speed); }, Qt::QueuedConnection);
}

bool MainApp::isReplayPaused() const
{
    return mainAlgo->getReplayState() == ReplayEngine::PlaybackState::Paused;
}

void MainApp::preloadChartForReplay(QDate p_date, QTime p_startTime, ReplayEngine::PlaybackSpeed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "preloadChartForReplay called when not in replay mode");

    qInfo() << "Preloading chart for replay:" << p_date.toString(Qt::ISODate) << "at"
            << p_startTime.toString("hh:mm:ss");

    // Get currently displayed symbol
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();

    // Reload chart data for new day/time (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol, p_date, p_startTime, p_speed]()
        {
            // Re-enter replay paused with new date/time
            // This stops existing replay, reloads data, and emits first bar to update chart
            mainAlgo->enterReplayModePaused(p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    qInfo() << "Chart preload initiated for" << displayedSymbol;
}

ReplayEngine* MainApp::getReplayEngine() const
{
    return mainAlgo->getReplayEngine();
}
