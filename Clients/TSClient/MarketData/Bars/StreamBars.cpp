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


StreamBars *TSClient::openStreamBars(QString &symbol, unsigned int interval, Bar::BarUnit unit, unsigned int barsback, Bar::BarSessionTemplate sessionTemplate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query;
    query.addQueryItem("interval", QString::number(interval));
    query.addQueryItem("unit", [unit]() -> QString {
        switch (unit) {
        case Bar::BarUnit::Minute: return "Minute";
        case Bar::BarUnit::Daily: return "Daily";
        case Bar::BarUnit::Weekly: return "Weekly";
        case Bar::BarUnit::Monthly: return "Monthly";
        default: Q_UNREACHABLE_RETURN("Unknown");
        }
    }());
    query.addQueryItem("barsback", QString::number(barsback));
    query.addQueryItem("sessiontemplate", [sessionTemplate]() -> QString {
        switch (sessionTemplate) {
        case Bar::BarSessionTemplate::USEQPre: return "USEQPre";
        case Bar::BarSessionTemplate::USEQPost: return "USEQPost";
        case Bar::BarSessionTemplate::USEPreAndPost: return "USEPreAndPost";
        case Bar::BarSessionTemplate::USEQ24Hour: return "USEQ24Hour";
        case Bar::BarSessionTemplate::Default: return "Default";
        default: Q_UNREACHABLE_RETURN("Unknown");
        }
    }());

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
