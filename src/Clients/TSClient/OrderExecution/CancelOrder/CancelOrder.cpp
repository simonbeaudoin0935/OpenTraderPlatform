#include <QJsonDocument>
#include <QRegularExpression>

#include "CancelOrder.h"
#include "TSClient.h"

CancelOrderResult::CancelOrderResult(const QJsonObject& jsonObj)
{
    orderID = jsonObj["OrderID"].toString();
    message = jsonObj["Message"].toString();

    // Error field is optional
    if (jsonObj.contains("Error")) {
        error = jsonObj["Error"].toString();
    }
}

QString CancelOrderResult::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["OrderID"] = orderID;
    jsonObj["Message"] = message;
    if (error.has_value()) {
        jsonObj["Error"] = error.value();
    }

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}

bool TSClient::cancelOrderSync(const QString &orderID, CancelOrderResult &result)
{
    Q_ASSERT(!orderID.isEmpty());
    Q_ASSERT(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QJsonDocument *jsonDocumentFromReplyToDelete = nullptr;

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT,
                                           QString(ENDPOINT_CANCEL_ORDER).arg(orderID));

    bool success = fetchSync(request,
                             jsonDocumentFromReplyToDelete,
                             HttpMethod::DELETE);

    if (!success) {
        // Make sure that if fetchSync failed that this pointed has not been allocated
        Q_ASSERT(jsonDocumentFromReplyToDelete == nullptr);

        return false;
    }


    // The positive return value implies jsonDocumentFromReplyToDelete has been allocated to something
    Q_ASSERT(jsonDocumentFromReplyToDelete != nullptr);

    result = CancelOrderResult(jsonDocumentFromReplyToDelete->object());

    qCDebug(TSClientLog).noquote() << Q_FUNC_INFO << "Received JSON : \n" << jsonDocumentFromReplyToDelete->toJson(QJsonDocument::Indented);

    // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
    TRACK_DELETED_JSON_ARRAY(delete jsonDocumentFromReplyToDelete);

    return true;
}

void TSClient::cancelOrderAsync(const QString &orderID)
{
    Q_ASSERT(!orderID.isEmpty());
    Q_ASSERT(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QNetworkRequest request = buildRequest(API_KEY_PLACEMENT,
                                           QString(ENDPOINT_CANCEL_ORDER).arg(orderID));

    fetchAsync(request,
               static_cast<RequestTypeInt>(RequestType::CancelOrder),
               HttpMethod::DELETE);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Cancel order async";

}
