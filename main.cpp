#ifdef GUI_ENABLED
#include <QApplication>
#include <QIcon>
#include "GUI/GuiFrontend.h"
#define FRONTEND GuiFrontend
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#include "Core/TerminalFrontend.h"
#define FRONTEND TerminalFrontend
#define APPLICATION QCoreApplication
#endif

#include "Misc/ArgumentParser.h"
#include "Misc/Settings.h"
#include "Core/MainApp.h"
#include "Clients/FMPClient/FMPClient.h"

int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QCoreApplication::setApplicationVersion("1.0");

    parseArguments(app.arguments());

    QString fmpToken = tokensSettings->value("FMP/AccessToken").toString();
    if (fmpToken.isEmpty()) {
        qFatal() << "No FMP access token found in config.ini. Exiting...";
    }

#warning Fix this shit
    FMPClient::getInstance().setAPIKey(fmpToken);

    // Set application icon
    QIcon appIcon(":/Icons/L2T.png");
    app.setWindowIcon(appIcon);

    MainApp mainApp(new FRONTEND());

    mainApp.start();

    return app.exec();
}
