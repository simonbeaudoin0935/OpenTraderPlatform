#include "StreamQuote.h"

#include "Assume.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

std::atomic<size_t> StreamQuote::s_numberOfQuoteStreams{0};

StreamQuote::StreamQuote(const QStringList& symbols, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbols(symbols)
{
    OBJ_ASSUME_FALSE(m_symbols.isEmpty());

    setObjectName(QString("Stream::Quote::%1symbols").arg(m_symbols.size()));

    ++s_numberOfQuoteStreams;
    DEBUG << "Quote stream created - symbols:" << m_symbols.size()
          << "total quote streams:" << s_numberOfQuoteStreams.load();
}

StreamQuote::~StreamQuote()
{
    if (s_numberOfQuoteStreams > 0)
    {
        --s_numberOfQuoteStreams;
    }

    DEBUG << "Quote stream destroyed - total quote streams:" << s_numberOfQuoteStreams.load();
}

void StreamQuote::processJsonObject(const QJsonObject& jsonObj)
{
    const QString symbol = jsonObj.value("Symbol").toString().trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return;
    }

    if (!m_symbols.contains(symbol)) [[unlikely]]
    {
        WARNING << "Received quote for unexpected symbol" << symbol;
        return;
    }

    QJsonObject& state = m_symbolState[symbol];
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        state.insert(it.key(), it.value());
    }

    Quote quote(state);
    if (!quote.isValid())
    {
        return;
    }

    emit newQuoteReceived(quote);
}
