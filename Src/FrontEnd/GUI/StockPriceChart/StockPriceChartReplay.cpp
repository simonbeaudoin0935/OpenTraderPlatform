// Replay mode functionality for StockPriceChart
// Contains: onReplayDayChanged, onReplayTimeChanged,
// onReplayDataLoadFailed, setReplayModeActive,
// updateCurrentTimeLine, updateReplayDayBoundaryLines

#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "IndexToTimeTicker.h"
#include "Misc/Settings.h"
#include "Logging.h"
#include "Assume.h"
#include "BarCache.h"
#include "MainApp.h"
#include "DBClient.h"
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

    // Check if replay data exists for this symbol on the selected date
    bool hasData = DBClient::getInstance()->hasReplayData(date, m_symbol);

    if (hasData)
    {
        chartToolbar->updateReplayInfo(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                       TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                       BarsConstants::MINUTE_BARS_PER_DAY);
    }
    else
    {
        WARNING << "No replay data found for" << m_symbol << "on" << date.toString(Qt::ISODate);
        chartToolbar->updateReplayInfo(QTime(), QTime(), 0);
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
}

/**
 * @brief Handles replay start time changes.
 *
 * When user changes the start time in paused state, clears the chart,
 * updates the replay time anchor to the new time, and triggers a chart
 * preload so all historical bars up to the new start are loaded.
 * The X/Y view range is preserved: X is shifted by the anchor delta so
 * the same bars remain in view, Y is restored unchanged.
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

        // Save current view ranges before clearing so we can restore them.
        // The X range must be shifted by the anchor delta (old start → new start in minutes)
        // so the same bars stay in view after the anchor moves.
        // The Y range is saved in m_preservedYRange so the initial-range logic in
        // onHistoricalBarsReceived restores it instead of auto-computing from bars.
        const QCPRange savedXRange = m_customPlot->xAxis->range();
        if (m_initialYAxisRangeSet)
        {
            m_preservedYRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
        }
        const int deltaMinutes = m_index0Timestamp.isValid()
            ? static_cast<int>(QDateTime(currentDate, m_index0Timestamp.time(), TradingHours::MARKET_TIMEZONE)
                                   .secsTo(QDateTime(currentDate, time, TradingHours::MARKET_TIMEZONE)) / 60)
            : 0;

        // Update the global replay time anchor BEFORE clearing the chart so that
        // initializeTimeAnchor() (called inside clearChart) uses the new start time
        // and checkForMissingBars loads all historical bars up to the new position.
        MainApp::currentAppReplayTime =
            QDateTime(currentDate, time, TradingHours::MARKET_TIMEZONE);

        clearChart();

        // Restore the X view range, shifting by the anchor delta.
        // Y range is restored in onHistoricalBarsReceived via m_preservedYRange.
        m_customPlot->xAxis->setRange(savedXRange.lower - deltaMinutes, savedXRange.upper - deltaMinutes);
        m_customPlot->replot();

        MainApp::getInstance()->preloadChartForReplay(currentDate, time, currentSpeed);
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
