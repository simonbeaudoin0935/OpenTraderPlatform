#pragma once

#include <QString>
#include <QStringList>
#include "LiveBarsDB.h"
#include "LiveMarketDepthQuoteDB.h"

QStringList loadStockTickers(const QString& csvFilePath);
QString createRecordingFolders(const QString& cacheLocation);
LiveBarsDB* initializeBarsDatabase(const QString& barsPath);
LiveMarketDepthQuoteDB* initializeMarketDepthQuoteDatabase(const QString& marketDepthQuotesPath, QStringList& stockTickers);