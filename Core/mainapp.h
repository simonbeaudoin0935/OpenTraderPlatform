#ifndef MAINAPP_H
#define MAINAPP_H

#include "../FMPClient/fmpclient.h"
#include "appfrontend.h"

class MainApp
{
public:
    MainApp(AppFrontend* appFrontend);

private:
    AppFrontend* appFrontend;
    FMPClient*   fmpClient;
};

#endif // MAINAPP_H
