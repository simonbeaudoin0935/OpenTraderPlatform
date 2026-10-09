#include "TimeFrame.h"

/**
 * @brief Converts a TimeFrame enum value to its string representation.
 */
QString timeFrameToString(TimeFrame timeframe)
{
    switch (timeframe)
    {
    case TimeFrame::ONE_MINUTE:
        return "1m";
    case TimeFrame::FIVE_MINUTES:
        return "5m";
    case TimeFrame::FIFTEEN_MINUTES:
        return "15m";
    case TimeFrame::THIRTY_MINUTES:
        return "30m";
    case TimeFrame::ONE_HOUR:
        return "1h";
    case TimeFrame::FOUR_HOURS:
        return "4h";
    case TimeFrame::ONE_DAY:
        return "1d";
    case TimeFrame::ONE_WEEK:
        return "1w";
    case TimeFrame::ONE_MONTH:
        return "1M";
    default:
        return "1m"; // Default to 1 minute
    }
}

/**
 * @brief Converts a string representation to a TimeFrame enum value.
 */
TimeFrame stringToTimeFrame(const QString& timeframeStr)
{
    if (timeframeStr == "1m")
        return TimeFrame::ONE_MINUTE;
    if (timeframeStr == "5m")
        return TimeFrame::FIVE_MINUTES;
    if (timeframeStr == "15m")
        return TimeFrame::FIFTEEN_MINUTES;
    if (timeframeStr == "30m")
        return TimeFrame::THIRTY_MINUTES;
    if (timeframeStr == "1h")
        return TimeFrame::ONE_HOUR;
    if (timeframeStr == "4h")
        return TimeFrame::FOUR_HOURS;
    if (timeframeStr == "1d")
        return TimeFrame::ONE_DAY;
    if (timeframeStr == "1w")
        return TimeFrame::ONE_WEEK;
    if (timeframeStr == "1M")
        return TimeFrame::ONE_MONTH;

    // Default to 1 minute for invalid strings
    return TimeFrame::ONE_MINUTE;
}

TimeFrame normalizeDisplayTimeFrame(TimeFrame timeframe)
{
    switch (timeframe)
    {
    case TimeFrame::ONE_MINUTE:
    case TimeFrame::FIVE_MINUTES:
    case TimeFrame::FIFTEEN_MINUTES:
    case TimeFrame::THIRTY_MINUTES:
    case TimeFrame::ONE_HOUR:
    case TimeFrame::FOUR_HOURS:
    case TimeFrame::ONE_DAY:
    case TimeFrame::ONE_WEEK:
    case TimeFrame::ONE_MONTH:
        return timeframe;
    case TimeFrame::ONE_SECOND:
    default:
        return TimeFrame::ONE_MINUTE;
    }
}
