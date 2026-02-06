#include "StreamMarketData.h"
#include "Logging.h"
#include "Assume.h"

#include <QJsonObject>

#define LOGGING_CATEGORY StreamLog

/**
 * @brief Handle MarketData category error messages.
 *
 * MarketData streams (Bars, MarketDepthQuote) receive error objects with format:
 * { "Error": "<type>", "Message": "<description>" }
 *
 * Error types and their handling:
 * - BadRequest:          CRITICAL + assert (our code error)
 * - DualLogon:           WARNING, server closes connection, consumer reconnects via future
 * - GoAway:              INFO, server closes connection, consumer reconnects via future (normal server shutdown)
 * - InternalServerError: WARNING, server closes connection, consumer reconnects via future
 *
 * @return true if the JSON was an error message (handled), false if normal data
 */
bool StreamMarketData::handleErrorOrStatus(const QJsonObject& jsonObj)
{
    if (!jsonObj.contains("Error")) [[likely]]
    {
        return false;
    }

    const QString errorStr = jsonObj["Error"].toString();
    const QString message = jsonObj["Message"].toString();

    m_jsonErrorString = errorStr + ": " + message;

    if (errorStr == "BadRequest")
    {
        CRITICAL << "BadRequest error (malformed request):" << message;
        OBJ_ASSUME_TRUE(false);
        return true;
    }

    if (errorStr == "GoAway")
    {
        INFO << "Server shutting down (GoAway):" << message;
        return true;
    }

    if (errorStr == "DualLogon")
    {
        WARNING << "DualLogon error:" << message;
        return true;
    }

    if (errorStr == "InternalServerError")
    {
        WARNING << "InternalServerError:" << message;
        return true;
    }

    // Unknown error type - log critical
    CRITICAL << "Unknown MarketData error type '" << errorStr << "':" << message;
    return true;
}
