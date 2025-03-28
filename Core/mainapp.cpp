#include "mainapp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TradeStationClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate);
    memoryMonitor.startMonitoring(500);
}
