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
    if (jsonObj.contains("Deleted") && jsonObj["Deleted"].toBool() && jsonObj.contains("PositionID"))
    {
        QString positionID = jsonObj["PositionID"].toString();
        emit positionDeleted(positionID);
        return;
    }

    // If not a status message, try to process as a position update
    Position position(jsonObj,
                      m_receivedEndSnapshot); // Pass the update flag based on EndSnapshot status

    // ASSERT that position is valid (pre-condition: JSON should always produce valid Position)
    if (!position.isValid()) [[unlikely]]
    {
        CRITICAL << "Position update object invalid : "
                 << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        OBJ_ASSUME_TRUE(position.isValid()); // ASSERT - this should never happen
        return;
    }

    emit newPositionReceived(position);
}