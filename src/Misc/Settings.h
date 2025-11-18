#pragma once

#include <QSettings>
#include <QString>

extern QSettings *criteriaSettings;

extern QString cacheRootDir;
extern QString stockCsvFile;
extern QString recordedDataDir;

QString getCacheLocation();
