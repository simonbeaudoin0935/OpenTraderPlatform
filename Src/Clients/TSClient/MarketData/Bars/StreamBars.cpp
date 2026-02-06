#include "StreamBars.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

StreamBars::StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    this->setObjectName("Stream::Bars::" + symbol);

    DEBUG << "Stream created";
}

void StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    // Happy path: try to parse a Bar object
    Bar bar(jsonObj);

    if (!bar.isValid()) [[unlikely]]
    {
        WARNING << "Bar malformed : " << bar.toJsonString();
        return;
    }

    emit newBarReceived(bar);
}