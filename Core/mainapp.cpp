#include "mainapp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr()),
    tradeStationClient(TradeStationClient::getInstancePtr()),
    mainAlgo(new MainAlgo())
{
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &AppFrontend::onMemoryUsageUpdate);
    memoryMonitor.startMonitoring(500);

    // Connect TradeStation authentication state changes to frontend
    QObject::connect(tradeStationClient, &TradeStationClient::authenticationStateChanged,
                    appFrontend, &AppFrontend::tradeStationAuthStateChanged);

    // Connect TradeStation authentication errors to frontend
    QObject::connect(tradeStationClient, &TradeStationClient::authenticationError,
                    appFrontend, &AppFrontend::tradeStationAuthError);

    // Check TradeStation authentication state after a short delay
    QTimer::singleShot(0, [this]() {
        bool isAuthenticated = AuthWindow::isAlreadyAuthenticated();
        emit this->appFrontend->tradeStationAuthStateChanged(isAuthenticated);
    });
}
