#include <QJsonArray>

#include "TSClient.h"

void TSClient::getBarsAsync(const Ticker &symbol,
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

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BARS).arg(symbol.toString()), query);

    void* arg = static_cast<void*> (new Ticker(symbol));
    Q_CHECK_PTR(arg);

    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetBars), HttpMethod::GET, QByteArray(), arg);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Bars for symbols : " << symbol.toString();
}

bool TSClient::getBarsSync(QVector<Bar> &results,
                           const Ticker &symbol,
                           unsigned int interval,
                           Bar::BarUnit unit,
                           unsigned int barsback,
                           Bar::BarSessionTemplate sessionTemplate,
                           QDateTime firstDate,
                           QDateTime lastDate) {
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    if (barsback > 0) Q_ASSERT(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);


    Q_ASSERT(!symbol.isEmpty());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BARS).arg(symbol.toString()), query);


    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (false == success) {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        return false;
    }

    // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    // The API returns a single object with an "Bar" array
    QJsonObject responseObj = jsonDocumentFromReplyToDelete->object();

    if (responseObj.contains("Error") && responseObj["Error"] == "NotFound") {
        results.reserve(0);

    } else { // Happy path with bars

        const QJsonArray barsArray = responseObj["Bars"].toArray();

        // Resize the array in advance
        results.reserve(barsArray.count());

        for (const QJsonValue &json: barsArray) {
            results.push_back(Bar(json.toObject()));
        }
    }


    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);
    return true;
}
