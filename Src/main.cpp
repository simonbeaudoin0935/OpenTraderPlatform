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
#include <QDir>

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

    // Initialize app state settings for persistent UI state.
    // Stored in XDG_STATE_HOME (~/.local/state/L2Trader/AppState.ini) because
    // it holds previous session state, not user configuration.
    QString appStateFilePath = getStateLocation() + "/AppState.ini";
    QDir().mkpath(getStateLocation());
    appStateSettings = new QSettings(appStateFilePath, QSettings::IniFormat);
    appStateSettings->setFallbacksEnabled(false);

    // Initialize strategies state settings for persisting loaded strategies across sessions.
    // Stored alongside AppState.ini in XDG_STATE_HOME (~/.local/state/L2Trader/).
    QString strategiesStateFilePath = getStateLocation() + "/StrategiesState.ini";
    strategiesStateSettings = new QSettings(strategiesStateFilePath, QSettings::IniFormat);
    strategiesStateSettings->setFallbacksEnabled(false);

    qInfo() << "Cache root directory:" << getCacheLocation();
    qInfo() << "Data root directory:" << getDataLocation();
    qInfo() << "State root directory:" << getStateLocation();

    MainApp* mainApp = MainApp::getInstance();

    mainApp->start();

    int exitCode = QCoreApplication::exec();

    // Cleanup all singletons for proper resource deallocation
    // This is important for valgrind memory leak detection
    qInfo() << "Application event loop exited, cleaning up singletons";
    MainApp::cleanupSingletons();

    // Drain and shut down the async log buffers after all singletons are gone
    // so the final log lines from cleanupSingletons() are not lost.
    shutdownLogging();

    return exitCode;
}
