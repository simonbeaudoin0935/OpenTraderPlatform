#pragma once

#include "Stream.h"

/**
 * @brief Intermediate class for MarketData category streams (Bars, Level2, Quote).
 *
 * Handles the common error format for TradeStation market-data streams:
 * { "Error": "<type>", "Message": "<description>" }
 */
class StreamMarketData : public Stream
{
    Q_OBJECT

  public:
    using Stream::Stream;

  protected:
    bool handleErrorOrStatus(const QJsonObject& jsonObj) override;
};
