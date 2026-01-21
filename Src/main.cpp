#ifdef GUI_ENABLED
#include <QApplication>
#include <QIcon>
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#define APPLICATION QCoreApplication
#endif

#include "ArgumentParser.h"
#include "Logging.h"
#include "Settings.h"
#include "Core/MainApp.h"

#include <QtGlobal>
#include <QDateTime>

#include <iostream>

int main(int argc, char* argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    // Initialize logging (opens file and installs handler)
    initLogging();

    // Write default config to disk on first run
    LoggingConfig::instance().writeConfigToDisk();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

#ifdef GUI_ENABLED
    QApplication::setWindowIcon(QIcon(":/Icons/L2T.png"));
#endif

    parseArguments(QCoreApplication::arguments());

    // Initialize app state settings for persistent UI state
    appStateSettings =
        new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName(), "AppState");
    appStateSettings->setFallbacksEnabled(false);

    qInfo() << "Cache root directory:" << getCacheLocation();

    MainApp* mainApp = MainApp::getInstance();

    mainApp->start();

    int exitCode = QCoreApplication::exec();

    // Cleanup all singletons for proper resource deallocation
    // This is important for valgrind memory leak detection
    qInfo() << "Application event loop exited, cleaning up singletons";
    MainApp::cleanupSingletons();

    return exitCode;
}
