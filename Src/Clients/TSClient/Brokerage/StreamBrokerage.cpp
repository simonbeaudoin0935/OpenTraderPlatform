#include "StreamBrokerage.h"
#include "Logging.h"
#include "Assume.h"

#include <QJsonDocument>
#include <QJsonObject>

#define LOGGING_CATEGORY StreamLog

/**
 * @brief Handle Brokerage category error and status messages.
 *
 * Brokerage streams (Orders, Positions) receive two types of non-data messages:
 *
 * StreamStatus messages:
 * - EndSnapshot: Initial data snapshot is complete, emit endSnapshotReceived()
 * - GoAway:      Server shutting down, server closes connection, consumer reconnects via future
 *
 * ErrorResponse messages with Error types:
 * - Forbidden:          CRITICAL + assert (our code error, bad account/permissions)
 * - InternalServerError: WARNING, server closes connection, consumer reconnects via future
 * - ServiceUnavailable: CRITICAL + assert (our code error, bad request format)
 * - GatewayTimeout:     WARNING, server closes connection, consumer reconnects via future
 * - Failed:             WARNING, server closes connection, consumer reconnects via future
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
            m_streamError = StreamError::GoAway;
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
            m_streamError = StreamError::Forbidden;
            CRITICAL << "Forbidden error (bad account/permissions):" << message << "AccountID:" << accountID;
            OBJ_ASSUME_TRUE(false);
            return true;
        }

        if (errorStr == "ServiceUnavailable")
        {
            m_streamError = StreamError::ServiceUnavailable;
            CRITICAL << "ServiceUnavailable error (bad request format):" << message << "AccountID:" << accountID;
            OBJ_ASSUME_TRUE(false);
            return true;
        }

        if (errorStr == "InternalServerError")
        {
            m_streamError = StreamError::InternalServerError;
            WARNING << "InternalServerError:" << message << "AccountID:" << accountID;
            return true;
        }

        if (errorStr == "GatewayTimeout")
        {
            m_streamError = StreamError::GatewayTimeout;
            WARNING << "GatewayTimeout:" << message << "AccountID:" << accountID;
            return true;
        }

        if (errorStr == "Failed")
        {
            m_streamError = StreamError::Failed;
            WARNING << "Failed:" << message << "AccountID:" << accountID;
            return true;
        }

        // Unknown error type - log critical
        m_streamError = StreamError::InternalServerError;
        CRITICAL << "Unknown Brokerage error type '" << errorStr << "':" << message << "AccountID:" << accountID;
        return true;
    }

    return false;
}
