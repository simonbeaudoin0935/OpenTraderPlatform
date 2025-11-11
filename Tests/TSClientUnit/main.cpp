#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestTSClient.h"
#include "ArgumentParser.h"
#include "Settings.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    QThread::currentThread()->setObjectName("MainThread");

    TestTSClient test;
    return QTest::qExec(&test, argc, argv);
} 
