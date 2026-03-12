#pragma once

#include <QDateTime>

#include "CONSTANTS.h"
#include "TimeFrame.h"

/**
 * @namespace ChartTimeUtils
 * @brief Utilities for time-anchored chart indexing
 *
 * Provides functions to:
 * 1. Compute the "index 0" bar timestamp from the current time
 * 2. Convert between bar timestamps and chart indices (4:00 AM = index 0)
 * - Intra-day gaps (sparse stocks) show as missing indices
 * - Index 0 is always "now" (or replay start time)
 */
namespace ChartTimeUtils
{

    /**
 * @brief Compute the index 0 bar timestamp for chart time-anchoring.
 *
 * Returns the bar open timestamp that should be at chart index 0.
 * During market hours, this is the current minute's bar open time.
 * Outside market hours, it's the last tradable bar (18:59) of the most
 * recent trading day.
 *
 * @param p_currentTime Current time (live clock or replay time)
 * @return QDateTime The bar timestamp for index 0 (always a valid bar open time on a weekday)
 */
    inline QDateTime computeIndex0Timestamp(const QDateTime& p_currentTime)
    {
        const QDateTime mt = p_currentTime.toTimeZone(TradingHours::MARKET_TIMEZONE);
        const QTime time = mt.time();
        const QDate date = mt.date();
        const int dow = date.dayOfWeek();
        const bool isWeekday = (dow >= TradingHours::MONDAY && dow <= TradingHours::FRIDAY);

        // During market hours on a weekday: bar open = floor(now) to current minute
        if (isWeekday && time >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION &&
            time <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
        {
            QTime barOpen(time.hour(), time.minute(), 0);

            if (barOpen > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
                barOpen = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

            return QDateTime(date, barOpen, TradingHours::MARKET_TIMEZONE);
        }

        // Outside market hours: find most recent trading day's last bar
        QDate tradingDate = date;

        // After market close on a weekday → today's last bar
        if (isWeekday && time > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
            return QDateTime(tradingDate,
                             TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                             TradingHours::MARKET_TIMEZONE);

        // Before market open on a weekday → previous day
        if (isWeekday)
            tradingDate = tradingDate.addDays(-1);

        // Roll back to most recent Friday if on weekend
        const int tdow = tradingDate.dayOfWeek();
        if (tdow == 7) // Sunday
            tradingDate = tradingDate.addDays(-2);
        else if (tdow == 6) // Saturday
            tradingDate = tradingDate.addDays(-1);

        return QDateTime(tradingDate,
                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                         TradingHours::MARKET_TIMEZONE);
    }

    // ── Internal helpers ──────────────────────────────────────────────────────

    namespace detail
    {
        /**
     * @brief Count trading days from a fixed epoch to a given date.
     * Epoch: 2000-01-03 (Monday) = trading day 0.
     * Only valid for weekday dates.
     */
        inline qint64 countTradingDays(const QDate& p_date)
        {
            static const QDate epoch(2000, 1, 3); // Monday
            const qint64 totalDays = epoch.daysTo(p_date);
            const qint64 weeks = totalDays / 7;
            const int remainder = static_cast<int>(totalDays % 7);
            return weeks * 5 + qMin(remainder, 5);
        }

        /**
     * @brief Convert a trading day count back to a calendar date.
     */
        inline QDate tradingDayToDate(qint64 p_tradingDay)
        {
            static const QDate epoch(2000, 1, 3); // Monday
            qint64 weeks;
            qint64 remainder;
            if (p_tradingDay >= 0)
            {
                weeks = p_tradingDay / 5;
                remainder = p_tradingDay % 5;
            }
            else
            {
                weeks = (p_tradingDay - 4) / 5;
                remainder = p_tradingDay - weeks * 5;
            }
            return epoch.addDays(weeks * 7 + remainder);
        }
    } // namespace detail

    // ── Public API ────────────────────────────────────────────────────────────

    /**
 * @brief Convert a bar timestamp to an absolute "trading second" index.
 *
 * Trading seconds are a continuous count that skips weekends and overnight gaps.
 * Each trading day contributes 54 000 seconds (4:00:00 to 18:59:59).
 *
 * @param p_barTimestamp A valid bar open timestamp (4:00-18:59 on a weekday)
 * @return Absolute trading second count
 */
    inline qint64 toAbsoluteTradingSecond(const QDateTime& p_barTimestamp)
    {
        const QDateTime mt = p_barTimestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
        const qint64 tradingDay = detail::countTradingDays(mt.date());
        const QTime time = mt.time();
        const int secondOfDay = (time.hour() - TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour()) * 3600 +
                                time.minute() * 60 + time.second();
        return tradingDay * static_cast<qint64>(BarsConstants::MINUTE_BARS_PER_DAY) * 60 + secondOfDay;
    }

    /**
 * @brief Convert a bar timestamp to an absolute "trading minute" index.
 *
 * Trading minutes are a continuous count that skips weekends and overnight gaps.
 * Each trading day contributes 900 minutes (4:00 to 18:59).
 *
 * @param p_barTimestamp A valid bar open timestamp (4:00-18:59 on a weekday)
 * @return Absolute trading minute count
 */
    inline qint64 toAbsoluteTradingMinute(const QDateTime& p_barTimestamp)
    {
        const QDateTime mt = p_barTimestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
        const qint64 tradingDay = detail::countTradingDays(mt.date());
        const int minuteOfDay = static_cast<int>(BarsConstants::timeToIndex(mt.time()));
        return tradingDay * BarsConstants::MINUTE_BARS_PER_DAY + minuteOfDay;
    }

    /**
 * @brief Convert a bar timestamp to a chart index relative to index 0.
 *
 * For minute-granularity timeframes (1m and above), each chart index unit
 * corresponds to one trading minute. For sub-minute timeframes (TEN_SECONDS),
 * each unit corresponds to one 10-second slot.
 *
 * @param p_barTimestamp   The bar's open timestamp
 * @param p_index0Timestamp The index 0 anchor timestamp (from computeIndex0Timestamp)
 * @param p_tf             The current display timeframe (determines index granularity)
 * @return Chart index (negative = before index 0, positive = after)
 */
    inline int timestampToChartIndex(const QDateTime& p_barTimestamp,
                                     const QDateTime& p_index0Timestamp,
                                     TimeFrame p_tf = TimeFrame::ONE_MINUTE)
    {
        if (p_tf == TimeFrame::TEN_SECONDS)
        {
            // 10-second slot granularity
            return static_cast<int>(
                (toAbsoluteTradingSecond(p_barTimestamp) - toAbsoluteTradingSecond(p_index0Timestamp)) / 10);
        }
        // Minute granularity (default, works for all TFs >= 1m)
        return static_cast<int>(toAbsoluteTradingMinute(p_barTimestamp) - toAbsoluteTradingMinute(p_index0Timestamp));
    }

    /**
 * @brief Convert a chart index to a bar timestamp, given the index 0 anchor.
 *
 * @param p_index           Chart index
 * @param p_index0Timestamp The index 0 anchor timestamp
 * @param p_tf              The current display timeframe (determines index granularity)
 * @return Bar open timestamp for the given index
 */
    inline QDateTime
    chartIndexToTimestamp(int p_index, const QDateTime& p_index0Timestamp, TimeFrame p_tf = TimeFrame::ONE_MINUTE)
    {
        if (p_tf == TimeFrame::TEN_SECONDS)
        {
            // 10-second slot granularity: index is in 10-second units
            const qint64 absoluteSecond =
                toAbsoluteTradingSecond(p_index0Timestamp) + static_cast<qint64>(p_index) * 10;
            constexpr qint64 secondsPerDay = static_cast<qint64>(BarsConstants::MINUTE_BARS_PER_DAY) * 60; // 54 000

            qint64 tradingDay;
            int secondOfDay;
            if (absoluteSecond >= 0)
            {
                tradingDay = absoluteSecond / secondsPerDay;
                secondOfDay = static_cast<int>(absoluteSecond % secondsPerDay);
            }
            else
            {
                tradingDay = (absoluteSecond - secondsPerDay + 1) / secondsPerDay;
                secondOfDay = static_cast<int>(absoluteSecond - tradingDay * secondsPerDay);
            }

            const QDate date = detail::tradingDayToDate(tradingDay);
            const int hour = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour() + secondOfDay / 3600;
            const int minute = (secondOfDay % 3600) / 60;
            const int second = secondOfDay % 60;

            return QDateTime(date, QTime(hour, minute, second), TradingHours::MARKET_TIMEZONE);
        }

        // Minute granularity (default)
        const qint64 absoluteMinute = toAbsoluteTradingMinute(p_index0Timestamp) + p_index;
        constexpr qint64 barsPerDay = BarsConstants::MINUTE_BARS_PER_DAY;

        qint64 tradingDay;
        int minuteOfDay;
        if (absoluteMinute >= 0)
        {
            tradingDay = absoluteMinute / barsPerDay;
            minuteOfDay = static_cast<int>(absoluteMinute % barsPerDay);
        }
        else
        {
            tradingDay = (absoluteMinute - barsPerDay + 1) / barsPerDay;
            minuteOfDay = static_cast<int>(absoluteMinute - tradingDay * barsPerDay);
        }

        const QDate date = detail::tradingDayToDate(tradingDay);
        const QTime time = BarsConstants::indexToTime(static_cast<size_t>(minuteOfDay));

        return QDateTime(date, time, TradingHours::MARKET_TIMEZONE);
    }

} // namespace ChartTimeUtils
