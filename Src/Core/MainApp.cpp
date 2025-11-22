#include "MainApp.h"
#ifdef GUI_ENABLED
#include "GUI/GUIFrontend.h"
#else
#include "TUI/TUIFrontend.h"
#endif

MainApp::MainApp() :
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TSClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
#ifdef GUI_ENABLED
    appFrontend = new GUIFrontend(mainAlgo);
#else
    appFrontend = new TUIFrontend(mainAlgo);
#endif
    // Connect memory usage updates to frontend
    bool connection1 = QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate, Qt::UniqueConnection);
    Q_ASSERT_X(connection1, "MainApp", "Failed to create unique connection for memoryUsageUpdated");

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    bool connection2 = QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     appFrontend, &AppFrontend::tradeStationAuthStateChanged, Qt::UniqueConnection);
    Q_ASSERT_X(connection2, "MainApp", "Failed to create unique connection for authStateChanged to frontend");

    bool connection3 = QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     mainAlgo, &MainAlgo::onTradeStationAuthStateChanged, Qt::UniqueConnection);
    Q_ASSERT_X(connection3, "MainApp", "Failed to create unique connection for authStateChanged to mainAlgo");

    bool connection4 = QObject::connect(tradeStationClient, &TSClient::getAccountsAsyncReceived,
                     appFrontend, &AppFrontend::tradeStationAccountsReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection4, "MainApp", "Failed to create unique connection for getAccountsAsyncReceived");

    // Connect FMP data usage updates to frontend
    bool connection5 = QObject::connect(fmpClient, &FMPClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::fmpDataUsageUpdated, Qt::UniqueConnection);
    Q_ASSERT_X(connection5, "MainApp", "Failed to create unique connection for totalDataReceivedBytesIncreased from FMP");

    // Connect TradeStation data usage updates to frontend
    bool connection6 = QObject::connect(tradeStationClient, &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::tradeStationDataUsageUpdated, Qt::UniqueConnection);
    Q_ASSERT_X(connection6, "MainApp", "Failed to create unique connection for totalDataReceivedBytesIncreased from TS");

    // Connect TradeStation stream count updates to frontend
    bool connection7 = QObject::connect(tradeStationClient, &TSClient::streamCountChanged,
                     appFrontend, &AppFrontend::streamCountUpdated, Qt::UniqueConnection);
    Q_ASSERT_X(connection7, "MainApp", "Failed to create unique connection for streamCountChanged");

    bool connection8 = QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewBar,
                     appFrontend, &AppFrontend::currentHighlightedStockBarReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection8, "MainApp", "Failed to create unique connection for displayedStockReceivedNewBar");

    bool connection9 = QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
                     appFrontend, &AppFrontend::currentHighlightedReceivedNewMarketDepthQuote, Qt::UniqueConnection);
    Q_ASSERT_X(connection9, "MainApp", "Failed to create unique connection for displayedStockReceivedNewMarketDepthQuote");


    bool connection10 = QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition,
                     appFrontend, &AppFrontend::newPositionReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection10, "MainApp", "Failed to create unique connection for receivedNewPosition");

    bool connection11 = QObject::connect(appFrontend, &AppFrontend::requestMissingBars,
                     mainAlgo,    &MainAlgo::onRequestMissingBarsDisplayedStock, Qt::UniqueConnection);
    Q_ASSERT_X(connection11, "MainApp", "Failed to create unique connection for requestMissingBars");

    bool connection12 = QObject::connect(mainAlgo,    &MainAlgo::requestedMissingBarsDisplayedStockReceived,
                     appFrontend, &AppFrontend::onRequestedMissingBarsDisplayedStockReceived, Qt::UniqueConnection);
    Q_ASSERT_X(connection12, "MainApp", "Failed to create unique connection for requestedMissingBarsDisplayedStockReceived");
}

void MainApp::start()
{
    // start the threads
    fmpClient->start();
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
