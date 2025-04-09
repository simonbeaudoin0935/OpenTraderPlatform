#include "StreamPositions.h"
#include <QJsonDocument>

bool StreamPositions::StreamPositionStatus::isValid() const {
    // A status object is valid if it contains a non-empty status string
    return !statusString.isEmpty();
}

bool StreamPositions::StreamPositionStatus::isStatusValid() const {
    // A status is valid if it's one of the known values
    return status != Status::Unknown;
}

QString StreamPositions::StreamPositionStatus::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["StreamStatus"] = statusString;
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Compact));
}

StreamPositions::StreamPositions(QString &account, QObject *parent) :
    Stream(parent),
    account(account),
    receivedEndSnapshot(false)
{}

StreamPositions::~StreamPositions()
{

}

bool StreamPositions::processJsonObject(const QJsonObject &jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) {
        StreamPositionStatus status(jsonObj);
        if (status.isValid()) {
            // Update the EndSnapshot flag if we receive that status
            if (status.getStatus() == StreamPositionStatus::Status::EndSnapshot) {
                receivedEndSnapshot = true;
                qCDebug(StreamLog) << Q_FUNC_INFO << "Received EndSnapshot status for account" << account;
            } else if (status.getStatus() == StreamPositionStatus::Status::GoAway) {
                qCWarning(StreamLog) << Q_FUNC_INFO << "Received GoAway status for account" << account;
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

    // If not a status message, try to process as a position update
    Position position(jsonObj, receivedEndSnapshot);  // Pass the update flag based on EndSnapshot status
    if (position.isValid()) {
        emit receivedNewPosition(account, position);
        return true;
    } else {
        QJsonDocument doc(jsonObj);
        QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Position update object invalid : " << jsonString <<
            "Malformed object to string : " << position.toJsonString();
        return false;
    }
}


StreamPositions::StreamPositionStatus::StreamPositionStatus(const QJsonObject& jsonObj) {
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
