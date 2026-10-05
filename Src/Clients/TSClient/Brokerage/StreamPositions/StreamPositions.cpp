#include "StreamPositions.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "MockNetworkReply.h"

#define LOGGING_CATEGORY StreamLog

// Initialize static counter
size_t StreamPositions::s_numberOfPositionStreams = 0;

StreamPositions::StreamPositions(const QString& accountID, QNetworkReply* reply, QObject* parent)
    : StreamBrokerage(reply, parent), m_accountID(accountID)
{
    const QString suffix = qobject_cast<MockNetworkReply*>(reply) ? QStringLiteral("::mock") : QStringLiteral("::live");
    this->setObjectName("Stream::Positions::" + accountID + suffix);

    // Assert that we're not creating a second positions stream
    OBJ_ASSUME_EQUAL(s_numberOfPositionStreams, 0u);

    s_numberOfPositionStreams++;

    DEBUG << "Stream created - Total position streams:" << s_numberOfPositionStreams;
}

StreamPositions::~StreamPositions()
{
    s_numberOfPositionStreams--;

    DEBUG << "Stream destroyed - Total position streams:" << s_numberOfPositionStreams;
}

void StreamPositions::processJsonObject(const QJsonObject& jsonObj)
{
    // Check if this is a position deletion.
    // The TradeStation API documentation does NOT talk about this, but when a position completely sold
    // and theres no shares left in the position, we receive a position object with "Deleted": true.
    if (jsonObj.contains("Deleted") && jsonObj["Deleted"].toBool())
    {
        const QString positionID = jsonObj["PositionID"].toString().trimmed();
        if (positionID.isEmpty())
        {
            WARNING << "Ignoring malformed position deletion payload (missing PositionID):"
                    << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
            return;
        }

        emit positionDeleted(positionID);
        return;
    }

    // Defensive guard: ignore non-position payloads instead of asserting inside Position::isValid().
    // TradeStation occasionally emits malformed/control messages that are not full position snapshots.
    const QString accountID = jsonObj["AccountID"].toString().trimmed();
    const QString positionID = jsonObj["PositionID"].toString().trimmed();
    if (accountID.isEmpty() || positionID.isEmpty())
    {
        WARNING << "Ignoring malformed position payload (missing AccountID/PositionID):"
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        return;
    }

    // If not a status message, try to process as a position update
    Position position(jsonObj,
                      m_receivedEndSnapshot); // Pass the update flag based on EndSnapshot status

    // Position payload comes from an external stream, so invalid records are ignored after logging.
    if (!position.isValid()) [[unlikely]]
    {
        WARNING << "Ignoring invalid position payload:"
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return;
    }

    emit newPositionReceived(position);
}
