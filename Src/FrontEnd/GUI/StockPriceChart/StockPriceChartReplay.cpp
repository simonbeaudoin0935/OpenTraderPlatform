// Replay mode functionality for StockPriceChart
// Contains: onReplayDayChanged, onReplayTimeChanged, onReplayTimeRangeQueryFinished,
// onReplayDataLoadFailed, queryStockTimeRangeForDate, setReplayModeActive,
// updateCurrentTimeLine

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

void StockPriceChart::onReplayDayChanged(const QDate& date)
{
    if (m_symbol.isEmpty())
    {
        WARNING << "No symbol selected for replay day query";
        return;
    }

    // Cancel any ongoing query
    if (replayTimeRangeWatcher->isRunning())
    {
        Q_UNREACHABLE(); // TO_DELETE
        replayTimeRangeWatcher->cancel();
    }

    // If in replay mode, trigger preload with the new day and current time selection
    if (MainApp::isInReplayMode())
    {
        QTime currentTime = chartToolbar->getReplayStartTime();
        ReplayEngine::PlaybackSpeed currentSpeed = chartToolbar->getReplaySpeed();

        qCInfo(ChartLog) << "Preloading chart for new replay day:" << date.toString(Qt::ISODate) << "at"
                         << currentTime.toString("hh:mm");

        MainApp::getInstance()->preloadChartForReplay(date, currentTime, currentSpeed);
    }

    // Start asynchronous query for time range (for display info in toolbar)
    QFuture<std::tuple<QDateTime, QDateTime, int>> future =
        QtConcurrent::run([this, date]() { return queryStockTimeRangeForDate(m_symbol, date); });
    replayTimeRangeWatcher->setFuture(future);
}

/**
 * @brief Handles replay start time changes.
 *
 * When user changes the start time in paused state, triggers chart preload
 * with the new time but keeping the current day selected.
 */
void StockPriceChart::onReplayTimeChanged(const QTime& time)
{
    if (m_symbol.isEmpty())
    {
        WARNING << "No symbol selected for replay time change";
        return;
    }

    // If in replay mode, trigger preload with the current day and new time
    if (MainApp::isInReplayMode())
    {
        QDate currentDate = chartToolbar->getSelectedReplayDay();
        ReplayEngine::PlaybackSpeed currentSpeed = chartToolbar->getReplaySpeed();

        qCInfo(ChartLog) << "Preloading chart for new replay time:" << currentDate.toString(Qt::ISODate) << "at"
                         << time.toString("hh:mm");

        MainApp::getInstance()->preloadChartForReplay(currentDate, time, currentSpeed);
    }
}

/**
 * @brief Handles completion of replay time range query.
 */
void StockPriceChart::onReplayTimeRangeQueryFinished()
{
    if (replayTimeRangeWatcher->isCanceled())
    {
        return;
    }

    std::tuple<QDateTime, QDateTime, int> timeRangeResult = replayTimeRangeWatcher->result();

    if (std::get<0>(timeRangeResult).isValid() && std::get<1>(timeRangeResult).isValid())
    {
        qCInfo(ChartLog) << "Found replay time range for" << m_symbol << "from"
                         << std::get<0>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") << "to"
                         << std::get<1>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") << "(NY timezone) -"
                         << std::get<2>(timeRangeResult) << "bars available";

        // Cache the replay day start and end times
        m_replayDayStart = std::get<0>(timeRangeResult);
        m_replayDayEnd = std::get<1>(timeRangeResult);

        // Update the toolbar with the time range and bar count info
        QTime startTime = std::get<0>(timeRangeResult).time();
        QTime endTime = std::get<1>(timeRangeResult).time();
        int barCount = std::get<2>(timeRangeResult);
        chartToolbar->updateReplayInfo(startTime, endTime, barCount);

        // Pre-fill the time input widget with the earliest available time
        chartToolbar->setReplayStartTime(startTime);

        // Update boundary lines if we have bars in the chart
        updateReplayDayBoundaryLines();
    }
    else
    {
        WARNING << "No data found for" << m_symbol << "on selected date";

        // Clear cached times
        m_replayDayStart = QDateTime();
        m_replayDayEnd = QDateTime();

        // Clear the info label when no data is found
        chartToolbar->updateReplayInfo(QTime(), QTime(), 0);

        // Hide boundary lines
        m_replayStartLine->setVisible(false);
        m_replayEndLine->setVisible(false);
        m_replayStartLabel->setVisible(false);
        m_replayEndLabel->setVisible(false);
    }
}

/**
 * @brief Handles replay data loading failure (database not found, corrupted, etc.)
 *
 * Shows error message to user and reverts to previous valid state.
 */
void StockPriceChart::onReplayDataLoadFailed(const QString& errorMessage)
{
    WARNING << "Replay data load failed:" << errorMessage;

    // Update toolbar info display to show failure
    chartToolbar->updateReplayInfo(QTime(), QTime(), 0);

    // Log detailed error
    qCCritical(ChartLog) << "Failed to preload replay data:" << errorMessage;
}

