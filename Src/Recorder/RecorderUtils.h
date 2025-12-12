#pragma once

#include <QString>
#include <QStringList>

QStringList loadStockTickers(const QString& csvFilePath);
QString createRecordingFolders(const QString& cacheLocation);
QString streamErrorToString(Stream::StreamError error);

#endif // RECORDER_UTILS_H