#include "mainapp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TradeStationClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate);
    memoryMonitor.startMonitoring(500);

    // Connect TradeStation authentication state changes to frontend
    QObject::connect(tradeStationClient, &TradeStationClient::authenticationStateChanged,
                    appFrontend, &AppFrontend::tradeStationAuthStateChanged);

    // Connect TradeStation authentication errors to frontend
    QObject::connect(tradeStationClient, &TradeStationClient::authenticationError,
                    appFrontend, &AppFrontend::tradeStationAuthError);

    // Connect FMP data usage updates to frontend
    QObject::connect(fmpClient, &FMPClient::totalDataReceivedBytesIncreased,
                    appFrontend, &AppFrontend::fmpDataUsageUpdated);

    // Check TradeStation authentication state once at startup after the event loop starts
    // This allows the frontend to be updated with the initial authentication state
    QTimer::singleShot(0, [this]() {
        bool isAuthenticated = AuthWindow::isAlreadyAuthenticated();
        emit this->appFrontend->tradeStationAuthStateChanged(isAuthenticated);
    });
}
