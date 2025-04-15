#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestRunUpDetector.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    TestRunUpDetector test;
    return QTest::qExec(&test, argc, argv);
} 
