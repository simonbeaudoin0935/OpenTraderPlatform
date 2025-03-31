#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestTSClient.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    TestTSClient test;
    return QTest::qExec(&test, argc, argv);
} 
