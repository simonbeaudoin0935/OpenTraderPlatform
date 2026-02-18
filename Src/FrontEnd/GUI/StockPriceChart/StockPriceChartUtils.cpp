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
 * @brief Adds historical bars to the index mapping using negative indices.
 */
void StockPriceChart::addHistoricalBarsToIndexMapping(const std::shared_ptr<QVector<Bar>>& bars)
{
    OBJ_ASSUME_FALSE(bars->isEmpty());
    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    int minIndex = indexToBar.firstKey();

    DEBUG << "addHistoricalBarsToIndexMapping: adding" << bars->size() << "bars, starting minIndex:" << minIndex;

    for (auto it = bars->rbegin(); it != bars->rend(); ++it)
    {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();

        //DEBUG << "Received historical bar" << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
        //      << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "isEndOfHistory:" << bar.getIsEndOfHistory()
        //      << "O:" << bar.getOpen() << "H:" << bar.getHigh() << "L:" << bar.getLow() << "C:" << bar.getClose();

        //OBJ_ASSUME_TRUE(timestampToIndex.contains(timestamp));

        --minIndex;
        indexToBar[minIndex] = bar;
        timestampToIndex[timestamp] = minIndex;

        if (it - bars->rbegin() >= bars->size() - 3 || minIndex >= -3)
        {
            DEBUG << "  Assigned index" << minIndex << "to timestamp" << timestamp.toString("hh:mm:ss");
        }
    }

    DEBUG << "addHistoricalBarsToIndexMapping: completed, new minIndex:" << minIndex;
}

/**
 * @brief Gets the timestamp corresponding to an index.
 */
QDateTime StockPriceChart::getTimestampForIndex(int index) const
{
    auto it = indexToBar.find(index);
    if (it != indexToBar.end())
    {
        return it.value().getTimeStamp();
    }

    // If indexToBar is empty (e.g., after clearSymbol()), return invalid QDateTime
    // The IndexToTimeTicker will handle this gracefully by displaying the index as a number
    if (indexToBar.isEmpty())
    {
        return QDateTime();
    }

    if (index < 0)
    {
        int firstIndex = indexToBar.firstKey();
        QDateTime currentTime = indexToBar.first().getTimeStamp();
        int deltaIndex = firstIndex - index;

        for (int i = 0; i < deltaIndex; ++i)
        {
            currentTime = getPreviousTradingMinute(currentTime);
        }

        return currentTime;
    }

    int lastIndex = indexToBar.lastKey();
    if (index > lastIndex)
    {
        QDateTime lastTime = indexToBar.last().getTimeStamp();
        int deltaIndex = index - lastIndex;
        return lastTime.addSecs(deltaIndex * 60);
    }

    return QDateTime::currentDateTime();
}

/**
 * @brief Gets the index corresponding to a timestamp.
 */
int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const
{
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end())
    {
        return it.value();
    }

    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    QDateTime firstTime = indexToBar.first().getTimeStamp();
    int firstIndex = indexToBar.firstKey();

    if (timestamp < firstTime)
    {
        // Calculate minutes before first bar
        qint64 minutesDiff = firstTime.toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
        OBJ_ASSUME_EQUAL(minutesDiff % 60, 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return firstIndex - indexDiff;
    }

    QDateTime lastTime = indexToBar.last().getTimeStamp();
    int lastIndex = indexToBar.lastKey();

    if (timestamp > lastTime)
    {
        // Calculate minutes after last bar
        qint64 minutesDiff = timestamp.toSecsSinceEpoch() - lastTime.toSecsSinceEpoch();
        OBJ_ASSUME_EQUAL(minutesDiff % 60, 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return lastIndex + indexDiff;
    }

    // Timestamp is between existing bars - interpolate
    auto lowerIt = timestampToIndex.lowerBound(timestamp);
    if (lowerIt == timestampToIndex.begin())
    {
        // Should not happen since we checked < firstTime
        return firstIndex;
    }
    else if (lowerIt == timestampToIndex.end())
    {
        // Should not happen since we checked > lastTime
        return lastIndex;
    }
    else
    {
        // Interpolate between prev and next
        auto prevIt = std::prev(lowerIt);
        QDateTime prevTime = prevIt.key();
        QDateTime nextTime = lowerIt.key();
        int prevIndex = prevIt.value();
        int nextIndex = lowerIt.value();

        qint64 totalSeconds = prevTime.secsTo(nextTime);
        qint64 secondsFromPrev = prevTime.secsTo(timestamp);

        if (totalSeconds == 0)
            return prevIndex;

        double fraction = static_cast<double>(secondsFromPrev) / totalSeconds;
        return prevIndex + static_cast<int>((nextIndex - prevIndex) * fraction);
    }
}


/**
 * @brief Converts an index to a time string for axis labels.
 */
QString StockPriceChart::indexToTimeString(double index) const
{
    QDateTime timestamp = getTimestampForIndex(static_cast<int>(index));
    return timestamp.toString("hh:mm");
}
