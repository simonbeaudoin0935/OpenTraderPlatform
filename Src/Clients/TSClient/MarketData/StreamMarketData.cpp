#include "StreamMarketData.h"

#include <QJsonObject>

#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

bool StreamMarketData::handleErrorOrStatus(const QJsonObject& jsonObj)
{
    if (!jsonObj.contains("Error")) [[likely]]
    {
        return false;
    }

    const QString errorStr = jsonObj.value("Error").toString();
    const QString message = jsonObj.value("Message").toString();

    m_jsonErrorString = errorStr + ": " + message;

    if (errorStr == "BadRequest" || errorStr == "InvalidSymbol" ||
        errorStr.compare("FAILED, INVALID SYMBOL", Qt::CaseInsensitive) == 0)
    {
        m_streamError = StreamError::BadRequest;
        WARNING << "BadRequest error (market data request rejected):" << message;
        return true;
    }

    if (errorStr == "GoAway")
    {
        m_streamError = StreamError::GoAway;
        INFO << "Server shutting down (GoAway):" << message;
        return true;
    }

    if (errorStr == "DualLogon")
    {
        m_streamError = StreamError::DualLogon;
        WARNING << "DualLogon error:" << message;
        return true;
    }

    if (errorStr == "InternalServerError" || errorStr == "InternalError")
    {
        m_streamError = StreamError::InternalServerError;
        WARNING << "InternalServerError:" << message;
        return true;
    }

    m_streamError = StreamError::InternalServerError;
    CRITICAL << "Unknown MarketData error type '" << errorStr << "':" << message;
    return true;
}
