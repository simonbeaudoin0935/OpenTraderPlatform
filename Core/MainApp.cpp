#include "MainApp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TradeStationClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate);

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    QObject::connect(tradeStationClient, &TradeStationClient::authStateChanged,
                    appFrontend, &AppFrontend::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient, &TradeStationClient::accountsReceived,
                     appFrontend, &AppFrontend::tradeStationAccountsReceived);


    // Connect FMP data usage updates to frontend
    QObject::connect(fmpClient, &FMPClient::totalDataReceivedBytesIncreased,
                    appFrontend, &AppFrontend::fmpDataUsageUpdated);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient, &TradeStationClient::totalDataReceivedBytesIncreased,
                     appFrontend, &AppFrontend::tradeStationDataUsageUpdated);

}

void MainApp::start()
{
    // start the threads
    fmpClient->start();
    tradeStationClient->start();
    mainAlgo->start();

    memoryMonitor.startMonitoring(500);
}
