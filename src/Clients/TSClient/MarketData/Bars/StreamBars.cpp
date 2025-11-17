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

StreamBars *TSClient::openStreamBars(const QString &symbol,
                                     unsigned int interval,
                                     Bar::BarUnit unit,
                                     unsigned int barsback,
                                     Bar::BarSessionTemplate sessionTemplate,
                                     bool mock)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamBars";

    return openStream<StreamBars>(symbol, endpoint, query, symbol);
}

void TSClient::closeStreamBars(StreamBars *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamBars " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}
