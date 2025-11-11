#include <QtTest>
#include <QCommandLineParser>
#include <QString>

#include "TestFMPClient.h"
#include "ArgumentParser.h"
#include "Settings.h"
#include "Logging.h"


int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("FMPClientUnit");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    QThread::currentThread()->setObjectName("MainThread");

    QCommandLineParser parser;
    parser.setApplicationDescription("FMPClient Unit Tests");
    parser.addHelpOption();

    TestFMPClient test;
    return QTest::qExec(&test, argc, argv);
}
