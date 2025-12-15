#include "StreamBars.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

StreamBars::StreamBars(const QString &symbol, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    m_symbol(symbol)
{
    this->setObjectName("Stream::Bars::" + symbol);

    INFO << "Stream created";
}

void StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    if (jsonObj.contains("Error")) [[unlikely]] {
            
        QString errorStr = jsonObj["Error"].toString();
        QString message =  jsonObj["Message"].toString();
            
        m_jsonErrorString = errorStr + ": " + message;

        CRITICAL << "Received error string '" << errorStr << "' and message: " << jsonObj["Message"].toString();
        
        return;
    }

    // Happy path: try to parse a Bar object
    Bar bar(jsonObj);

    if (!bar.isValid()) [[unlikely]] {
        qCWarning(StreamLog) << "Bar malformed : " << bar.toJsonString();
        return;
    }

    emit newBarReceived(bar);
}