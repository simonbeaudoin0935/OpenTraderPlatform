#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestBarCache.h"
#include "ArgumentParser.h"
#include "Settings.h"
#include "Logging.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("BarCacheUnit");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    parseArguments(app.arguments());

    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

    qInfo() << "Cache root directory:" << getCacheLocation();

    QThread::currentThread()->setObjectName("MainThread");

    TestBarCache test;
    return QTest::qExec(&test, argc=0, argv); //FIXME need to put 0 here to avoid passing our arguments
} 
