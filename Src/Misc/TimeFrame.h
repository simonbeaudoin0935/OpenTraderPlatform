#pragma once

#include <QString>
#include <QMetaType>

/**
 * @enum TimeFrame
 * @brief Enumeration of available chart timeframes for bar data.
 *
 * Defines the supported time intervals for displaying candlestick charts.
 * Each value represents the duration of one bar in **seconds**.
 *
 * ONE_SECOND is an internal data-source type only (Databento Ohlcv1S).
 * It is not shown in the chart UI; it is used as the aggregate source for TEN_SECONDS.
 */
enum class TimeFrame
{
    ONE_SECOND = 1,        // Internal data source only (not shown in chart UI)
    TEN_SECONDS = 10,      // 10-second candles
    ONE_MINUTE = 60,       // 1 * 60 seconds
    FIVE_MINUTES = 300,    // 5 * 60 seconds
    FIFTEEN_MINUTES = 900, // 15 * 60 seconds
    THIRTY_MINUTES = 1800, // 30 * 60 seconds
    ONE_HOUR = 3600,       // 60 * 60 seconds
    FOUR_HOURS = 14400,    // 4 * 60 * 60 seconds
    ONE_DAY = 86400,       // 24 * 60 * 60 seconds
    ONE_WEEK = 604800,     // 7 * 24 * 60 * 60 seconds
    ONE_MONTH = 2592000    // 30 * 24 * 60 * 60 seconds (approximate)
};

// Required for Qt queued (cross-thread) signal/slot connections carrying TimeFrame arguments
Q_DECLARE_METATYPE(TimeFrame)

/**
 * @brief Converts a TimeFrame enum value to its string representation.
 *
 * @param timeframe The TimeFrame enum value to convert.
 * @return QString representation of the timeframe (e.g., "1m", "5m", "1h", etc.).
 */
QString timeFrameToString(TimeFrame timeframe);

/**
 * @brief Converts a string representation to a TimeFrame enum value.
 *
 * @param timeframeStr The string representation of the timeframe.
 * @return TimeFrame enum value, or ONE_MINUTE if the string is invalid.
 */
TimeFrame stringToTimeFrame(const QString& timeframeStr);