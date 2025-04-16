#include "MainApp.h"
#ifdef GUI_ENABLED
#include "GUI/GuiFrontend.h"
#else
#include "TerminalFrontend.h"
#endif

MainApp::MainApp() :
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TSClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
#ifdef GUI_ENABLED
    appFrontend = new GuiFrontend(mainAlgo);
#endif
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate);

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     appFrontend, &AppFrontend::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient, &TSClient::authStateChanged,
                     mainAlgo, &MainAlgo::onTradeStationAuthStateChanged);

    QObject::connect(tradeStationClient, &TSClient::accountsAsyncReceived,
                     appFrontend, &AppFrontend::tradeStationAccountsReceived);

    // Connect FMP data usage updates to frontend
    QObject::connect(fmpClient, &FMPClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::fmpDataUsageUpdated);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient, &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::tradeStationDataUsageUpdated);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewBar,
                     appFrontend, &AppFrontend::currentHighlightedStockBarReceived);

    QObject::connect(mainAlgo, &MainAlgo::displayedStockReceivedNewMarketDepthQuote,
                     appFrontend, &AppFrontend::currentHighlightedReceivedNewMarketDepthQuote);


    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition,
                     appFrontend, &AppFrontend::newPositionReceived);

}

void MainApp::start()
{
    // start the threads
    fmpClient->start();
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
