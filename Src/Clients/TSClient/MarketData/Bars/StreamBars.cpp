#include "StreamBars.h"
#include "TSClient.h"

StreamBars::StreamBars(const QString &symbol, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    m_symbol(symbol)
{
    this->setObjectName("Stream::Bars::" + symbol);
}

void StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    Bar bar(jsonObj);

    if (!bar.isValid()) [[unlikely]] {
        qCWarning(StreamLog) << "Bar malformed : " << bar.toJsonString();
        return;
    }

    emit newBarReceived(bar);
}