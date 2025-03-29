#include <QtTest>
#include "test_tradestationclient.h"
#include <QCommandLineParser>
#include <QString>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    TestTradeStationClient test;
    return QTest::qExec(&test, argc, argv);
} 