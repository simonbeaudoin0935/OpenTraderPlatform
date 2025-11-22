#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>
#include <QObject>

#include "StreamBars.h"
#include "StreamMarketDepthQuote.h"
#include "Ticker.h"

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

    QMap<Ticker, QMap<Stream::StreamError, int>> getErrorCounters() const { return streamErrorCounters; }
    QMap<Ticker, int> getRecoveredTimeouts() const { return recoveredTimeouts; }
    QMap<Ticker, int> getUnrecoveredTimeoutCounts() const { return unrecoveredTimeoutCounts; }
    QMap<Ticker, int> getRecoveryAttempts() const { return recoveryAttempts; }
    QMap<Ticker, int> getSuccessfulRecoveries() const { return successfulRecoveries; }

    int getRecordCount() const;
    int getActiveStreamCount() const;
    int getTotalConfiguredStreams() const { return stockTickers.size(); }

    void finalizeUnrecoveredTimeouts();
    void attemptStreamRecovery(const Ticker& symbol);

private slots:
    void onReceivedNewRawDataForStock(Ticker symbol, const QByteArray& rawData);
    void onStreamErrorOccurred(Stream::StreamError error, QString errorMessage);

private:
    bool storeData(const Ticker& stock, qint64 epochMs, const QByteArray& rawData);

    StreamType streamType;
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<Ticker, int> stockSequences;

    // Union-like storage for different stream types
    QMap<Ticker, StreamBars*> streamBars;
    QMap<Ticker, StreamMarketDepthQuote*> streamMarketDepthQuotes;

    QMap<Ticker, QMap<Stream::StreamError, int>> streamErrorCounters;
    QSet<Ticker> unrecoveredTimeouts;
    QMap<Ticker, int> recoveredTimeouts;
    QMap<Ticker, int> unrecoveredTimeoutCounts;
    QMap<Ticker, int> recoveryAttempts;
    QMap<Ticker, int> successfulRecoveries;
};