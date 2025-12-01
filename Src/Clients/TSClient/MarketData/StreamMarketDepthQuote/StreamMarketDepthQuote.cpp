#include "StreamMarketDepthQuote.h"
#include "TSClient.h"

StreamMarketDepthQuote::StreamMarketDepthQuote(const QString &symbol, QObject *parent) :
    Stream(parent),
    m_symbol(symbol)
{
    this->setObjectName("Stream::MarketDepthQuote::" + symbol);
}

StreamMarketDepthQuote::~StreamMarketDepthQuote()
{
}

bool StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (quote.isValid()) {
        if (quote.isLocked()) {
            qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is locked";
        } else if (quote.isCrossed()) {
            qCDebug(StreamLog) << Q_FUNC_INFO << " Quote is crossed";
        }
        emit receivedNewMarketDepthQuote(m_symbol, quote);
        return true;
    } else {
        QJsonDocument doc(jsonObj);
        QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Market Depth Quote invalid. Received data : " << jsonString;
        qCWarning(StreamLog).noquote() << Q_FUNC_INFO <<
            "Malformed object to string : " << quote.toJsonString();

        return false;
    }

    return false;
}