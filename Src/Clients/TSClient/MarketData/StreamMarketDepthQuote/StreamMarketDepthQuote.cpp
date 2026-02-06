#include "StreamMarketDepthQuote.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog


StreamMarketDepthQuote::StreamMarketDepthQuote(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    this->setObjectName("Stream::MarketDepthQuote::" + symbol);

    DEBUG << "Stream created";
}

void StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (!quote.isValid()) [[unlikely]]
    {

        WARNING << "Market Depth Quote invalid. Received data : "
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));

        return;
    }

    if (quote.isLocked())
    {
        DEBUG << " Quote is locked";
    }
    else if (quote.isCrossed())
    {
        DEBUG << " Quote is crossed";
    }

    emit newMarketDepthQuoteReceived(quote);
}