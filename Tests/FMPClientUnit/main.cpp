#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestFMPClient.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    QCommandLineParser parser;
    parser.setApplicationDescription("FMPClient Unit Tests");
    parser.addHelpOption();

    TestFMPClient test;
    return QTest::qExec(&test, argc, argv);
}
