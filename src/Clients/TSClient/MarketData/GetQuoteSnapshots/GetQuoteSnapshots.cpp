#include <QJsonArray>

#include "TSClient.h"

bool TSClient::getQuoteSnapshotsSync(QString &symbols, QVector<QuoteSnapshot> &quoteSnapshots)
{
    Q_ASSERT(!symbols.isEmpty());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_QUOTE_SNAPSHOTS).arg(symbols));
    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    bool success = fetchSync(request, jsonDocumentFromReplyToDelete);

    if (false == success) {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        return false;
    }

    // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    // The API returns a single object with an "Accounts" array
    QJsonObject responseObj = jsonDocumentFromReplyToDelete->object();
    const QJsonArray quoteSnapshotsArray = responseObj["Quotes"].toArray();

    // Resize the array in advance
    quoteSnapshots.reserve(quoteSnapshotsArray.count());

    for (const QJsonValue &json: quoteSnapshotsArray) {
        quoteSnapshots.push_back(QuoteSnapshot(json.toObject()));
    }

    // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::getQuoteSnapshotsAsync(QString &symbols)
{
    Q_ASSERT(!symbols.isEmpty());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_QUOTE_SNAPSHOTS).arg(symbols));
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetQuoteSnapshots));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching quotes for symbols : " << symbols;
}
