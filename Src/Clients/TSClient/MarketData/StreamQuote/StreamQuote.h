#pragma once

#include <atomic>

#include <QJsonObject>
#include <QMap>
#include <QSet>
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
    // TradeStation Quote Stream is differential: first message is a full snapshot,
    // subsequent messages are delta patches with only changed fields present.
    // We accumulate the full state per symbol here and always emit the merged result.
    // We only start emitting once the initial snapshot (containing "Bid" and "Ask") has arrived.
    QMap<QString, QJsonObject> m_symbolState;
    QSet<QString> m_symbolsWithSnapshot;
    static std::atomic<size_t> s_numberOfQuoteStreams;
};
