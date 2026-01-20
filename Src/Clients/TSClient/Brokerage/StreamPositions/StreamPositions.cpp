#include "StreamPositions.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY StreamLog


StreamPositions::StreamPositions(const QString& accountID, QNetworkReply* reply, QObject* parent)
    : Stream(reply, parent), m_accountID(accountID)
{
    this->setObjectName("Stream::Positions::" + accountID);

    INFO << "Stream created";
}

void StreamPositions::processJsonObject(const QJsonObject& jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) [[unlikely]]
    {
        QString statusStr = jsonObj["StreamStatus"].toString();
        if (statusStr == "EndSnapshot")
        {
            m_receivedEndSnapshot = true;
            emit endSnapshotReceived();
        }
        else if (statusStr == "GoAway")
        {
            WARNING << "Received GoAway status for account" << m_accountID;
            // TODO handle this properly
            Q_UNREACHABLE();
        }
        else
        {
            WARNING << "Stream status object invalid : "
                    << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        }

        return;
    }

    if (jsonObj.contains("ErrorResponse")) [[unlikely]]
    {
        QString errorStr = jsonObj["Error"].toString();
        QString message = jsonObj["Message"].toString();
        QString accountID = jsonObj["AccountID"].toString();

        m_jsonErrorString = errorStr + ": " + message + " (AccountID: " + accountID + ")";

        CRITICAL << "Received error string '" << errorStr << "' and message: " << message << " for account "
                 << accountID;

        return;
    }

    // Check if this is a position deletion.
    // The TradeStation API documentation does NOT talk about this, but when a position completely sold
    // and theres no shares left in the position, we receive a position object with "Deleted": true.
    if (jsonObj.contains("Deleted") && jsonObj["Deleted"].toBool() && jsonObj.contains("PositionID"))
    {
        QString positionID = jsonObj["PositionID"].toString();
        emit positionDeleted(positionID);
        return;
    }

    // If not a status message, try to process as a position update
    Position position(jsonObj,
                      m_receivedEndSnapshot); // Pass the update flag based on EndSnapshot status

    if (!position.isValid()) [[unlikely]]
    {
        WARNING << "Position update object invalid : "
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return;
    }

    emit newPositionReceived(position);
}