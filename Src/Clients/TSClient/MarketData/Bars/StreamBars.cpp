#include "StreamBars.h"
#include "TSClient.h"
#include "Logging.h"
#include "MockNetworkReply.h"

#define LOGGING_CATEGORY StreamLog

// Initialize static counter
size_t StreamBars::s_numberOfBarsStreams = 0;

StreamBars::StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    const QString suffix = qobject_cast<MockNetworkReply*>(reply) ? QStringLiteral("::mock") : QStringLiteral("::live");
    this->setObjectName("Stream::Bars::" + symbol + suffix);

    s_numberOfBarsStreams++;

    DEBUG << "Stream created - Total bar streams:" << s_numberOfBarsStreams;
}

StreamBars::~StreamBars()
{
    s_numberOfBarsStreams--;

    DEBUG << "Stream destroyed - Total bar streams:" << s_numberOfBarsStreams;
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