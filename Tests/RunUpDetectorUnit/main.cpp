#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestRunUpDetector.h"
#include "ArgumentParser.h"
#include "Settings.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    QThread::currentThread()->setObjectName("MainThread");

    TestRunUpDetector test;
    return QTest::qExec(&test, argc, argv);
} 
