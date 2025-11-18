#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>
#include <QObject>

#include "StreamBars.h"
#include "StreamMarketDepthQuote.h"

class LiveStreamDB : public QObject {
    Q_OBJECT

public:
    enum class StreamType {
        Bars,
        MarketDepthQuotes
    };

    LiveStreamDB(StreamType type, const QString& dbPath, QStringList& stockTickers);
    ~LiveStreamDB();

    bool isOpen() const;
    void startRecording();

    QMap<QString, QMap<Stream::StreamError, int>> getErrorCounters() const { return streamErrorCounters; }
    QMap<QString, int> getRecoveredTimeouts() const { return recoveredTimeouts; }
    QMap<QString, int> getUnrecoveredTimeoutCounts() const { return unrecoveredTimeoutCounts; }

    int getRecordCount() const;
    int getActiveStreamCount() const;
    int getTotalConfiguredStreams() const { return stockTickers.size(); }

    void finalizeUnrecoveredTimeouts();

private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);
    void onStreamErrorOccurred(Stream::StreamError error, QString errorMessage);

private:
    bool storeData(const QString& stock, qint64 epochMs, const QByteArray& rawData);

    StreamType streamType;
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;

    // Union-like storage for different stream types
    QMap<QString, StreamBars*> streamBars;
    QMap<QString, StreamMarketDepthQuote*> streamMarketDepthQuotes;

    QMap<QString, QMap<Stream::StreamError, int>> streamErrorCounters;
    QSet<QString> unrecoveredTimeouts;
    QMap<QString, int> recoveredTimeouts;
    QMap<QString, int> unrecoveredTimeoutCounts;
};