#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>
#include <QObject>

#include "StreamBars.h"
#include "StreamMarketDepthQuote.h"

Q_DECLARE_LOGGING_CATEGORY(LiveStreamDBLog)

class LiveStreamDB : public QObject
{
    Q_OBJECT

  public:
    enum class StreamType
    {
        Bars,
        MarketDepthQuotes
    };

    LiveStreamDB(StreamType type, const QString& dbPath, QStringList& p_stockTickers);
    ~LiveStreamDB();

    bool isOpen() const;
    void startRecording();

    QMap<QString, int> getRecoveredTimeouts() const
    {
        return recoveredTimeouts;
    }
    QMap<QString, int> getUnrecoveredTimeoutCounts() const
    {
        return unrecoveredTimeoutCounts;
    }
    QMap<QString, int> getRecoveryAttempts() const
    {
        return recoveryAttempts;
    }
    QMap<QString, int> getSuccessfulRecoveries() const
    {
        return successfulRecoveries;
    }

    int getRecordCount() const;
    int getActiveStreamCount() const;
    int getTotalConfiguredStreams() const
    {
        return stockTickers.size();
    }

    void finalizeUnrecoveredTimeouts();
    void attemptStreamRecovery(const QString& symbol);

    [[nodiscard]] QMap<QString, QMap<Stream::StreamError, int>> getErrorCounters() const
    {
        return m_streamErrorCounters;
    }

  private slots:
    void onReceivedNewRawDataForStock(QString symbol, const QByteArray& rawData);

  private:
    bool storeData(const QString& stock, qint64 epochMs, const QByteArray& rawData);
    void handleStreamError(const QString& symbol, Stream::StreamError reason, const QString& message);

    StreamType streamType;
    QStringList stockTickers;
    QSqlDatabase db;
    QMap<QString, int> stockSequences;

    // Union-like storage for different stream types
    QMap<QString, QPointer<StreamBars>> m_streamBars;
    QMap<QString, QPointer<StreamMarketDepthQuote>> m_streamMarketDepthQuotes;

    QMap<QString, QMap<Stream::StreamError, int>> m_streamErrorCounters;
    QSet<QString> unrecoveredTimeouts;
    QMap<QString, int> recoveredTimeouts;
    QMap<QString, int> unrecoveredTimeoutCounts;
    QMap<QString, int> recoveryAttempts;
    QMap<QString, int> successfulRecoveries;
};