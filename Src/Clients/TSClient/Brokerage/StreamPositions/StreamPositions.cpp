#include "StreamPositions.h"
#include "TSClient.h"

StreamPositions::StreamPositions(const QString &accountID, QNetworkReply * reply, QObject *parent) :
    Stream(reply, parent),
    accountID(accountID),
    receivedEndSnapshot(false)
{
    this->setObjectName("Stream::Positions::" + accountID);

    // Install the promise wrapper in the base class
    setPromise(m_promise);
    m_future = m_promise.future();
    m_promise.start();
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
                qCDebug(StreamLog) << Q_FUNC_INFO << "Received EndSnapshot status for account" << accountID;
            } else if (status.getStatus() == StreamPositionStatus::Status::GoAway) {
                qCWarning(StreamLog) << "Received GoAway status for account" << accountID;
            }
        } else {
            qCWarning(StreamLog) << "Stream status object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
            return;
        }
        return;
    }

    // If not a status message, try to process as a position update
    Position position(jsonObj, receivedEndSnapshot); // Pass the update flag based on EndSnapshot status
    
    if (!position.isValid()) [[unlikely]] {
        qCWarning(StreamLog) << "Position update object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented)); 
        return;
    } 
    
    qCritical() << this->objectName() << "happy path, promise.addResult";

    m_promise.addResult(position);

    static int counter = 0;
    counter++;

            m_promise.finish();

    if (counter == 3) {
        qCritical() << this->objectName() << " added 3 positions";
      //  m_promise.finish();
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
