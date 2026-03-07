#include "Settings.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QDir>

QSettings* appStateSettings;

QString cacheRootDir;
QString stockCsvFile;
QString recordedDataDir;

QString getCacheLocation()
{
    if (!cacheRootDir.isEmpty())
    {
        return cacheRootDir + "/" + QCoreApplication::applicationName();
    }
    else
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
}

QString getDataLocation()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString getStateLocation()
{
    QString xdgStateHome = QProcessEnvironment::systemEnvironment().value("XDG_STATE_HOME");
    if (xdgStateHome.isEmpty())
    {
        xdgStateHome = QDir::homePath() + "/.local/state";
    }
    return xdgStateHome + "/" + QCoreApplication::applicationName();
}
