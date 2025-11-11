#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestFMPClient.h"
#include "ArgumentParser.h"
#include "Settings.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    QThread::currentThread()->setObjectName("MainThread");

    QCommandLineParser parser;
    parser.setApplicationDescription("FMPClient Unit Tests");
    parser.addHelpOption();

    TestFMPClient test;
    return QTest::qExec(&test, argc, argv);
}
