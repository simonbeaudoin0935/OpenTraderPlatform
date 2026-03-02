// Time/index utility functions for StockPriceChart
// Contains: getPreviousTradingMinute, adjustToValidTradingTime, getPreviousFriday,
// updateAxisLabelsDensity, addHistoricalBarsToIndexMapping, getTimestampForIndex,
// getIndexForTimestamp, indexToTimeString

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "IndexToTimeTicker.h"
#include "Misc/Settings.h"
#include "Logging.h"
#include "Assume.h"
#include "SQL/StockPriceChartQueries.h"
#include "BarCache.h"
#include "MainApp.h"
#include "Order.h"
#include "Position.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"
#define LOGGING_CATEGORY ChartLog

QDateTime StockPriceChart::getPreviousTradingMinute(const QDateTime& timestamp) const
{
    OBJ_ASSUME_TRUE(timestamp.timeZone() == TradingHours::MARKET_TIMEZONE);

    QDateTime previousMinute = timestamp.addSecs(-60);

    QTime time = previousMinute.time();
    int dayOfWeek = previousMinute.date().dayOfWeek();

    if (dayOfWeek >= TradingHours::MONDAY && dayOfWeek <= TradingHours::FRIDAY)
    {
        if (time < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
        {
            QDateTime result = QDateTime(previousMinute.date().addDays(-1),
                                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                         TradingHours::MARKET_TIMEZONE);
            if (result.date().dayOfWeek() > TradingHours::FRIDAY)
            {
                QDate friday = getPreviousFriday(result.date());
                result = QDateTime(friday,
                                   TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                   TradingHours::MARKET_TIMEZONE);
            }
            return result.toTimeZone(timestamp.timeZone());
        }
        else if (time > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
        {
            DEBUG << "Unexpected: getPreviousTradingMinute called with time after 8PM:" << time;
            return QDateTime(previousMinute.date(),
                             TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                             TradingHours::MARKET_TIMEZONE);
        }
    }
    else
    {
        QDate friday = getPreviousFriday(previousMinute.date());
        return QDateTime(friday, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
    }

    OBJ_ASSUME_GTE(previousMinute.time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(previousMinute.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    return previousMinute;
}

/**
 * @brief Adjusts a timestamp to the nearest valid trading time.
 */
QDateTime StockPriceChart::adjustToValidTradingTime(const QDateTime& timestamp) const
{
    OBJ_ASSUME_TRUE(timestamp.timeZone() == TradingHours::MARKET_TIMEZONE);

    QTime time = timestamp.time();
    int dayOfWeek = timestamp.date().dayOfWeek();

    if (dayOfWeek > TradingHours::FRIDAY)
    {
        QDate friday = getPreviousFriday(timestamp.date());
        return QDateTime(friday, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
    }

    if (time < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        QDate previousDay = timestamp.date().addDays(-1);
        if (previousDay.dayOfWeek() > TradingHours::FRIDAY)
        {
            previousDay = getPreviousFriday(previousDay);
        }
        return QDateTime(previousDay,
                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                         TradingHours::MARKET_TIMEZONE);
    }
    else if (time >= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return QDateTime(timestamp.date(),
                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                         TradingHours::MARKET_TIMEZONE);
    }

    return timestamp;
}

/**
 * @brief Gets the previous Friday given a date.
 */
QDate StockPriceChart::getPreviousFriday(const QDate& date) const
{
    int dayOfWeek = date.dayOfWeek();
    if (dayOfWeek == TradingHours::FRIDAY)
    {
        return date;
    }
    else if (dayOfWeek > TradingHours::FRIDAY)
    {
        return date.addDays(-(dayOfWeek - TradingHours::FRIDAY));
    }
    else
    {
        return date.addDays(-(dayOfWeek + 2));
    }
}

/**
 * @brief Updates axis tick intervals dynamically based on screen density.
 */
void StockPriceChart::updateAxisLabelsDensity()
{
    // Simple implementation - qcustomplot handles most of this automatically
    // Can be enhanced later if needed
    m_customPlot->xAxis->setNumberFormat("g");
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setNumberFormat("f");
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setNumberPrecision(2);
}

/**
 * @brief Adds historical bars to the index mapping using time-anchored indices.
 * Each bar's index is computed from its timestamp relative to m_index0Timestamp,
 * so bars land at their correct temporal positions (gaps = empty indices).
 */
void StockPriceChart::addHistoricalBarsToIndexMapping(const std::shared_ptr<QVector<Bar>>& bars)
{
    OBJ_ASSUME_FALSE(bars->isEmpty());
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());

    DEBUG << "addHistoricalBarsToIndexMapping: adding" << bars->size() << "bars";

    for (const Bar& bar: *bars)
    {
        const QDateTime& timestamp = bar.getTimeStamp();
        const int index = ChartTimeUtils::timestampToChartIndex(timestamp, m_index0Timestamp);

        indexToBar[index] = bar;
        timestampToIndex[timestamp] = index;
    }

    DEBUG << "addHistoricalBarsToIndexMapping: completed, index range [" << indexToBar.firstKey() << ","
          << indexToBar.lastKey() << "]";
}

/**
 * @brief Gets the timestamp corresponding to a chart index.
 * Checks indexToBar first, then falls back to ChartTimeUtils for computed positions.
 */
QDateTime StockPriceChart::getTimestampForIndex(int index) const
{
    // Fast path: bar exists at this index
    auto it = indexToBar.find(index);
    if (it != indexToBar.end())
    {
        return it.value().getTimeStamp();
    }

    // Compute from time anchor (works for any index, even without bars loaded)
    if (m_index0Timestamp.isValid())
    {
        return ChartTimeUtils::chartIndexToTimestamp(index, m_index0Timestamp);
    }

    // No time anchor yet (chart not initialized) — return invalid
    return QDateTime();
}

/**
 * @brief Gets the chart index corresponding to a timestamp.
 * Checks timestampToIndex first, then falls back to ChartTimeUtils.
 */
int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const
{
    // Fast path: exact match in map
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end())
    {
        return it.value();
    }

    // Compute from time anchor
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());
    return ChartTimeUtils::timestampToChartIndex(timestamp, m_index0Timestamp);
}


/**
 * @brief Converts an index to a time string for axis labels.
 */
QString StockPriceChart::indexToTimeString(double index) const
{
    QDateTime timestamp = getTimestampForIndex(static_cast<int>(index));
    return timestamp.toString("hh:mm");
}
