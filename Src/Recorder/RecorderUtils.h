#pragma once

#include <QString>
#include <QStringList>
#include "Stream.h"

QStringList loadStockTickers(const QString& csvFilePath);
QString createRecordingFolders(const QString& cacheLocation);
QString streamErrorToString(Stream::StreamError error);
