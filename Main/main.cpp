#include "../FMPClient/fmpclient.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("TradeStation Algo");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Trading Algorithm Application");
    parser.addHelpOption();

    QCommandLineOption tokenOption("tokens", "Path to the token file", "file", "../access_tokens.ini");
    parser.addOption(tokenOption);
    parser.process(app);

    QString tokenFile = parser.value(tokenOption);
    QSettings settings(tokenFile, QSettings::IniFormat);
    QString fmpKey = settings.value("FMP/AccessToken", "DEFAULT_KEY_IF_NOT_FOUND").toString();

    FMPClient::setAPIKey(fmpKey);

    FMPClient& client = FMPClient::getInstance();

    QObject::connect(&client, &FMPClient::quoteReceived, [](double price, double bid, double ask) {
        qDebug() << "Async Quote - Price:" << price << "Bid:" << bid << "Ask:" << ask;
    });
    client.fetchQuoteAsync("AAPL");

    double price, bid, ask;
    if (client.fetchQuoteSync("MSFT", price, bid, ask)) {
        qDebug() << "Sync Quote - Price:" << price << "Bid:" << bid << "Ask:" << ask;
    } else {
        qDebug() << "Sync Quote failed or timed out";
    }

    QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    return app.exec();
}
