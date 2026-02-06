#pragma once

#include "Stream.h"

#include <QJsonDocument>

/**
 * @brief Intermediate class for Brokerage category streams (Orders, Positions).
 *
 * Handles two error/status message formats:
 * 1. StreamStatus: { "StreamStatus": "EndSnapshot|GoAway" }
 * 2. ErrorResponse: { "ErrorResponse": "...", "Error": "<type>", "Message": "...", "AccountID": "..." }
 *
 * StreamStatus types: EndSnapshot (emits signal), GoAway (recoverable)
 * Error types: Forbidden, InternalServerError, ServiceUnavailable, GatewayTimeout, Failed
 */
class StreamBrokerage : public Stream
{
    Q_OBJECT

  public:
    using Stream::Stream;

  protected:
    bool handleErrorOrStatus(const QJsonObject& jsonObj) override;

    bool m_receivedEndSnapshot = false;
};
