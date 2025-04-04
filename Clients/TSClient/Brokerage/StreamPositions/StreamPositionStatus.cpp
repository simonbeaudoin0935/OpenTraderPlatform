#include "StreamPositionStatus.h"
#include <QJsonDocument>

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