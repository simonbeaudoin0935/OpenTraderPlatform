#include "StreamBars.h"

StreamBars::StreamBars() :
    Stream()
{

}

StreamBars::~StreamBars()
{
}

bool StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    Bar bar(jsonObj);

    if (bar.isValid()) {
        emit receivedNewBar(bar);
        return true;
    } else {
        qCWarning(StreamLog) << "Bar malformed : " << bar.toJsonString();
        return false;
    }
}
