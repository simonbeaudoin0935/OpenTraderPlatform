#ifndef MAINAPP_H
#define MAINAPP_H

#include "../Clients/FMPClient/fmpclient.h"
#include "appfrontend.h"
#include "../Algo/mainalgo.h"
#include "memorymonitor.h"

class MainApp
{
public:
    MainApp(AppFrontend* appFrontend);

private:
    AppFrontend* appFrontend;
    FMPClient*   fmpClient;
    MainAlgo*    mainAlgo;
    MemoryMonitor memoryMonitor;
};

#endif // MAINAPP_H
