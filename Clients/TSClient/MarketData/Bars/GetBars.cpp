#include "../../TSClient.h"

void TSClient::getBarsAsync(QString &symbol,
                            unsigned int interval,
                            Bar::BarUnit unit,
                            unsigned int barsback,
                            Bar::BarSessionTemplate sessionTemplate,
                            QDateTime firstDate,
                            QDateTime lastDate)
{
    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);


    Q_ASSERT(!symbol.isEmpty());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BARS).arg(symbol), query);
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetQuoteSnapshots));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Bars for symbols : " << symbol;
}

