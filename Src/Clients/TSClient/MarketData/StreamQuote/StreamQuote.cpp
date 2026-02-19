#include "StreamQuote.h"

#include <QJsonDocument>

#include "Assume.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

std::atomic<size_t> StreamQuote::s_numberOfQuoteStreams{0};

StreamQuote::StreamQuote(const QStringList& symbols, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbols(symbols)
{
    OBJ_ASSUME_FALSE(m_symbols.isEmpty());
    for (const QString& symbol: m_symbols)
    {
        OBJ_ASSUME_FALSE(symbol.isEmpty());
    }

    setObjectName(QString("Stream::Quote::%1symbols").arg(m_symbols.size()));

    s_numberOfQuoteStreams++;
    DEBUG << "Quote stream created - Symbols:" << m_symbols.size()
          << "Total quote streams:" << s_numberOfQuoteStreams.load();
}

StreamQuote::~StreamQuote()
{
    OBJ_ASSUME_GT(s_numberOfQuoteStreams.load(), static_cast<size_t>(0));
    s_numberOfQuoteStreams--;

    DEBUG << "Quote stream destroyed - Symbols:" << m_symbols.size()
          << "Total quote streams:" << s_numberOfQuoteStreams.load();
}

void StreamQuote::processJsonObject(const QJsonObject& jsonObj)
{
    OBJ_ASSUME_FALSE(jsonObj.isEmpty());

    // Non-quote messages (Heartbeat, Error) lack a "Symbol" field — pass through
    // without merging (no symbol to key on).
    const QString symbol = jsonObj.value("Symbol").toString();
    if (symbol.isEmpty())
    {
        Quote quote(jsonObj);
        if (!quote.isEmpty())
        {
            emit newQuoteReceived(quote);
        }
        return;
    }

    if (!m_symbols.contains(symbol)) [[unlikely]]
    {
        WARNING << "Received quote for unexpected symbol" << symbol;
        OBJ_ASSUME_TRUE(false);
        return;
    }

    // Merge delta into accumulated state: only keys present in this message are updated.
    // This handles TradeStation's differential stream where each message after the first
    // snapshot only contains changed fields.
    QJsonObject& state = m_symbolState[symbol];
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        state.insert(it.key(), it.value());
    }

    // The initial snapshot MUST always arrive before any delta for a given symbol.
    // If this fires, the snapshot was lost somewhere in the pipeline — that is a bug.
    OBJ_ASSUME_TRUE(state.contains("Bid") && state.contains("Ask"));

    Quote quote(state);
    OBJ_ASSUME_TRUE(quote.isValid());

    emit newQuoteReceived(quote);
}
