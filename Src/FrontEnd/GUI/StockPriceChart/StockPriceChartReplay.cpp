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
        // Full day available: 4:00 AM to 18:59 (900 bars)
        QTimeZone nyZone = TradingHours::MARKET_TIMEZONE;
        m_replayDayStart = QDateTime(date, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION, nyZone);
        m_replayDayEnd = QDateTime(date, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, nyZone);

        chartToolbar->updateReplayInfo(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                       TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                       BarsConstants::MINUTE_BARS_PER_DAY);

        updateReplayDayBoundaryLines();
    }
    else
    {
        WARNING << "No replay data found for" << m_symbol << "on" << date.toString(Qt::ISODate);

        m_replayDayStart = QDateTime();
        m_replayDayEnd = QDateTime();

        chartToolbar->updateReplayInfo(QTime(), QTime(), 0);

        m_replayStartLine->setVisible(false);
        m_replayEndLine->setVisible(false);
        m_replayStartLabel->setVisible(false);
        m_replayEndLabel->setVisible(false);
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
