#include "StreamBars.h"

StreamBars::StreamBars(QString &symbol, QObject *parent) :
    Stream(symbol, parent)
{

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
