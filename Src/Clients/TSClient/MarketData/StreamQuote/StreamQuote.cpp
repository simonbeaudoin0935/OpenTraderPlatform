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

    Quote quote(jsonObj);
    if (!quote.isValid()) [[unlikely]]
    {
        WARNING << "Quote invalid. Received data :" << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return;
    }

    OBJ_ASSUME_FALSE(quote.getSymbol().isEmpty());
    if (!m_symbols.contains(quote.getSymbol())) [[unlikely]]
    {
        WARNING << "Received quote for unexpected symbol" << quote.getSymbol();
        OBJ_ASSUME_TRUE(false);
        return;
    }

    emit newQuoteReceived(quote);
}
