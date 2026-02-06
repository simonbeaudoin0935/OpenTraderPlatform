#include "StreamBrokerage.h"
#include "Logging.h"
#include "Assume.h"

#include <QJsonObject>

#define LOGGING_CATEGORY StreamLog

/**
 * @brief Handle Brokerage category error/status messages.
 *
 * Brokerage streams (Orders, Positions) receive two types of control messages:
 *
 * 1. StreamStatus: { "StreamStatus": "EndSnapshot|GoAway" }
 *    - EndSnapshot: Initial snapshot complete, subsequent data is live. Emits endSnapshotReceived().
 *    - GoAway: Server shutting down, connection will close. Recoverable via future.
 *
 * 2. ErrorResponse: { "ErrorResponse": "...", "Error": "<type>", "Message": "...", "AccountID": "..." }
 *    - Forbidden:          CRITICAL + assert (bad account/permissions - our code error)
 *    - ServiceUnavailable: CRITICAL + assert (bad request format - our code error)
 *    - InternalServerError: WARNING, server closes connection, consumer reconnects via future
 *    - GatewayTimeout:     WARNING, server closes connection, consumer reconnects via future
 *    - Failed:             WARNING, server closes connection, consumer reconnects via future
 *
 * @return true if the JSON was an error/status message (handled), false if normal data
 */
bool StreamBrokerage::handleErrorOrStatus(const QJsonObject& jsonObj)
{
    // Check StreamStatus messages first
    if (jsonObj.contains("StreamStatus")) [[unlikely]]
    {
        const QString statusStr = jsonObj["StreamStatus"].toString();

        if (statusStr == "EndSnapshot")
        {
            m_receivedEndSnapshot = true;
            emit endSnapshotReceived();
            return true;
        }

        if (statusStr == "GoAway")
        {
            INFO << "Server shutting down (GoAway)";
            return true;
        }

        CRITICAL << "Unknown StreamStatus value:" << statusStr
                 << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return true;
    }

    // Check ErrorResponse messages
    if (jsonObj.contains("ErrorResponse")) [[unlikely]]
    {
        const QString errorStr = jsonObj["Error"].toString();
        const QString message = jsonObj["Message"].toString();
        const QString accountID = jsonObj["AccountID"].toString();

        m_jsonErrorString = errorStr + ": " + message + " (AccountID: " + accountID + ")";

        if (errorStr == "Forbidden")
        {
            CRITICAL << "Forbidden error (bad account/permissions):" << message << "AccountID:" << accountID;
            OBJ_ASSUME_TRUE(false);
            return true;
        }

        if (errorStr == "ServiceUnavailable")
        {
            CRITICAL << "ServiceUnavailable error (bad request format):" << message << "AccountID:" << accountID;
            OBJ_ASSUME_TRUE(false);
            return true;
        }

        if (errorStr == "InternalServerError")
        {
            WARNING << "InternalServerError:" << message << "AccountID:" << accountID;
            return true;
        }

        if (errorStr == "GatewayTimeout")
        {
            WARNING << "GatewayTimeout:" << message << "AccountID:" << accountID;
            return true;
        }

        if (errorStr == "Failed")
        {
            WARNING << "Failed:" << message << "AccountID:" << accountID;
            return true;
        }

        // Unknown error type - log critical
        CRITICAL << "Unknown Brokerage error type '" << errorStr << "':" << message << "AccountID:" << accountID;
        return true;
    }

    return false;
}
