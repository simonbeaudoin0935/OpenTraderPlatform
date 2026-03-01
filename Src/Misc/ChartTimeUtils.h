#pragma once

#include <QDateTime>

#include "CONSTANTS.h"

/**
 * @namespace ChartTimeUtils
 * @brief Utilities for time-anchored chart indexing
 *
 * Provides functions to:
 * 1. Compute the "index 0" bar timestamp from the current time
 * 2. Convert between bar timestamps and chart indices
 *
 * Chart indices are based on "trading minutes" — continuous counting that
 * skips overnight gaps and weekends. This means:
 * - Friday 20:00 bar is immediately adjacent to Monday 4:01 bar
 * - Intra-day gaps (sparse stocks) show as missing indices
 * - Index 0 is always "now" (or replay start time)
 */
namespace ChartTimeUtils
{

    /**
 * @brief Compute the index 0 bar timestamp for chart time-anchoring.
 *
 * Returns the bar close timestamp that should be at chart index 0.
 * During market hours, this is the current minute's bar close time.
 * Outside market hours, it's the last tradable bar (20:00) of the most
 * recent trading day.
 *
 * @param p_currentTime Current time (live clock or replay time)
 * @return QDateTime The bar timestamp for index 0 (always a valid bar close time on a weekday)
 */
    inline QDateTime computeIndex0Timestamp(const QDateTime& p_currentTime)
    {
        const QDateTime mt = p_currentTime.toTimeZone(TradingHours::MARKET_TIMEZONE);
        const QTime time = mt.time();
        const QDate date = mt.date();
        const int dow = date.dayOfWeek();
        const bool isWeekday = (dow >= TradingHours::MONDAY && dow <= TradingHours::FRIDAY);

        // During market hours on a weekday: bar close = floor(now) + 1 minute
        if (isWeekday && time >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION &&
            time <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
        {
            QTime barClose(time.hour(), time.minute(), 0);
            barClose = barClose.addSecs(60);

            if (barClose > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
                barClose = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

            return QDateTime(date, barClose, TradingHours::MARKET_TIMEZONE);
        }

        // Outside market hours: find most recent trading day's 20:00
        QDate tradingDate = date;

        // After 20:00 on a weekday → today's close
        if (isWeekday && time > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
            return QDateTime(tradingDate,
                             TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                             TradingHours::MARKET_TIMEZONE);

        // Before 4:01 on a weekday → previous day
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
 * @brief Convert a bar timestamp to an absolute "trading minute" index.
 *
 * Trading minutes are a continuous count that skips weekends and overnight gaps.
 * Each trading day contributes 960 minutes (4:01 to 20:00).
 *
 * @param p_barTimestamp A valid bar close timestamp (4:01-20:00 on a weekday)
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
 * @param p_barTimestamp The bar's close timestamp
 * @param p_index0Timestamp The index 0 anchor timestamp (from computeIndex0Timestamp)
 * @return Chart index (negative = before index 0, positive = after)
 */
    inline int timestampToChartIndex(const QDateTime& p_barTimestamp, const QDateTime& p_index0Timestamp)
    {
        return static_cast<int>(toAbsoluteTradingMinute(p_barTimestamp) - toAbsoluteTradingMinute(p_index0Timestamp));
    }

    /**
 * @brief Convert a chart index to a bar timestamp, given the index 0 anchor.
 *
 * @param p_index Chart index
 * @param p_index0Timestamp The index 0 anchor timestamp
 * @return Bar close timestamp for the given index
 */
    inline QDateTime chartIndexToTimestamp(int p_index, const QDateTime& p_index0Timestamp)
    {
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
