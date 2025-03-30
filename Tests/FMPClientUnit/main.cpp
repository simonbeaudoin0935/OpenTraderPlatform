#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "../FMPClientUnit/TestFMPClient.h"

// Global variable to store the token file path
QString fmpKey;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    QCommandLineParser parser;
    parser.setApplicationDescription("FMPClient Unit Tests");
    parser.addHelpOption();

    QCommandLineOption tokenOption("tokens", "Path to the token file", "file", "/home/simon/Desktop/access_tokens.ini");
    parser.addOption(tokenOption);
    parser.process(app);

    QString tokenFile = parser.value(tokenOption);
    QSettings settings(tokenFile, QSettings::IniFormat);

    fmpKey = settings.value("FMP/AccessToken", "DEFAULT_KEY_IF_NOT_FOUND").toString();

    TestFMPClient test;
    return QTest::qExec(&test, argc, argv);
}
