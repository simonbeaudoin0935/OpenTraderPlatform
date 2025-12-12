#include "StreamOrders.h"
#include "TSClient.h"

StreamOrders::StreamOrdersStatus::StreamOrdersStatus(const QJsonObject& jsonObj) {
    // Get the StreamStatus string
    statusString = jsonObj["StreamStatus"].toString();

    // Convert string to enum
    if (statusString == "EndSnapshot") {
        status = Status::EndSnapshot;
    } else if (statusString == "GoAway") {
        status = Status::GoAway;
    } else {
        status = Status::Unknown;
    }
}

bool StreamOrders::StreamOrdersStatus::isValid() const {
    // A status object is valid if it contains a non-empty status string
    return !statusString.isEmpty();
}

bool StreamOrders::StreamOrdersStatus::isStatusValid() const {
    // A status is valid if it's one of the known values
    return status != Status::Unknown;
}

StreamOrders::StreamOrders(const QString &accountID, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    accountID(accountID)
{
    this->setObjectName("Stream::Orders::" + accountID);
}

void StreamOrders::processJsonObject(const QJsonObject &jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) {
        StreamOrdersStatus status(jsonObj);
        if (status.isValid()) {
            // Update the EndSnapshot flag if we receive that status
            if (status.getStatus() == StreamOrdersStatus::Status::EndSnapshot) {
                receivedEndSnapshot = true;
                qCDebug(StreamLog) << "Received EndSnapshot status for account" << accountID;
            } else if (status.getStatus() == StreamOrdersStatus::Status::GoAway) {
                qCWarning(StreamLog) << "Received GoAway status for account" << accountID;
            }
        } else {
            qCWarning(StreamLog) << "Stream status object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
            return;
        }
        return;
    }

    // If not a status message, try to process as an order update
    Order order(jsonObj, receivedEndSnapshot);  // Pass the update flag based on EndSnapshot status

    if (!order.isValid()) [[unlikely]] {
        qCWarning(StreamLog) << "Order update object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented)); 
        return;
    } 
    
    m_promise.addResult(order);
    emit newOrderReceived(order);
}