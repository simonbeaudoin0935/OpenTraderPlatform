#include "StreamPositions.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog


StreamPositions::StreamPositions(const QString &accountID, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    accountID(accountID),
    receivedEndSnapshot(false)
{
    this->setObjectName("Stream::Positions::" + accountID);
}

void StreamPositions::processJsonObject(const QJsonObject &jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) {
        StreamPositionStatus status(jsonObj);
        if (status.isValid()) {
            // Update the EndSnapshot flag if we receive that status
            if (status.getStatus() == StreamPositionStatus::Status::EndSnapshot) {
                receivedEndSnapshot = true;
                DEBUG << "Received EndSnapshot status for account" << accountID;
            } else if (status.getStatus() == StreamPositionStatus::Status::GoAway) {
                WARNING << "Received GoAway status for account" << accountID;
            }
        } else {
            WARNING << "Stream status object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
            return;
        }
        return;
    }

    // Check if this is a position deletion
    if (jsonObj.contains("Deleted") && jsonObj["Deleted"].toBool() && jsonObj.contains("PositionID")) {
        QString positionID = jsonObj["PositionID"].toString();
        emit positionDeleted(positionID);
        return;
    }
    // If not a status message, try to process as a position update
    Position position(jsonObj, receivedEndSnapshot); // Pass the update flag based on EndSnapshot status
    
    if (!position.isValid()) [[unlikely]] {
        WARNING << "Position update object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented)); 
        return;
    } 
    
    emit newPositionReceived(position);
}


bool StreamPositionStatus::isValid() const {
    // A status object is valid if it contains a non-empty status string
    return !statusString.isEmpty();
}

bool StreamPositionStatus::isStatusValid() const {
    // A status is valid if it's one of the known values
    return status != Status::Unknown;
}

QString StreamPositionStatus::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["StreamStatus"] = statusString;
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Compact));
}

StreamPositionStatus::StreamPositionStatus(const QJsonObject& jsonObj) {
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
