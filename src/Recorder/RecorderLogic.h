#pragma once

#include <QString>
#include <QStringList>
#include "LiveBarsDB.h"

QStringList loadStockTickers(const QString& csvFilePath);
QString createRecordingFolders(const QString& cacheLocation);
LiveBarsDB* initializeBarsDatabase(const QString& barsPath);