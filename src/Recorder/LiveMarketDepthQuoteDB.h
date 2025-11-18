#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>

#include "StreamMarketDepthQuote.h"

class LiveMarketDepthQuoteDB : public QObject {
    Q_OBJECT
    
public:
    LiveMarketDepthQuoteDB(const QString& dbPath, QStringList& stockTickers);
    ~LiveMarketDepthQuoteDB();

    bool isOpen() const;

    void startRecording();
    
    QMap<QString, QMap<Stream::StreamError, int>> getErrorCounters() const { return streamErrorCounters; }
    QMap<QString, int> getRecoveredTimeouts() const { return recoveredTimeouts; }
    QMap<QString, int> getUnrecoveredTimeoutCounts() const { return unrecoveredTimeoutCounts; }
    
    void finalizeUnrecoveredTimeouts();
    
private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);
    void onStreamErrorOccurred(Stream::StreamError error, QString errorMessage);

private:
    bool storeMarketDepthQuoteRawData(const QString& stock, qint64 epochMs, const QByteArray& rawData);
    
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;
    QMap<QString, StreamMarketDepthQuote*> streamMarketDepthQuotes;
    QMap<QString, QMap<Stream::StreamError, int>> streamErrorCounters;
    QSet<QString> unrecoveredTimeouts;
    QMap<QString, int> recoveredTimeouts;
    QMap<QString, int> unrecoveredTimeoutCounts;
};