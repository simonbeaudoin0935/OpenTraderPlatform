#include "StreamMarketDepthQuote.h"

StreamMarketDepthQuote::StreamMarketDepthQuote(QString &symbol, QObject *parent) :
    Stream(parent),
    symbol(symbol)
{
}

StreamMarketDepthQuote::~StreamMarketDepthQuote()
{
}

bool StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (quote.isValid()) {
        emit receivedNewMarketDepthQuote(symbol, quote);
        return true;
    } else {
        QJsonDocument doc(jsonObj);
        QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Market Depth Quote invalid. Received data : " << jsonString <<
            "Malformed object to string : " << quote.toJsonString();

        return false;
    }




    return false;
}
