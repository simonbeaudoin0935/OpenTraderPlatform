#pragma once

#include <atomic>

#include <QStringList>

#include "Quote.h"
#include "StreamMarketData.h"

class StreamQuote final : public StreamMarketData
{
    Q_OBJECT

  public:
    explicit StreamQuote(const QStringList& symbols, QNetworkReply* reply, QObject* parent = nullptr);
    ~StreamQuote();
    Q_DISABLE_COPY_MOVE(StreamQuote)

    QStringList getSymbols() const
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
    static std::atomic<size_t> s_numberOfQuoteStreams;
};
