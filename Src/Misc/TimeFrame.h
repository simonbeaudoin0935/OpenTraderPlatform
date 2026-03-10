#pragma once

#include <QString>
#include <QMetaType>

/**
 * @enum TimeFrame
 * @brief Enumeration of available chart timeframes for bar data.
 *
 * Defines the supported time intervals for displaying candlestick charts.
 * Each timeframe represents the duration of each bar/candlestick in the chart.
 */
enum class TimeFrame
{
    ONE_MINUTE = 1,
    FIVE_MINUTES = 5,
    FIFTEEN_MINUTES = 15,
    THIRTY_MINUTES = 30,
    ONE_HOUR = 60,
    FOUR_HOURS = 240,
    ONE_DAY = 1440,   // 24 * 60 minutes
    ONE_WEEK = 10080, // 7 * 24 * 60 minutes
    ONE_MONTH = 43200 // 30 * 24 * 60 minutes (approximate)
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