#include <QJsonArray>

#include "TSClient.h"

bool TSClient::getBalancesSync(const QString accounts, QVector<Balance> &results) {
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BALANCES).arg(accounts));
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
    const QJsonArray balancesArray = responseObj["Balances"].toArray();

    // Resize the array in advance
    results.reserve(balancesArray.count());

    for (const QJsonValue &json: balancesArray) {
        results.push_back(Balance(json.toObject()));
    }

    // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::getBalancesAsync(const QString accounts) {
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, QString(ENDPOINT_GET_BALANCES).arg(accounts));
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetBalances));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Balances";
}
