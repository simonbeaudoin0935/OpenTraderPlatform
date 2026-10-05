#pragma once

#include <atomic>

#include <QMap>
#include <QStringList>

#include "MarketData/GetQuoteSnapshots/Quote.h"
#include "MarketData/StreamMarketData.h"

class StreamQuote final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamQuote(const QStringList& symbols, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamQuote() override;
    Q_DISABLE_COPY_MOVE(StreamQuote)

    [[nodiscard]] QStringList getSymbols() const
    {
        return m_symbols;
    }

    static size_t getNumberOfQuoteStreams()
    {
        return s_numberOfQuoteStreams.load();
    }

  signals:
    void newQuoteReceived(Quote quote);

  private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QStringList m_symbols;
    QMap<QString, QJsonObject> m_symbolState;
    static std::atomic<size_t> s_numberOfQuoteStreams;
};
