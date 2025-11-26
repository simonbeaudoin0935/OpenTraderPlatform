#include "MainApp.h"
#ifdef GUI_ENABLED
#include "GUIFrontend.h"
#else
#include "TUIFrontend.h"
#endif

MainApp::MainApp() :
    tradeStationClient(TSClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
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
    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     appFrontend, &FrontEnd::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     mainAlgo, &MainAlgo::onTradeStationAuthStateChanged);

    QObject::connect(mainAlgo, &MainAlgo::tradeStationAccountsReceived,
                     appFrontend, &FrontEnd::tradeStationAccountsReceived);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient, &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend, &FrontEnd::tradeStationDataUsageUpdated);

    // Connect TradeStation stream count updates to frontend
    QObject::connect(tradeStationClient, &TSClient::streamCountChanged,
                     appFrontend, &FrontEnd::streamCountUpdated);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewBar,
                     appFrontend, &FrontEnd::currentHighlightedStockBarReceived);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
                     appFrontend, &FrontEnd::currentHighlightedReceivedNewMarketDepthQuote);


    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition,
                     appFrontend, &FrontEnd::newPositionReceived);

    QObject::connect(appFrontend, &FrontEnd::requestMissingBars,
                     mainAlgo,    &MainAlgo::onRequestMissingBarsDisplayedStock);

    QObject::connect(mainAlgo,    &MainAlgo::requestedMissingBarsDisplayedStockReceived,
                     appFrontend, &FrontEnd::onRequestedMissingBarsDisplayedStockReceived);
}

void MainApp::start()
{
    // start the threads
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
