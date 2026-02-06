#pragma once

#include "Stream.h"

/**
 * @brief Intermediate class for MarketData category streams (Bars, MarketDepthQuote).
 *
 * Handles the common error format for MarketData streams:
 * { "Error": "<type>", "Message": "<description>" }
 *
 * Error types: BadRequest, DualLogon, GoAway, InternalServerError
 */
class StreamMarketData : public Stream
{
    Q_OBJECT

  public:
    using Stream::Stream;

  protected:
    bool handleErrorOrStatus(const QJsonObject& jsonObj) override;
};
