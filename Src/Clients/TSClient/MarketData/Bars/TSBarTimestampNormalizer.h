#pragma once

#include <QDateTime>

class QString;

namespace TSBarTimestampNormalizer
{
    /**
 * @brief Parse a TradeStation bar timestamp and normalize minute bars to open-time.
 *
 * For non-minute bars, pass 0 and the function only parses timezone-normalized
 * RFC3339 input.
 *
 * For minute bars, pass `interval * 60`. The normalizer preserves already
 * open-time-aligned timestamps, and shifts close-time-shaped values back by one
 * interval when needed (for example, 20:00 -> 19:59 for 1-minute bars).
 *
 * @return A timestamp in TradingHours::MARKET_TIMEZONE, or invalid when parsing
 *         fails or minute timestamp cannot be interpreted as a valid open-time.
 */
    [[nodiscard]] QDateTime normalizeToCanonicalBarTimestamp(const QString& p_rawTimestamp,
                                                             int p_minuteIntervalSeconds);
} // namespace TSBarTimestampNormalizer
