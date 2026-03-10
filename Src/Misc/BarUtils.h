#pragma once

#include <QDateTime>
#include <QVector>

#include "Assume.h"
#include "Bar.h"
#include "CONSTANTS.h"
#include "TimeFrame.h"

/**
 * @file BarUtils.h
 * @brief Helper functions for multi-timescale bar arithmetic.
 *
 * Provides utilities for converting between TimeFrame values and bar indices,
 * determining the number of bars per trading day, and identifying the aggregation
 * source timescale for non-natively-supported intervals.
 *
 * Databento only provides native OHLCV schemas for 1m, 1h, and 1d.
 * All other timescales (5m, 15m, 30m, 4h, 1w, 1M) are derived in-app by
 * aggregating finer-grained bars fetched from Databento.
 *
 * Aggregation chain:
 *   Ohlcv1M  →  aggregate in-app  →  5m, 15m, 30m
 *   Ohlcv1H  →  aggregate in-app  →  4h
 *   Ohlcv1D  →  aggregate in-app  →  1w, 1M
 */
namespace BarUtils
{

    /**
 * @brief SQLite cache schema version for the multi-timescale bar table.
 *
 * Bump this value to force all per-symbol cache databases to be dropped and
 * rebuilt on next launch. The DatabaseThread checks this on DB open.
 * Since the BarCache is a rebuildable cache (Databento is the source of truth),
 * no migration is required — the cache is simply regenerated.
 */
    inline constexpr int BARCACHE_SCHEMA_VERSION = 2; // v1 = 1m-only, v2 = multi-timescale

    /**
 * @brief Returns the number of minutes per bar for a given timescale.
 *
 * The TimeFrame enum values are defined in minutes, so this is a direct cast.
 *
 * @param p_tf The timescale
 * @return Number of minutes per bar
 */
    [[nodiscard]] inline constexpr int minutesPerBar(TimeFrame p_tf)
    {
        return static_cast<int>(p_tf);
    }

    /**
 * @brief Returns true if the timescale covers only part of a single trading day.
 *
 * Intraday timescales (1m, 5m, 15m, 30m, 1h, 4h) are stored day-by-day with
 * multiple bars per day. Daily+ timescales (1d, 1w, 1M) use a single-bar-per-day
 * keying scheme and may span multiple calendar days.
 *
 * @param p_tf The timescale to test
 * @return true for 1m, 5m, 15m, 30m, 1h, 4h; false for 1d, 1w, 1M
 */
    [[nodiscard]] inline constexpr bool isIntradayTimeFrame(TimeFrame p_tf)
    {
        return static_cast<int>(p_tf) < static_cast<int>(TimeFrame::ONE_DAY);
    }

    /**
 * @brief Returns true if Databento has a native OHLCV schema for this timescale.
 *
 * Native schemas: 1m (Ohlcv1M), 1h (Ohlcv1H), 1d (Ohlcv1D).
 * All others must be derived by aggregating a finer native timescale.
 *
 * @param p_tf The timescale to test
 * @return true if Databento natively supports this timescale
 */
    [[nodiscard]] inline constexpr bool isNativeTimeFrame(TimeFrame p_tf)
    {
        return p_tf == TimeFrame::ONE_MINUTE || p_tf == TimeFrame::ONE_HOUR || p_tf == TimeFrame::ONE_DAY;
    }

