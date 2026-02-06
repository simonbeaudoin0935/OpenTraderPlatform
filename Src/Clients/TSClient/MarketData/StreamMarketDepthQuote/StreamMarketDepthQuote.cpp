#include "StreamMarketDepthQuote.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog


StreamMarketDepthQuote::StreamMarketDepthQuote(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    this->setObjectName("Stream::MarketDepthQuote::" + symbol);

    INFO << "Stream created";
}

void StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (!quote.isValid()) [[unlikely]]
    {

        qCWarning(StreamLog) << Q_FUNC_INFO << "Market Depth Quote invalid. Received data : "
                             << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        qCWarning(StreamLog).noquote() << Q_FUNC_INFO << "Malformed object to string : " << quote.toJsonString();

        return;
    }

    if (quote.isLocked())
    {
        qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is locked";
    }
    else if (quote.isCrossed())
    {
        qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is crossed";
    }

    emit newMarketDepthQuoteReceived(quote);
}