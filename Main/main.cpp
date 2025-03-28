#ifdef GUI_ENABLED
#include <QApplication>
#include "../GUI/guifrontend.h"
#define FRONTEND GuiFrontend
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#include "../Core/terminalfrontend.h"
#define FRONTEND TerminalFrontend
#define APPLICATION QCoreApplication
#endif

#include "argumentparser.h"
#include "settings.h"
#include "../Core/mainapp.h"
#include "../Clients/FMPClient/fmpclient.h"
#include "../Clients/TradeStationClient/tradestationclient.h"

int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("TradeStation Algo");
    QCoreApplication::setApplicationVersion("1.0");

    parseArguments(app.arguments());

    QString fmpToken = tokensSettings->value("FMP/AccessToken").toString();
    if (fmpToken.isEmpty()) {
        qFatal() << "No FMP access token found in config.ini. Exiting...";
    }

    FMPClient::getInstance().setAPIKey(fmpToken);

    QString tradeStationToken = tokensSettings->value("TradeStation/AccessToken").toString();
    if (tradeStationToken.isEmpty()) {
        qFatal() << "No TradeStation access token found in config.ini. Exiting...";
    }

    TradeStationClient::getInstance().setAPIKey(tradeStationToken);

    MainApp mainApp(new FRONTEND());

    return app.exec();
}