    /**
 * @brief Returns the source timescale to aggregate from when building bars for @p p_tf.
 *
 * For native timescales, returns the timescale itself (no aggregation needed).
 * For derived timescales, returns the finest native timescale to fetch first:
 *
 *   5m / 15m / 30m  →  ONE_MINUTE  (fetch 1m bars, then aggregate)
 *   4h              →  ONE_HOUR    (fetch 1h bars, then aggregate)
 *   1w / 1M         →  ONE_DAY     (fetch 1d bars, then aggregate)
 *
 * @param p_tf The target timescale
 * @return The native source timescale to use for fetching from Databento
 */
    [[nodiscard]] inline constexpr TimeFrame aggregateSourceTimeFrame(TimeFrame p_tf)
    {
        if (isNativeTimeFrame(p_tf))
            return p_tf;

        if (p_tf == TimeFrame::FIVE_MINUTES || p_tf == TimeFrame::FIFTEEN_MINUTES || p_tf == TimeFrame::THIRTY_MINUTES)
            return TimeFrame::ONE_MINUTE;

        if (p_tf == TimeFrame::FOUR_HOURS)
            return TimeFrame::ONE_HOUR;

        // ONE_WEEK or ONE_MONTH
        return TimeFrame::ONE_DAY;
    }

    /**
 * @brief Returns the number of bars per trading day for intraday timescales.
 *
 * The L2Trader trading day spans 4:00 AM – 6:59 PM ET (900 minutes).
 * For daily+ timescales (1d, 1w, 1M) there is exactly 1 bar per cache entry.
 *
 * @param p_tf The timescale
 * @return Number of bars stored per trading day (always ≥ 1)
 */
    [[nodiscard]] inline constexpr int barsPerDay(TimeFrame p_tf)
    {
        if (!isIntradayTimeFrame(p_tf))
            return 1;

        const int totalMinutes = static_cast<int>(BarsConstants::MINUTE_BARS_PER_DAY);
        const int interval = minutesPerBar(p_tf);
        ASSUME_GT(interval, 0);
        // Ceiling division: include the final partial bar when the session duration
        // is not an exact multiple of the interval (e.g., 900 min / 240 min → 4, not 3).
        return (totalMinutes + interval - 1) / interval;
    }

