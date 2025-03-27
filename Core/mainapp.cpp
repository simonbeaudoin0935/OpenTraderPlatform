#include "mainapp.h"

MainApp::MainApp(AppFrontend* appFrontend) :
    appFrontend(appFrontend),
    fmpClient(FMPClient::getInstancePtr())
{

}
