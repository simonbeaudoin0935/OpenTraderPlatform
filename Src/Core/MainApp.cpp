#include "MainApp.h"
#include "Assume.h"
#include "DatabaseThread.h"
#include "Logging.h"
#include "PlatformControlProtocol.h"
#include "PlatformControlServer.h"
#include "Settings.h"
#include "Stream.h"
#include "CONSTANTS.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"
#include "DBClient.h"
#include <QCoreApplication>
#include <QJsonValue>
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

namespace
{
    [[nodiscard]] QString tradingModeToString(const TradingMode p_mode)
    {
        switch (p_mode)
        {
        case TradingMode::Live:
            return "live";
        case TradingMode::Sim:
        default:
            return "sim";
        }
    }

    [[nodiscard]] QString dataSourceModeToString(const DataSourceMode p_mode)
    {
        switch (p_mode)
        {
        case DataSourceMode::Replay:
            return "replay";
        case DataSourceMode::Live:
        default:
            return "live";
        }
    }

    [[nodiscard]] QString replayStateToString(const Playback::State p_state)
    {
        switch (p_state)
        {
        case Playback::State::Playing:
            return "playing";
        case Playback::State::Paused:
            return "paused";
        case Playback::State::Stopped:
        default:
            return "stopped";
        }
    }

    [[nodiscard]] QJsonObject makeControlResponse(bool p_ok,
                                                  const QString& p_message,
                                                  const QJsonObject& p_result = {},
                                                  const QString& p_error = {})
    {
        QJsonObject response;
        response["protocolVersion"] = PlatformControlProtocol::kProtocolVersion;
        response["ok"] = p_ok;
        response["message"] = p_message;
        if (!p_error.isEmpty())
        {
            response["error"] = p_error;
        }
        if (!p_result.isEmpty())
        {
            response["result"] = p_result;
        }
        return response;
    }

    [[nodiscard]] std::expected<QDate, QString> parseRequiredDate(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("date").toString().trimmed();
        if (rawValue.isEmpty())
        {
            return std::unexpected("Missing required 'date' argument (expected YYYY-MM-DD)");
        }

        const QDate date = QDate::fromString(rawValue, Qt::ISODate);
        if (!date.isValid())
        {
            return std::unexpected(QString("Invalid date '%1' (expected YYYY-MM-DD)").arg(rawValue));
        }

        return date;
    }

    [[nodiscard]] std::expected<QTime, QString> parseRequiredTime(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("startTime").toString().trimmed();
        if (rawValue.isEmpty())
        {
            return std::unexpected("Missing required 'startTime' argument (expected HH:MM[:SS])");
        }

        const QTime time = QTime::fromString(rawValue, Qt::ISODate);
        if (!time.isValid())
        {
            return std::unexpected(QString("Invalid startTime '%1' (expected HH:MM[:SS])").arg(rawValue));
        }

        return time;
    }

    [[nodiscard]] std::expected<Playback::Speed, QString> parseRequiredReplaySpeed(const QJsonObject& p_arguments)
    {
        const QJsonValue speedValue = p_arguments.value("speed");
        if (speedValue.isUndefined())
        {
            return std::unexpected(QString("Missing required 'speed' argument (supported values: %1)")
                                       .arg(PlatformControlProtocol::supportedReplaySpeeds().join(", ")));
        }

        if (speedValue.isDouble())
        {
            return PlatformControlProtocol::replaySpeedFromString(QString::number(speedValue.toInt()));
        }

        return PlatformControlProtocol::replaySpeedFromString(speedValue.toString());
    }

    [[nodiscard]] std::expected<TradingMode, QString> parseRequiredTradingMode(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("mode").toString().trimmed().toLower();
        if (rawValue.isEmpty())
        {
            return std::unexpected("Missing required 'mode' argument (supported values: sim, live)");
        }

        if (rawValue == "sim")
        {
            return TradingMode::Sim;
        }

        if (rawValue == "live")
        {
            return TradingMode::Live;
        }

        return std::unexpected(QString("Unsupported trading mode '%1' (supported values: sim, live)").arg(rawValue));
    }
} // namespace

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
    const QDateTime currentDateTime = getCurrentAppTime();
    const QDate currentDate = currentDateTime.date();
    const Qt::DayOfWeek day = static_cast<Qt::DayOfWeek>(currentDate.dayOfWeek());

    if (day == Qt::Saturday || day == Qt::Sunday)
    {
        return TradingSession::Weekend;
    }

    // Full-day market holiday
    if (MarketCalendar::isHoliday(currentDate))
    {
        return TradingSession::Holiday;
    }

    const QTime currentTime = currentDateTime.time();
    const bool earlyClose = MarketCalendar::isEarlyCloseDay(currentDate);

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

    // On early-close days the regular session ends at 1:00 PM ET
    const QTime regularEnd =
        earlyClose ? MarketCalendar::EARLY_CLOSE_TIME : TradingHours::TIME_LAST_CANDLE_REGULAR_SESSION;
    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_REGULAR_SESSION && currentTime < regularEnd)
    {
        return TradingSession::Regular;
    }

    // After-hours is suppressed entirely on early-close days (market fully closed after 1 PM)
    if (!earlyClose && currentTime >= TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return TradingSession::AfterHours;
    }

    return TradingSession::Closed;
}

