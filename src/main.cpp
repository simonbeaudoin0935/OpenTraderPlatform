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
#include "Misc/Settings.h"
#include "Core/MainApp.h"

#include <QtGlobal>
#include <QDateTime>

#include <iostream>

int main(int argc, char *argv[])
{
    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    APPLICATION app(argc, argv);

    QCoreApplication::setApplicationName("L2Trader");
    QCoreApplication::setApplicationVersion("1.0");
#ifdef GUI_ENABLED
    app.setWindowIcon(QIcon(":/Icons/L2T.png"));
#endif

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    MainApp mainApp;

    mainApp.start();

    return app.exec();
}
