#include "StreamMarketDepthQuote.h"
#include "TSClient.h"

StreamMarketDepthQuote::StreamMarketDepthQuote(const QString &symbol, QObject *parent) :
    Stream(symbol, parent)
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
        emit receivedNewMarketDepthQuote(symbol, quote);
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

StreamMarketDepthQuote* TSClient::openStreamMarketDepthQuote(const QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    const QString endpoint = ENDPOINT_STREAM_MARKET_DEPTH_QUOTE;
    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";

    return openStream<StreamMarketDepthQuote>(symbol, endpoint, query, symbol);
}

void TSClient::closeStreamMarketDepthQuote(StreamMarketDepthQuote *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamMarketDepthQuote " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}
