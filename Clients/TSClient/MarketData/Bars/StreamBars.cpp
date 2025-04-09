#include "StreamBars.h"
#include "../../TSClient.h"

StreamBars::StreamBars(QString &symbol, QObject *parent) :
    Stream(parent),
    symbol(symbol)
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


StreamBars *TSClient::openStreamBars(QString &symbol,
                                     unsigned int interval,
                                     Bar::BarUnit unit,
                                     unsigned int barsback,
                                     Bar::BarSessionTemplate sessionTemplate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    StreamBars * stream = new StreamBars(symbol);
    stream->moveToThread(thread);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamBars" << static_cast<void*>(stream);

    TSClient::openStream(symbol, endpoint, query, stream);

    return stream;
}

void TSClient::closeStreamBars(StreamBars *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamBars " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}