MainApp::MainApp() : tradeStationClient(TSClient::getInstance()), mainAlgo(MainAlgo::getInstance())
{
    QThread::currentThread()->setObjectName("GUI/Main Thread");

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

    // High-frequency market data (bars, L2, trades, aggregator bars, replay time)
    // is now pulled by GUIFrontend at 30 Hz from DisplaySnapshot — no cross-thread signals needed.

    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition, appFrontend, &FrontEnd::onNewPositionReceived);

    QObject::connect(mainAlgo, &MainAlgo::positionDeleted, appFrontend, &FrontEnd::onPositionDeleted);

    QObject::connect(mainAlgo, &MainAlgo::receivedNewOrder, appFrontend, &FrontEnd::onNewOrderReceived);

    QObject::connect(mainAlgo, &MainAlgo::balanceUpdated, appFrontend, &FrontEnd::onBalanceUpdated);
}

MainApp::~MainApp()
{
    qInfo() << "MainApp destructor - cleaning up";

    // Stop memory monitoring first to avoid cross-thread timer warnings
    memoryMonitor.stopMonitoring();

    if (m_platformControlServer)
    {
        m_platformControlServer.reset();
    }

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

    // Auto-connect to Databento if API key is present
    auto* dbClient = DBClient::getInstance();
    dbClient->start();

    // Connect Databento data usage updates to frontend
    QObject::connect(dbClient, &DBClient::dataUsageUpdated, appFrontend, &FrontEnd::onDBClientDataUsageUpdate);

    if (dbClient->hasApiKey())
    {
        dbClient->connectLive();
    }

    if (m_platformControlServer == nullptr)
    {
        m_platformControlServer = std::make_unique<PlatformControlServer>(this);
        if (!m_platformControlServer->startListening())
        {
            qCritical() << "Platform control socket failed to start;"
                           " l2trader-ctl and l2trader-mcp-server will be unavailable";
        }
    }

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

    // Delete DBClient singleton
    if (DBClient::isInstantiated())
    {
        DBClient::destroyInstance();
    }

    // Delete DatabaseThread singleton (which will stop its thread)
    DatabaseThread::destroyInstance();

    qInfo() << "All singletons cleaned up";
}

void MainApp::enterReplayMode(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
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

    // 5. Destroy and recreate database singletons to pick up new timestamped replay database path
    OrdersDatabase::destroyInstance();
    PositionsDatabase::destroyInstance();

    // 6. Clean slate: stop everything and recreate fresh, then start replay paused (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol, p_date, p_startTime, p_speed]()
        {
            // Stop all running strategies
            mainAlgo->stopAllStrategies();

            // Close positions/orders streams
            mainAlgo->pauseLiveStreams();

            // Delete all stock instruments (and their streams)
            mainAlgo->deleteAllSymbolContext();

            // Create fresh stock instrument with mock-backed streams
            mainAlgo->createAndSetDisplayedSymbolContext(displayedSymbol);

            // Start replay in paused state - emits first bar to populate chart
            mainAlgo->enterReplayModePaused(displayedSymbol, p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    // 7. Update UI
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
            mainAlgo->deleteAllSymbolContext();
        },
        Qt::BlockingQueuedConnection);

    // 3. Reset data source mode
    m_dataSourceMode = DataSourceMode::Live;

    // 4. Switch TSClient back to live mode (blocking)
    QMetaObject::invokeMethod(
        tradeStationClient,
        [this]() { tradeStationClient->setMode(TSClient::Mode::Live); },
        Qt::BlockingQueuedConnection);

    // 5. Destroy and recreate database singletons to switch back to Live/Sim database
    OrdersDatabase::destroyInstance();
    PositionsDatabase::destroyInstance();

    // 6. Recreate fresh live instruments and resume streams (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol]()
        {
            // Reopen positions/orders streams
            mainAlgo->resumeLiveStreams();

            // Create fresh stock instrument with live streams
            mainAlgo->createAndSetDisplayedSymbolContext(displayedSymbol);
        },
        Qt::QueuedConnection);

    // 6. Update UI
    appFrontend->onReplayModeExited();

    qInfo() << "Replay mode exited, live mode resumed";
}

void MainApp::startReplayPlayback(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "startReplayPlayback called when not in replay mode");

    qInfo() << "Starting replay playback for" << p_date.toString(Qt::ISODate) << "at"
            << p_startTime.toString("hh:mm:ss");

    // Tell MainAlgo to start replay (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, p_date, p_startTime, p_speed]()
        {
            QString symbol = mainAlgo->getDisplayedSymbol();
            mainAlgo->enterReplayMode(symbol, p_date, p_startTime, p_speed);
        },
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

