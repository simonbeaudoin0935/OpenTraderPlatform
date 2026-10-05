#include "TSBarTimestampNormalizer.h"

#include <QString>

#include "CONSTANTS.h"

namespace
{
    [[nodiscard]] bool isValidMinuteOpenTimestamp(const QDateTime& p_timestamp, const int p_intervalSeconds)
    {
        if (!p_timestamp.isValid() || p_intervalSeconds <= 0)
        {
            return false;
        }

        const QTime time = p_timestamp.time();
        if (time.second() != 0 || time.msec() != 0)
        {
            return false;
        }

        if (time < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION ||
            time > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
        {
            return false;
        }

        const int secondsFromSessionOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.secsTo(time);
        if (secondsFromSessionOpen < 0)
        {
            return false;
        }

        return (secondsFromSessionOpen % p_intervalSeconds) == 0;
    }

    [[nodiscard]] QDateTime parseToMarketTimezone(const QString& p_rawTimestamp)
    {
        if (p_rawTimestamp.isEmpty())
        {
            return {};
        }

        QDateTime parsed = QDateTime::fromString(p_rawTimestamp, Qt::ISODate);
        if (!parsed.isValid())
        {
            return {};
        }

        if (parsed.timeZone().isValid())
        {
            parsed = parsed.toTimeZone(TradingHours::MARKET_TIMEZONE);
        }
        else
        {
            parsed.setTimeZone(TradingHours::MARKET_TIMEZONE);
        }

        return parsed;
    }
} // namespace

QDateTime TSBarTimestampNormalizer::normalizeToCanonicalBarTimestamp(const QString& p_rawTimestamp,
                                                                     const int p_minuteIntervalSeconds)
{
    const QDateTime parsed = parseToMarketTimezone(p_rawTimestamp);
    if (!parsed.isValid())
    {
        return {};
    }

    if (p_minuteIntervalSeconds <= 0)
    {
        return parsed;
    }

    // Canonical model is open-time.
    //
    // TradeStation minute bars are emitted with close-time timestamps in this
    // integration path, so we normalize by shifting one full interval backward
    // first (e.g. 16:53:00 -> 16:52:00 for a 1-minute bar).
    //
    // Keep a fallback for already-open timestamps that cannot be shifted into a
    // valid bar-open slot (e.g. 04:00:00 at the session boundary).
    const QDateTime shifted = parsed.addSecs(-p_minuteIntervalSeconds);
    if (isValidMinuteOpenTimestamp(shifted, p_minuteIntervalSeconds))
    {
        return shifted;
    }

    if (isValidMinuteOpenTimestamp(parsed, p_minuteIntervalSeconds))
    {
        return parsed;
    }

    return {};
}
