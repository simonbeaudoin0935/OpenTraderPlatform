#pragma once

#include <QSettings>
#include <QString>

extern QSettings *criteriaSettings;
extern QSettings *appStateSettings;

extern QString cacheRootDir;
extern QString stockCsvFile;
extern QString recordedDataDir;

QString getCacheLocation();
