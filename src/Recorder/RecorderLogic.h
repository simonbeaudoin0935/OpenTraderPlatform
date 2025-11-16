#pragma once

#include <QString>
#include <QStringList>

QStringList loadStockTickers(const QString& csvFilePath);
QString createRecordingFolders(const QString& cacheLocation);
void initializeBarsDatabase(const QString& barsPath);