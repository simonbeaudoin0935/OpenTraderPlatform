#include "MainApp.h"
#include "DatabaseThread.h"
#ifdef GUI_ENABLED
#include "GUIFrontend.h"
#else
#include "TUIFrontend.h"
#endif

bool MainApp::isInReplayMode = false;

QDateTime MainApp::currentAppReplayTime = QDateTime::fromSecsSinceEpoch(0);

// Get the current application time (real or replay)
QDateTime MainApp::getCurrentAppTime()
{
    if (isInReplayMode) {
        // Not implemented yet
        Q_UNREACHABLE();
        return currentAppReplayTime;
    } else {
        return QDateTime::currentDateTime().toTimeZone(QTimeZone("America/New_York"));
    }
}

MainApp::MainApp() :
    tradeStationClient(TSClient::getInstance()),
    mainAlgo(MainAlgo::getInstance())
{
#ifdef GUI_ENABLED
    m_appFrontend = std::make_unique<GUIFrontend>(mainAlgo);
#else
    m_appFrontend = std::make_unique<TUIFrontend>(mainAlgo);
#endif
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, m_appFrontend.get(), &FrontEnd::onMemoryUsageUpdate);

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     m_appFrontend.get(), &FrontEnd::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     mainAlgo, &MainAlgo::onTradeStationAuthStateChanged);

    QObject::connect(mainAlgo, &MainAlgo::tradeStationAccountsReceived,
                     m_appFrontend.get(), &FrontEnd::tradeStationAccountsReceived);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient, &TSClient::totalDataReceivedBytesIncreased,
                     m_appFrontend.get(), &FrontEnd::tradeStationDataUsageUpdated);

    // Connect TradeStation stream count updates to frontend
    QObject::connect(tradeStationClient, &TSClient::openStreamCountChanged,
                     m_appFrontend.get(), &FrontEnd::streamCountUpdated);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewBar,
                     m_appFrontend.get(), &FrontEnd::currentHighlightedStockBarReceived);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
                     m_appFrontend.get(), &FrontEnd::currentHighlightedReceivedNewMarketDepthQuote);


    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition,
                     m_appFrontend.get(), &FrontEnd::newPositionReceived);

    QObject::connect(mainAlgo, &MainAlgo::positionDeleted,
                     m_appFrontend.get(), &FrontEnd::positionDeleted);

    QObject::connect(mainAlgo, &MainAlgo::receivedNewOrder,
                     m_appFrontend.get(), &FrontEnd::newOrderReceived);

    QObject::connect(mainAlgo, &MainAlgo::balanceUpdated,
                     m_appFrontend.get(), &FrontEnd::balanceUpdated);
}

void MainApp::start()
{
    // Start the database thread first (other threads may depend on it)
    DatabaseThread::getInstance()->start();

    // start the other threads
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
