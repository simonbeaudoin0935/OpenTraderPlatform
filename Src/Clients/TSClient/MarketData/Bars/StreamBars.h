#pragma once

#include "Core/Models/Bar.h"
#include "MarketData/StreamMarketData.h"

class StreamBars final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamBars(const QString& symbol,
                        QNetworkReply* reply,
                        QObject* parent = nullptr,
                        int p_minuteIntervalSeconds = 0);
    ~StreamBars() override;
    Q_DISABLE_COPY_MOVE(StreamBars)

    [[nodiscard]] QString getSymbol() const
    {
        return m_symbol;
    }

    static size_t getNumberOfBarsStreams()
    {
        return s_numberOfBarsStreams;
    }

  signals:
    void newBarReceived(Bar bar);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_symbol;
    int m_minuteIntervalSeconds = 0;
    static size_t s_numberOfBarsStreams;
};
