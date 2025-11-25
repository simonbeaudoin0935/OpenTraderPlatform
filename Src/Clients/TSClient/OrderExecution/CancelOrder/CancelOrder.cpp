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