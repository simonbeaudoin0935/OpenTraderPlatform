#include "RecorderUtils.h"
#include "Settings.h"
#include "Stream.h"

#include <QFile>
#include <QTextStream>
#include <QDir>

QStringList loadStockTickers(const QString& csvFilePath)
{
    QStringList stockTickers;
    QFile file(csvFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qFatal("Cannot open stock CSV file: %s", qUtf8Printable(csvFilePath));
    }
    QTextStream in(&file);
    QString header = in.readLine(); // Skip header line
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList fields = line.split(',');
        if (!fields.isEmpty() && !fields[0].isEmpty())
        {
            QString ticker = fields[0].trimmed();
            // Remove surrounding quotes (both single and double)
            if ((ticker.startsWith('"') && ticker.endsWith('"')) || (ticker.startsWith('\'') && ticker.endsWith('\'')))
            {
                ticker = ticker.mid(1, ticker.length() - 2);
            }
            stockTickers.append(ticker);
        }
    }
    file.close();
    return stockTickers;
}

QString createRecordingFolders(const QString& cacheLocation)
{
    // Determine the base path for recorded data
    QString basePath;
    if (!recordedDataDir.isEmpty())
    {
        // Use the user-specified recorded data directory
        basePath = recordedDataDir;
        qDebug() << "Using custom recorded data directory:" << basePath;
    }
    else
    {
        // Use the default cache location
        basePath = cacheLocation;
        qDebug() << "Using default cache location:" << basePath;
    }

    QDir baseDir(basePath);
    if (!baseDir.exists())
    {
        if (!baseDir.mkpath("."))
        {
            qFatal("Cannot create base directory: %s", qUtf8Printable(basePath));
        }
    }

    QString recordedDataPath = basePath + "/RecordedLiveData";
    QDir recordedDir(recordedDataPath);
    if (!recordedDir.exists())
    {
        if (!recordedDir.mkpath("."))
        {
            qFatal("Cannot create RecordedLiveData directory: %s", qUtf8Printable(recordedDataPath));
        }
    }
    QString barsPath = recordedDataPath + "/Bars";
    QDir barsDir(barsPath);
    if (!barsDir.exists())
    {
        if (!barsDir.mkpath("."))
        {
            qFatal("Cannot create Bars directory: %s", qUtf8Printable(barsPath));
        }
    }
    QString marketDepthPath = recordedDataPath + "/MarketDepthQuotes";
    QDir mdDir(marketDepthPath);
    if (!mdDir.exists())
    {
        if (!mdDir.mkpath("."))
        {
            qFatal("Cannot create MarketDepthQuotes directory: %s", qUtf8Printable(marketDepthPath));
        }
    }
    return recordedDataPath;
}