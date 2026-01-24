#pragma once

#include <QSettings>
#include <QString>

extern QSettings* criteriaSettings;
extern QSettings* appStateSettings;

extern QString cacheRootDir;
extern QString stockCsvFile;
extern QString recordedDataDir;

QString getCacheLocation();

// Subdirectory name for bar cache database files
inline constexpr const char* BARS_CACHE_SUBDIR = "Bars";
