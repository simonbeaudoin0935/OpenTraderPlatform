#include "Settings.h"

#include <QStandardPaths>
#include <QCoreApplication>

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
