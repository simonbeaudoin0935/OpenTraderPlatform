
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

StreamOrders::StreamOrders(QString &accountID, QObject *parent) :
    Stream("Orders " + accountID, parent),
    accountID(accountID)
{
    this->setObjectName("Stream::Orders::" + accountID);
}

StreamOrders::~StreamOrders()
{

}

bool StreamOrders::processJsonObject(const QJsonObject &jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) {
        StreamOrdersStatus status(jsonObj);
        if (status.isValid()) {
            // Update the EndSnapshot flag if we receive that status
            if (status.getStatus() == StreamOrdersStatus::Status::EndSnapshot) {
                receivedEndSnapshot = true;
                qCDebug(StreamLog) << Q_FUNC_INFO << "Received EndSnapshot status for account" << accountID;
            } else if (status.getStatus() == StreamOrdersStatus::Status::GoAway) {
                qCWarning(StreamLog) << Q_FUNC_INFO << "Received GoAway status for account" << accountID;
            }
            return true;
        } else {
            QJsonDocument doc(jsonObj);
            QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
            qCWarning(StreamLog) << Q_FUNC_INFO <<
                "Stream status object invalid : " << jsonString;
            return false;
        }
    }

    qDebug().noquote() << "***** " << jsonObj;

    // If not a status message, try to process as a position update
    Order order(jsonObj, receivedEndSnapshot);  // Pass the update flag based on EndSnapshot status
    if (order.isValid()) {
        emit receivedNewOrder(accountID, order);
        return true;
    } else {
        QJsonDocument doc(jsonObj);
        QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Position update object invalid : " << jsonString; // <<
//            "Malformed object to string : " << order.toJsonString();
        return false;
    }
}