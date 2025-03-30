#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestTradeStationClient.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    TestTradeStationClient test;
    return QTest::qExec(&test, argc, argv);
} 
