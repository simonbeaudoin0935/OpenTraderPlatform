#include "StreamBars.h"
#include "TSClient.h"

StreamBars::StreamBars(const QString &symbol, QObject *parent) :
    Stream(symbol, parent)
{
    this->setObjectName("Stream::Bars::" + symbol);
}

StreamBars::~StreamBars()
{
}

bool StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    Bar bar(jsonObj);

    if (bar.isValid()) {
        emit receivedNewBar(symbol, bar);
        return true;
    } else {
        qCWarning(StreamLog) << "Bar malformed : " << bar.toJsonString();
        return false;
    }
}