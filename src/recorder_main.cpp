#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDateTime>

#include "Misc/ArgumentParser.h"
#include "Misc/Logging.h"
#include "Misc/Settings.h"

#include <QtGlobal>

#include <iostream>

int main(int argc, char *argv[])
{
    // Initialize logging (opens file and installs handler)
    initLogging();

    qInfo() << "Qt version:" << QT_VERSION_STR;

    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("Recorder");
    QString version = QString("%1 ~ %2@%3").arg(GIT_TAG, GIT_BRANCH, GIT_HASH);
    QCoreApplication::setApplicationVersion(version);

    qInfo() << "Version:" << version;

    parseArguments(app.arguments());

    qInfo() << "Cache root directory:" << getCacheLocation();

    // TODO: Implement recording logic here
    qInfo() << "Recorder started - recording market data...";

    return app.exec();
}