/**
 * @brief Queries the database for the first and last timestamps of a stock on a specific date.
 * @param symbol The stock symbol to query
 * @param date The date to query (in NY timezone)
 * @return A tuple of QDateTime objects representing the first and last timestamps (in NY timezone) and the bar count
 */
std::tuple<QDateTime, QDateTime, int> StockPriceChart::queryStockTimeRangeForDate(const QString& symbol,
                                                                                  const QDate& date)
{
    std::tuple<QDateTime, QDateTime, int> result;

    // Build database path: ~/.cache/L2Trader/RecordedLiveData/Bars/YYYY-MM-DD.db
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString dbPath = QString("%1/RecordedLiveData/Bars/%2.db").arg(cacheDir, date.toString("yyyy-MM-dd"));

    if (!QFile::exists(dbPath))
    {
        WARNING << "Database file does not exist:" << dbPath;
        return result;
    }

    // Use a scoped block to ensure QSqlQuery goes out of scope before removeDatabase
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "replay_query");
        db.setDatabaseName(dbPath);

        if (!db.open())
        {
            WARNING << "Failed to open database:" << db.lastError().text();
            QSqlDatabase::removeDatabase("replay_query");
            return result;
        }

        // Calculate epoch range for the date (start of day to end of day in NY timezone)
        QTimeZone nyZone("America/New_York");
        QDateTime dayStart(date, QTime(0, 0, 0), nyZone);
        QDateTime dayEnd(date, QTime(23, 59, 59, 999), nyZone);
        qint64 startEpochMs = dayStart.toMSecsSinceEpoch();
        qint64 endEpochMs = dayEnd.toMSecsSinceEpoch();

        // Query for min, max timestamps and count for the symbol
        QSqlQuery query(db);
        query.prepare(StockPriceChartQueries::SELECT_STOCK_TIME_RANGE);
        query.addBindValue(symbol);
        query.addBindValue(startEpochMs);
        query.addBindValue(endEpochMs);

        if (query.exec() && query.next())
        {
            qint64 minEpochMs = query.value(0).toLongLong();
            qint64 maxEpochMs = query.value(1).toLongLong();
            int barCount = query.value(2).toInt();

            if (minEpochMs > 0 && maxEpochMs > 0)
            {
                // Convert from UTC to New York timezone
                std::get<0>(result) = QDateTime::fromMSecsSinceEpoch(minEpochMs, Qt::UTC).toTimeZone(nyZone);
                std::get<1>(result) = QDateTime::fromMSecsSinceEpoch(maxEpochMs, Qt::UTC).toTimeZone(nyZone);
                std::get<2>(result) = barCount;

                qCInfo(ChartLog) << "Database query result for" << symbol << "on" << date.toString("yyyy-MM-dd") << ":"
                                 << barCount << "bars found";
            }
            else
            {
                qCInfo(ChartLog) << "No bars found for" << symbol << "on" << date.toString("yyyy-MM-dd");
            }
        }
        else
        {
            WARNING << "Query failed:" << query.lastError().text();
        }

        db.close();
    } // QSqlQuery and QSqlDatabase go out of scope here

    QSqlDatabase::removeDatabase("replay_query");

    return result;
}

void StockPriceChart::setReplayModeActive(bool active)
{
    if (m_isReplayModeActive == active)
    {
        return;
    }

    m_isReplayModeActive = active;

    // Update background color
    QColor bgColor = active ? REPLAY_BACKGROUND_COLOR : NORMAL_BACKGROUND_COLOR;
    m_customPlot->setBackground(QBrush(bgColor));
    m_volumeAxisRect->setBackground(QBrush(bgColor));

    // Show/hide replay boundary lines based on mode
    if (active && m_replayDayStart.isValid() && m_replayDayEnd.isValid())
    {
        // Show boundary lines when entering replay mode if we have valid data
        updateReplayDayBoundaryLines();
    }
    else
    {
        // Hide boundary lines when exiting replay mode
        m_replayStartLine->setVisible(false);
        m_replayEndLine->setVisible(false);
        m_replayStartLabel->setVisible(false);
        m_replayEndLabel->setVisible(false);
    }

    m_customPlot->replot();

    qCInfo(ChartLog) << "Replay mode visual" << (active ? "activated" : "deactivated");
}

/**
 * @brief Updates the position of the current time vertical line.
 *
 * This slot is called every second to move the vertical white line to the current time position.
 * The line is positioned based on the fractional index calculated from the current time.
 * Each minute corresponds to 1 index unit, so each second moves the line by 1/60 of an index.
 *
 * Note: Databento open-time convention — bars are timestamped at their opening time.
 * For example, a bar covering 4:00:00-4:00:59 has timestamp 4:00:00.
 * m_index0Timestamp is already the bar open time at index 0.
 *
 * The line stops advancing after market close (6:59 PM) and resumes at market open (4:00 AM).
 */
