#pragma once

#include "StreamMarketData.h"
#include "Bar.h"

class StreamBars final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamBars();
    Q_DISABLE_COPY_MOVE(StreamBars)

    QString getSymbol() const
    {
        return m_symbol;
    }

    /**
     * @brief Get the number of currently open bar streams
     * @return Current count of active bar streams
     * @note No limit enforced on bar streams
     */
    static size_t getNumberOfBarsStreams()
    {
        return s_numberOfBarsStreams;
    }

  signals:
    void newBarReceived(Bar bar);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;

    /// Counter for concurrent bar streams (no limit enforced)
    static size_t s_numberOfBarsStreams;
};
