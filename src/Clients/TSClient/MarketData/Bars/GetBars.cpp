#include "TSClient.h"

void TSClient::getBarsAsync(QString &symbol,
                            unsigned int interval,
                            Bar::BarUnit unit,
                            unsigned int barsback,
                            Bar::BarSessionTemplate sessionTemplate,
                            QDateTime firstDate,
                            QDateTime lastDate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    if (barsback > 0) Q_ASSERT(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);


    Q_ASSERT(!symbol.isEmpty());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BARS).arg(symbol), query);

    void* arg = static_cast<void*> (new QString(symbol));
    Q_CHECK_PTR(arg);

    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetBars), HttpMethod::GET, QByteArray(), arg);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Bars for symbols : " << symbol;
}

