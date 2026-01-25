#pragma once

#include <QSettings>
#include <QString>

#include "CONSTANTS.h"

extern QSettings* criteriaSettings;
extern QSettings* appStateSettings;

extern QString cacheRootDir;
extern QString stockCsvFile;
extern QString recordedDataDir;

QString getCacheLocation();