void StockPriceChart::updateCurrentTimeLine()
{
    if (!m_index0Timestamp.isValid())
    {
        return;
    }

    // Get the current application time (NY timezone)
    QDateTime currentTime = MainApp::getCurrentAppTime();

    // Compute fractional chart index for current time
    // m_index0Timestamp is already bar open time
    qint64 secondsDiff = m_index0Timestamp.secsTo(currentTime);

    // Clamp to trading hours
    QTime currentTimeOfDay = currentTime.time();
    QTime marketOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION;
    QTime marketClose = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

    if (currentTimeOfDay < marketOpen)
    {
        QDateTime marketOpenTime(currentTime.date(), marketOpen, TradingHours::MARKET_TIMEZONE);
        secondsDiff = m_index0Timestamp.secsTo(marketOpenTime);
    }
    else if (currentTimeOfDay > marketClose)
    {
        QDateTime marketCloseTime(currentTime.date(), marketClose, TradingHours::MARKET_TIMEZONE);
        secondsDiff = m_index0Timestamp.secsTo(marketCloseTime);
    }

    // Convert to fractional index position
    // Each minute is 1 index unit, so each second is 1/60.0 of an index
    double currentIndex = secondsDiff / 60.0;

    // There is a particularity with how the index and bar printing works;
    // a bar is placed at an index, but half of the bar is before and half after the index.
    // To center the line within the current minute, we subtract 0.5
    currentIndex -= 0.5; // Center the line within the current minute

    // Get the current Y-axis range for the line
    QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();

    // Position the vertical line at the calculated index
    m_currentTimeLine->start->setCoords(currentIndex, yRange.lower);
    m_currentTimeLine->end->setCoords(currentIndex, yRange.upper);

    // Use queued replot for better performance - allows batching multiple updates
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

/**
 * @brief Updates the positions of the replay day boundary lines.
 *
 * This method positions the blue start line and red end line to mark the temporal boundaries
 * of the selected replay day. The lines are only visible in replay mode and span the full
 * height of the chart.
 *
 * The positioning logic is similar to updateCurrentTimeLine(), converting QDateTime timestamps
 * to fractional chart index positions based on the first bar (index 0).
 */
void StockPriceChart::updateReplayDayBoundaryLines()
{
    // Don't update if we don't have valid replay day timestamps
    if (!m_replayDayStart.isValid() || !m_replayDayEnd.isValid())
    {
        m_replayStartLine->setVisible(false);
        m_replayEndLine->setVisible(false);
        m_replayStartLabel->setVisible(false);
        m_replayEndLabel->setVisible(false);
        return;
    }

    // Need time anchor to calculate positions
    if (!m_index0Timestamp.isValid())
    {
        DEBUG << "Cannot update replay boundary lines: no time anchor set";
        return;
    }

    // m_index0Timestamp is already bar open time
    QDateTime zeroIndexTime = m_index0Timestamp;

    // Calculate index positions for start and end times
    qint64 startSecondsDiff = zeroIndexTime.secsTo(m_replayDayStart);
    qint64 endSecondsDiff = zeroIndexTime.secsTo(m_replayDayEnd);

    // Convert to fractional index (each minute = 1 index, each second = 1/60.0 index)
    double startIndex = startSecondsDiff / 60.0 - 0.5; // Center within the minute
    double endIndex = endSecondsDiff / 60.0 - 0.5;

    // Get the current Y-axis range
    QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();

    // Position the start line (blue)
    m_replayStartLine->start->setCoords(startIndex, yRange.lower);
    m_replayStartLine->end->setCoords(startIndex, yRange.upper);

    // Position the end line (red)
    m_replayEndLine->start->setCoords(endIndex, yRange.lower);
    m_replayEndLine->end->setCoords(endIndex, yRange.upper);

    // Position labels at the top of the chart
    m_replayStartLabel->position->setCoords(startIndex, yRange.upper);
    m_replayStartLabel->setText(m_replayDayStart.time().toString("hh:mm"));

    m_replayEndLabel->position->setCoords(endIndex, yRange.upper);
    m_replayEndLabel->setText(m_replayDayEnd.time().toString("hh:mm"));

    // Show the lines and labels if in replay mode
    if (m_isReplayModeActive)
    {
        m_replayStartLine->setVisible(true);
        m_replayEndLine->setVisible(true);
        m_replayStartLabel->setVisible(true);
        m_replayEndLabel->setVisible(true);
    }

    DEBUG << "Updated replay boundary lines: start at" << m_replayDayStart.toString("hh:mm") << "(index" << startIndex
          << "), end at" << m_replayDayEnd.toString("hh:mm") << "(index" << endIndex << ")";

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}
