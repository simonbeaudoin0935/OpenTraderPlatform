#include "StreamMarketDepthQuote.h"
#include "TSClient.h"

StreamMarketDepthQuote::StreamMarketDepthQuote(const QString &symbol, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    m_symbol(symbol)
{
    this->setObjectName("Stream::MarketDepthQuote::" + symbol);

    // Install the promise wrapper in the base class
    setPromise(m_promise);
    m_promise.start();
}

void StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (!quote.isValid()) [[unlikely]] {

        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Market Depth Quote invalid. Received data : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        qCWarning(StreamLog).noquote() << Q_FUNC_INFO <<
            "Malformed object to string : " << quote.toJsonString();

        return;
    }
    
    if (quote.isLocked()) {
        qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is locked";
    } else if (quote.isCrossed()) {
        qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is crossed";
    }
  
    m_promise.addResult(quote);
}