#include "Settings.h"

#include <QStandardPaths>

QSettings *criteriaSettings;

QString cacheRootDir;

QString getCacheLocation() {
    if (!cacheRootDir.isEmpty()) {
        return cacheRootDir + "/L2Trader";
    } else {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
}