    /**
 * @brief Converts a bar open-time to its zero-based index within the trading day.
 *
 * Uses the open-time convention (Databento standard): the bar is identified by
 * the minute it opens. Index 0 = 4:00 AM bar.
 *
 * For intraday timescales the formula is:
 *   index = floor((hour - 4) * 60 + minute) / minutesPerBar(tf)
 *
 * For daily+ timescales (1d, 1w, 1M) this always returns 0, since the entire
 * "day" is a single bar and the QDate key is the canonical identifier.
 *
 * @param p_tf   The timescale
 * @param p_time Bar open time (America/New_York, must be within 4:00 AM – 6:59 PM)
 * @return Zero-based bar index for this timescale
 *
 * @pre p_time is within the extended trading session (4:00 AM – 6:59 PM ET)
 */
    [[nodiscard]] inline constexpr int barIndex(TimeFrame p_tf, const QTime& p_time)
    {
        if (!isIntradayTimeFrame(p_tf))
            return 0;

        ASSUME_GTE(p_time, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
        ASSUME_LTE(p_time, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

        const int minutesFromOpen =
            (p_time.hour() - TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour()) * 60 + p_time.minute();
        const int interval = minutesPerBar(p_tf);
        ASSUME_GT(interval, 0);

        const int index = minutesFromOpen / interval;
        ASSUME_GTE(index, 0);
        ASSUME_LT(index, barsPerDay(p_tf));
        return index;
    }

    /**
 * @brief Returns the bar open-time for a given index and intraday timescale.
 *
 * Inverse of barIndex(). Returns the QTime of the first minute covered by
 * the bar at @p p_index.
 *
 * For daily+ timescales always returns TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION
 * (4:00 AM) since there is only one bar per day.
 *
 * @param p_tf    The timescale (must be intraday for meaningful result)
 * @param p_index Zero-based bar index (0 to barsPerDay(tf)-1)
 * @return QTime of the bar's open time (4:00 AM – 6:59 PM ET)
 *
 * @pre p_index < barsPerDay(p_tf)
 */
    [[nodiscard]] inline constexpr QTime indexToBarTime(TimeFrame p_tf, int p_index)
    {
        ASSUME_GTE(p_index, 0);
        ASSUME_LT(p_index, barsPerDay(p_tf));

        const int minuteOffset =
            p_index * minutesPerBar(p_tf) + TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.hour() * 60;

        const int hour = minuteOffset / 60;
        const int minute = minuteOffset % 60;
        return QTime(hour, minute, 0);
    }

    /**
 * @brief Compute the open-time of the target bar that a source bar belongs to.
 *
 * For intraday timescales, floors the source bar's time to the nearest
 * target-TF boundary. For weekly/monthly, floors to Monday / month start.
 *
 * @param p_srcTs  Timestamp of the source (finer) bar (America/New_York)
 * @param p_targetTf  The coarser target timescale
 * @return QDateTime of the target bar's open (same timezone as p_srcTs)
 */
    [[nodiscard]] inline QDateTime targetBarOpen(const QDateTime& p_srcTs, TimeFrame p_targetTf)
    {
        const QDate date = p_srcTs.date();
        const QTime time = p_srcTs.time();

        if (isIntradayTimeFrame(p_targetTf))
        {
            const int minFromOpen = (time.hour() - 4) * 60 + time.minute();
            const int targetMinutes = minutesPerBar(p_targetTf);
            const int groupStart = (minFromOpen / targetMinutes) * targetMinutes;
            return QDateTime(date, QTime(4 + groupStart / 60, groupStart % 60, 0, 0), p_srcTs.timeZone());
        }
        if (p_targetTf == TimeFrame::ONE_WEEK)
        {
            QDate monday = date.addDays(-(date.dayOfWeek() - 1));
            return QDateTime(monday, QTime(4, 0, 0, 0), p_srcTs.timeZone());
        }
        if (p_targetTf == TimeFrame::ONE_MONTH)
        {
            return QDateTime(QDate(date.year(), date.month(), 1), QTime(4, 0, 0, 0), p_srcTs.timeZone());
        }
        // ONE_DAY
        return QDateTime(date, QTime(4, 0, 0, 0), p_srcTs.timeZone());
    }

    /**
 * @brief Aggregate finer-grained source bars into coarser target bars.
 *
 * Null and Uninitialized bars are skipped so they don't contaminate OHLCV values.
 * The output bars use open-time convention and carry Bar::BarStatus::Closed.
 *
 * @param p_sourceBars  Bars at the source (finer) timescale, in chronological order
 * @param p_targetTf    The coarser target timescale to aggregate into
 * @return Aggregated bars in chronological order
 */
    [[nodiscard]] inline QVector<Bar> aggregateBars(const QVector<Bar>& p_sourceBars, TimeFrame p_targetTf)
    {
        QVector<Bar> result;
        if (p_sourceBars.isEmpty())
            return result;

        QDateTime groupOpenTs;
        double open = 0, high = 0, low = 0, close = 0;
        qint64 volume = 0;
        bool hasData = false;

        for (const Bar& bar: p_sourceBars)
        {
            if (bar.getBarStatus() == Bar::BarStatus::Null || bar.getBarStatus() == Bar::BarStatus::Uninitialized)
                continue;

            QDateTime grpOpen = targetBarOpen(bar.getTimeStamp(), p_targetTf);

            if (!hasData || grpOpen != groupOpenTs)
            {
                if (hasData)
                    result.append(Bar(groupOpenTs, open, high, low, close, volume));

                groupOpenTs = grpOpen;
                open = bar.getOpen();
                high = static_cast<double>(bar.getHigh());
                low = static_cast<double>(bar.getLow());
                close = bar.getClose();
                volume = bar.getTotalVolume();
                hasData = true;
            }
            else
            {
                high = std::max(high, static_cast<double>(bar.getHigh()));
                low = std::min(low, static_cast<double>(bar.getLow()));
                close = bar.getClose();
                volume += bar.getTotalVolume();
            }
        }

        if (hasData)
            result.append(Bar(groupOpenTs, open, high, low, close, volume));

        return result;
    }

} // namespace BarUtils