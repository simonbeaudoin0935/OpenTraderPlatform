#include "Settings.h"

#include <QStandardPaths>
#include <QCoreApplication>

QSettings *criteriaSettings;

QString cacheRootDir;
QString stockCsvFile;

QString getCacheLocation() {
    if (!cacheRootDir.isEmpty()) {
        return cacheRootDir + "/" + QCoreApplication::applicationName();
    } else {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
}
