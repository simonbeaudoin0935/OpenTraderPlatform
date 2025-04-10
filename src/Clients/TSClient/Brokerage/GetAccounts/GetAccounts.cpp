#include <QJsonArray>

#include "GetAccounts.h"
#include "../../TSClient.h"

bool TSClient::getAccountsSync(QVector<Account> &results)
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, ENDPOINT_GET_ACCOUNTS);
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
    const QJsonArray accountsArray = responseObj["Accounts"].toArray();

    // Resize the array in advance
    results.reserve(accountsArray.count());

    for (const QJsonValue &json: accountsArray) {
        results.push_back(Account(json.toObject()));
    }

    // This pointer to a JSON document was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::getAccountsAsync()
{
    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT, ENDPOINT_GET_ACCOUNTS);
    fetchAsync(request, static_cast<RequestTypeInt>(RequestType::GetAccounts));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching accounts";
}
