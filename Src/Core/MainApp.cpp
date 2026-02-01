#include "MainApp.h"
#include "DatabaseThread.h"
#include "Logging.h"
#include "Stream.h"
#include <QCoreApplication>
#ifdef GUI_ENABLED
#include "GUIFrontend.h"
#else
#include "TUIFrontend.h"
#endif

bool MainApp::isInReplayMode = false;

QDateTime MainApp::currentAppReplayTime = QDateTime::fromSecsSinceEpoch(0);

// Initialize static member
MainApp* MainApp::m_instance = nullptr;

MainApp* MainApp::getInstance()
{
    if (m_instance == nullptr)
    {
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

// Get the current application time (real or replay)
QDateTime MainApp::getCurrentAppTime()
{
    if (isInReplayMode)
    {
        // Not implemented yet
        Q_UNREACHABLE();
        return currentAppReplayTime;
    }
    return QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE);
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
}

MainApp::~MainApp()
{
    qInfo() << "MainApp destructor - cleaning up";

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
