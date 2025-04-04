#include "MainApp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TSClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
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

    QObject::connect(tradeStationClient, &TSClient::marketDepthNotAvailable,
                     appFrontend, &AppFrontend::marketDepthNotAvailable);

    // Connect FMP data usage updates to frontend
    QObject::connect(fmpClient, &FMPClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::fmpDataUsageUpdated);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient, &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::tradeStationDataUsageUpdated);

    QObject::connect(mainAlgo, &MainAlgo::currentHighlightedReceivedNewBar,
                     appFrontend, &AppFrontend::currentHighlightedStockBarReceived);

    QObject::connect(mainAlgo, &MainAlgo::currentHighlightedReceivedNewMarketDepthQuote,
                     appFrontend, &AppFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote);

    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition,
                     appFrontend, &AppFrontend::onNewPositionReceived);

}

void MainApp::start()
{
    // start the threads
    fmpClient->start();
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
