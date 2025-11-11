#pragma once

#include <QSettings>
#include <QString>

extern QSettings *criteriaSettings;

extern QString cacheRootDir;

QString getCacheLocation();
