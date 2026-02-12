#pragma once

#include <QString>
#include <QFileInfo>
#include <QtSql/QSqlDatabase>
#include <QMap>
#include <QStringList>
#include <QObject>
#include <QTimer>

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
    void stopRecording();

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

    [[nodiscard]] qint64 getDatabaseFileSizeBytes() const
    {
        return QFileInfo(db.databaseName()).size();
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
    void openNextStream();

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

    // Stream ramp-up state
    QTimer m_rampTimer;
    int m_currentRampIndex = -1; // -1 = not ramping, >=0 = index of next symbol to open
};