#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestBarCache.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QThread::currentThread()->setObjectName("MainThread");

    TestBarCache test;
    return QTest::qExec(&test, argc, argv);
} 