void MainApp::setReplaySpeed(Playback::Speed p_speed)
{
    QMetaObject::invokeMethod(mainAlgo, [this, p_speed]() { mainAlgo->setReplaySpeed(p_speed); }, Qt::QueuedConnection);
}

bool MainApp::isReplayPaused() const
{
    return mainAlgo->getReplayState() == Playback::State::Paused;
}

QJsonObject MainApp::getControlStatus() const
{
    QJsonObject status;
    status["dataSourceMode"] = dataSourceModeToString(m_dataSourceMode);
    status["tradingMode"] = tradingModeToString(m_tradingMode);
    status["tradingModeChangeRequiresRestart"] = true;
    status["displayedSymbol"] = mainAlgo->getDisplayedSymbol();
    status["replayState"] = replayStateToString(mainAlgo->getReplayState());
    status["replayPaused"] = isReplayPaused();
    status["currentAppTime"] = getCurrentAppTime().toString(Qt::ISODateWithMs);
    status["currentReplayTime"] =
        isInReplayMode() ? QJsonValue(currentAppReplayTime.toString(Qt::ISODateWithMs)) : QJsonValue(QJsonValue::Null);
    status["controlSocketPath"] = PlatformControlProtocol::socketPath();
    status["supportedReplaySpeeds"] = PlatformControlProtocol::supportedReplaySpeedsJson();
    return status;
}

QJsonObject MainApp::handleControlRequest(const QJsonObject& p_request)
{
    const int protocolVersion = p_request.value("protocolVersion").toInt(-1);
    if (protocolVersion != PlatformControlProtocol::kProtocolVersion)
    {
        return makeControlResponse(false,
                                   "Platform control request rejected.",
                                   {},
                                   QString("Unsupported control protocol version %1 (expected %2)")
                                       .arg(protocolVersion)
                                       .arg(PlatformControlProtocol::kProtocolVersion));
    }

    const QString command = p_request.value("command").toString().trimmed();
    if (command.isEmpty())
    {
        return makeControlResponse(false, "Platform control request rejected.", {}, "Missing required 'command' field");
    }

    const QJsonObject arguments = p_request.value("arguments").toObject();

    if (command == PlatformControlProtocol::kCommandStatus)
    {
        return makeControlResponse(true, "Platform status retrieved.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandEnterReplay)
    {
        if (isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay mode change rejected.",
                                       {},
                                       "Application is already in replay mode");
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, startTime.error());
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, speed.error());
        }

        enterReplayMode(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay mode entry requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandStartReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay playback request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, startTime.error());
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, speed.error());
        }

        startReplayPlayback(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay playback start requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandPauseReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay pause request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        if (isReplayPaused())
        {
            return makeControlResponse(true, "Replay is already paused.", getControlStatus());
        }

        pauseReplayPlayback();
        return makeControlResponse(true, "Replay pause requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandResumeReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay resume request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        if (!isReplayPaused())
        {
            return makeControlResponse(true, "Replay is already running.", getControlStatus());
        }

        resumeReplayPlayback();
        return makeControlResponse(true, "Replay resume requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandSetReplaySpeed)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay speed update rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay speed update rejected.", {}, speed.error());
        }

        setReplaySpeed(speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay speed update requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandPreloadReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay preload request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, startTime.error());
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, speed.error());
        }

        preloadChartForReplay(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay chart preload requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandExitReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay exit request rejected.",
                                       {},
                                       "Application is not currently in replay mode");
        }

        exitReplayMode();
        return makeControlResponse(true, "Replay mode exit requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandSetTradingMode)
    {
        const auto mode = parseRequiredTradingMode(arguments);
        if (!mode.has_value())
        {
            return makeControlResponse(false, "Trading mode update rejected.", {}, mode.error());
        }

        if (m_tradingMode == mode.value())
        {
            QJsonObject status = getControlStatus();
            status["restartRequired"] = false;
            return makeControlResponse(true, "Trading mode already set.", status);
        }

        setTradingMode(mode.value());
        QJsonObject status = getControlStatus();
        status["restartRequired"] = true;
        status["requestedTradingMode"] = tradingModeToString(mode.value());
        return makeControlResponse(true, "Trading mode updated. Restart required to apply it.", status);
    }

    return makeControlResponse(false,
                               "Platform control request rejected.",
                               {},
                               QString("Unknown control command '%1'").arg(command));
}

void MainApp::preloadChartForReplay(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
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
            mainAlgo->enterReplayModePaused(displayedSymbol, p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    qInfo() << "Chart preload initiated for" << displayedSymbol;
}
