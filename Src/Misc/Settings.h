#pragma once

#include <QSettings>
#include <QString>

#include "CONSTANTS.h"

extern QSettings* appStateSettings;

extern QString cacheRootDir;
extern QString stockCsvFile;
extern QString recordedDataDir;

QString getCacheLocation();
QString getDataLocation();
QString getStateLocation();
