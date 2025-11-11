#ifdef GUI_ENABLED
#include <QApplication>
#include <QIcon>
#define APPLICATION QApplication
#else
#include <QCoreApplication>
#define APPLICATION QCoreApplication
#endif

#include "Misc/ArgumentParser.h"
#include "Misc/Logging.h"
#include "Settings.h"
#include "Core/MainApp.h"

#include <QtGlobal>
#include <QDateTime>

#include <iostream>

int main(int argc, char *argv[])
{
    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    qInfo() << "Version:" << version;

#ifdef GUI_ENABLED
    app.setWindowIcon(QIcon(":/Icons/L2T.png"));
#endif

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    MainApp mainApp;

    mainApp.start();

    return app.exec();
}
