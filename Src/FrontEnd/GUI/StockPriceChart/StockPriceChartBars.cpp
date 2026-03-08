// Bar management and drawing functionality for StockPriceChart
// Contains: addLiveBar, updateCandlestickData, updateVolumeData, rescaleVolumeAxisToVisibleRange,
// drawBackgroundsForReceivedBars, clearBackgroundRects, drawFixedBackgroundRect,
// onRequestedMissingBarsReceived, onRequestedMissingBarsFailed, redrawLastPriceLine,
// checkForMissingBars, clearSymbol, clearChart

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

void StockPriceChart::addLiveBar(const QString& symbol, const Bar& bar)
{
    // Its a bug if we receive a new bar for a different symbol than current
    OBJ_ASSUME_EQUAL(symbol, m_symbol);
    OBJ_ASSUME_TRUE(bar.isValid());
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());

    DEBUG << "Received bar for" << symbol << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
          << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "O:" << bar.getOpen() << "H:" << bar.getHigh()
          << "L:" << bar.getLow() << "C:" << bar.getClose();

    // Track when we transition from historical backfill to live bars
    if (startedReceivingRealtimeBars == false) [[unlikely]]
    {
        if (bar.getBarStatus() == Bar::BarStatus::Closed)
        {
            startedReceivingRealtimeBars = true;
        }
    }

    if (MainApp::isInReplayMode() == false)
    {
        OBJ_ASSUME_DIFF(bar.getBarStatus(), Bar::BarStatus::Uninitialized);
        OBJ_ASSUME_DIFF(bar.getBarStatus(), Bar::BarStatus::Null);
    }

    // Compute chart index from bar timestamp
    const int index = ChartTimeUtils::timestampToChartIndex(bar.getTimeStamp(), m_index0Timestamp);

    indexToBar[index] = bar;
    timestampToIndex[bar.getTimeStamp()] = index;
    m_latestBar = bar;
    m_latestBarIndex = index;

    // Incremental QCP data update — O(1) in the common case.
    // Closed bars already in the containers never change; only the live bar ticks.
    const Bar::BarStatus status = bar.getBarStatus();
    if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
    {
        const qint64 volume = bar.getTotalVolume();
        const bool barIsUp = bar.getClose() >= bar.getOpen();

        if (index == m_chartLiveBarIndex)
        {
            // --- Same live bar ticking: modify the last entry in-place (O(1)) ---

            // Candle: update OHLC directly via mutable iterator (key unchanged)
            {
                auto it = std::prev(m_candlesticks->data()->end());
                it->open = bar.getOpen();
                it->high = bar.getHigh();
                it->low = bar.getLow();
                it->close = bar.getClose();
            }

            // Volume: in-place if direction unchanged, re-route between series if flipped
            if (barIsUp == m_chartLiveBarIsUp)
            {
                // Same color bucket — just update the value in-place (O(1))
                auto& series = barIsUp ? m_volumePos : m_volumeNeg;
                auto it = std::prev(series->data()->end());
                it->value = static_cast<double>(volume);
            }
            else
            {
                // Bar flipped direction (rare) — removeAfter on the old series, add to new
                const double removeKey = static_cast<double>(index) - 0.5;
                (m_chartLiveBarIsUp ? m_volumePos : m_volumeNeg)->data()->removeAfter(removeKey);
                (barIsUp ? m_volumePos : m_volumeNeg)
                    ->data()
                    ->add({static_cast<double>(index), static_cast<double>(volume)});
                m_chartLiveBarIsUp = barIsUp;
            }
        }
        else
        {
            // --- New bar index: just append (O(1) since key > all existing keys) ---
            QCPFinancialData candleData;
            candleData.key = index;
            candleData.open = bar.getOpen();
            candleData.high = bar.getHigh();
            candleData.low = bar.getLow();
            candleData.close = bar.getClose();
            m_candlesticks->data()->add(candleData);

            (barIsUp ? m_volumePos : m_volumeNeg)
                ->data()
                ->add({static_cast<double>(index), static_cast<double>(volume)});

            m_chartLiveBarIndex = index;
            m_chartLiveBarIsUp = barIsUp;
        }

        rescaleVolumeAxisToVisibleRange();
    }

    if (!m_datesWithBackgrounds.contains(bar.getTimeStamp().date()))
    {
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});
    }

    redrawLastPriceLine();

    if (m_currentOpenPosition && !m_currentOpenPosition->isClosed)
    {
        double currentPrice = bar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    // Queue a replot — QCustomPlot coalesces multiple rpQueuedReplot calls so that
    // only one actual render happens per event-loop iteration regardless of trade rate.
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}


/**
 * @brief Updates the candlestick data from indexToBar map.
 */
void StockPriceChart::updateCandlestickData()
{
    // Convert our bar data to QCPFinancialData format
    QVector<QCPFinancialData> financialData;

    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();

        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            QCPFinancialData barData;
            barData.key = index; // Use index as the x-axis value
            barData.open = bar.getOpen();
            barData.high = bar.getHigh();
            barData.low = bar.getLow();
            barData.close = bar.getClose();
            financialData.append(std::move(barData));
        }
    }

    m_candlesticks->data()->set(financialData);
}

/**
 * @brief Updates the volume bar data from indexToBar map.
 */
void StockPriceChart::updateVolumeData()
{
    // Clear existing volume data
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();

    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();

        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            // Determine if bar is up or down based on close vs open
            bool isUp = bar.getClose() >= bar.getOpen();
            qint64 volume = bar.getTotalVolume();

            // Add to appropriate bar series
            if (isUp)
            {
                m_volumePos->addData(index, volume);
            }
            else
            {
                m_volumeNeg->addData(index, volume);
            }
        }
    }

    // Rescale volume axis to fit visible data
    rescaleVolumeAxisToVisibleRange();
}

/**
 * @brief Rescales the volume Y-axis based on the second highest volume of visible bars.
 *
 * Uses the second highest volume bar to determine the Y-axis range, which helps avoid
 * erratic single high-volume bars from skewing the scale. If there's only one bar with
 * volume, the highest is used instead.
 *
 * Only performs rescaling if m_volumeAutoRescaleEnabled is true.
 */
void StockPriceChart::rescaleVolumeAxisToVisibleRange()
{
    if (!m_volumeAutoRescaleEnabled)
    {
        return;
    }

    if (indexToBar.isEmpty())
    {
        return;
    }

    // Get the visible X range
    const QCPRange xRange = m_customPlot->xAxis->range();
    const int visibleStart = qMax(static_cast<int>(qFloor(xRange.lower)), indexToBar.firstKey());
    const int visibleEnd = qMin(static_cast<int>(qCeil(xRange.upper)), indexToBar.lastKey());

    // Track the two highest volumes
    qint64 highestVolume = 0;
    qint64 secondHighestVolume = 0;

    for (int i = visibleStart; i <= visibleEnd; ++i)
    {
        if (indexToBar.contains(i))
        {
            const Bar& bar = indexToBar[i];
            const Bar::BarStatus status = bar.getBarStatus();
            if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
            {
                const qint64 volume = static_cast<qint64>(bar.getTotalVolume());
                if (volume > highestVolume)
                {
                    secondHighestVolume = highestVolume;
                    highestVolume = volume;
                }
                else if (volume > secondHighestVolume)
                {
                    secondHighestVolume = volume;
                }
            }
        }
    }

    // Use second highest if available, otherwise fall back to highest
    const qint64 scaleVolume = (secondHighestVolume > 0) ? secondHighestVolume : highestVolume;

    if (scaleVolume > 0)
    {
        // Add small padding (5%) at the top for visual clarity
        const double upperBound = scaleVolume * 1.05;
        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, upperBound);
    }
}

/**
 * @brief Updates the background colors for different market sessions.
 *
 * Draws colored background rectangles to indicate:
 * - Pre-market hours (brownish/orange)
 * - After-hours (blueish/violet)
 *
 * Only draws backgrounds for the currently visible time range.
 */
/**
 * @brief Draws background rectangles for trading sessions based on received bars.
 *
 * This is called once when bars are received, creating fixed rectangles that
 * QCustomPlot will automatically clip to the visible range.
 *
 * @param bars The bars that were just received (typically a complete day's worth)
 */
void StockPriceChart::drawBackgroundsForReceivedBars(const QVector<Bar>& bars)
{
    OBJ_ASSUME_FALSE(bars.isEmpty());

    QSet<QDate> datesToDrawRectancles;

    // Go through all received bars to find unique dates
    for (const Bar& bar: bars)
    {
        OBJ_ASSUME_TRUE(bar.getTimeStamp().timeZone() == TradingHours::MARKET_TIMEZONE);

        datesToDrawRectancles.insert(bar.getTimeStamp().date());
    }

    // Draw full session rectangles only for dates that don't already have backgrounds
    for (const QDate& date: datesToDrawRectancles)
    {
        // Skip if we already have backgrounds for this date
        if (m_datesWithBackgrounds.contains(date))
        {
            continue;
        }

        // Draw early pre-market rectangle (4:00am - 5:59am ET) - paler orange
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 90),
                                m_earlyPreMarketRects);

        // Draw pre-market rectangle (6:00am - 9:29am ET)
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 180),
                                m_preMarketRects);

        // Draw after-hours rectangle (4:00pm - 6:59pm ET)
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                QColor(138, 43, 226, 180),
                                m_afterHoursRects);

        // Mark this date as having backgrounds
        m_datesWithBackgrounds.insert(date);

        DEBUG << "Created background rectangles for date" << date.toString();
    }
}

/**
 * @brief Clears all background rectangles from the chart.
 */
void StockPriceChart::clearBackgroundRects()
{
    // Delete and clear early pre-market rectangles
    for (auto rect: m_earlyPreMarketRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_earlyPreMarketRects.clear();

    // Delete and clear pre-market rectangles
    for (auto rect: m_preMarketRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_preMarketRects.clear();

    // Delete and clear after-hours rectangles
    for (auto rect: m_afterHoursRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_afterHoursRects.clear();

    // Clear the tracking set so backgrounds can be redrawn
    m_datesWithBackgrounds.clear();
}

/**
 * @brief Draws a fixed background rectangle for a specific time range.
 *
 * Creates a rectangle with fixed coordinates that spans the entire Y-axis.
 * QCustomPlot will automatically handle clipping to the visible range.
 *
 * @param rangeStart The start time of the range to highlight.
 * @param rangeEnd The end time of the range to highlight.
 * @param color The color for the background rectangle.
 * @param rectList The list to add the created rectangle to.
 */
void StockPriceChart::drawFixedBackgroundRect(const QDate& date,
                                              const QTime& rangeStart,
                                              const QTime& rangeEnd,
                                              const QColor& color,
                                              QList<QCPItemRect*>& rectList)
{
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());
    OBJ_ASSUME_TRUE(date.dayOfWeek() >= Qt::Monday && date.dayOfWeek() <= Qt::Friday);

    QDateTime rangeStartDT(date, rangeStart, TradingHours::MARKET_TIMEZONE);
    QDateTime rangeEndDT(date, rangeEnd, TradingHours::MARKET_TIMEZONE);

    // Calculate indices for the time boundaries
    qreal sessionStartIndex = static_cast<qreal>(getIndexForTimestamp(rangeStartDT));
    qreal sessionEndIndex = static_cast<qreal>(getIndexForTimestamp(rangeEndDT));

    sessionStartIndex -= 0.5; // Make start index inclusive of the first candle
    sessionEndIndex += 0.5;   // Make end index inclusive of the last candle

    // Create rectangle with fixed coordinates
    QCPItemRect* rect = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(rect);

    // Set the rectangle to use plot coordinates
    rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    rect->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));

    // Set fixed X coordinates (index range) and full Y range
    // Y coordinates will automatically adapt to axis range changes
    rect->topLeft->setCoords(sessionStartIndex, 0);
    rect->bottomRight->setCoords(sessionEndIndex, 0);

    // Use axis rect ratio for Y to span full height
    rect->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->topLeft->setCoords(sessionStartIndex,
                             0); // 0 = top of axis rect
    rect->bottomRight->setCoords(sessionEndIndex,
                                 1); // 1 = bottom of axis rect

    rect->setPen(Qt::NoPen);
    rect->setBrush(QBrush(color));

    // Set layer to ensure rectangles are behind the data
    rect->setLayer("background");

    DEBUG << "Created fixed background rect from index" << sessionStartIndex << "to" << sessionEndIndex << "for time"
          << rangeStart.toString("hh:mm") << "-" << rangeEnd.toString("hh:mm") << "with color" << color.name();

    rectList.append(rect);
}

/**
 * @brief Handles the response to a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsReceived(const std::shared_ptr<QVector<Bar>>& barsPtr)
{

    DEBUG << "Received missing bars response with" << barsPtr->size() << "bars";

    // Sanity check: semaphore should be acquired (count == 0) when we receive the response
    OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 0);
    m_missingBarsRequestSemaphore.release();

    OBJ_ASSUME_FALSE(barsPtr->isEmpty());

    addHistoricalBarsToIndexMapping(barsPtr);

    DEBUG << "After addHistoricalBarsToIndexMapping, index range:"
          << QString("%1 to %2").arg(indexToBar.firstKey()).arg(indexToBar.lastKey());

    // Draw background rectangles for the visible range
    drawBackgroundsForReceivedBars(*barsPtr);

    // Only set initial Y-axis range on the first batch of historical bars
    if (!m_initialYAxisRangeSet)
    {
        if (m_preservedYRange.has_value())
        {
            // User had previously zoomed — restore their range instead of auto-computing
            m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(m_preservedYRange.value());
            m_preservedYRange.reset();
        }
        else
        {
            // Compute min/max price from the last 60 non-null bars to set initial Y-axis range
            // We skip Null (void) bars since they have 0 prices and would distort the range
            double minPrice = std::numeric_limits<double>::max();
            double maxPrice = std::numeric_limits<double>::lowest();
            int barsAnalyzed = 0;

            for (int i = barsPtr->size() - 1; i >= 0 && barsAnalyzed < 60; --i)
            {
                const Bar& bar = barsPtr->at(i);
                if (bar.getBarStatus() == Bar::BarStatus::Null)
                {
                    continue;
                }
                minPrice = qMin(minPrice, bar.getLow());
                maxPrice = qMax(maxPrice, bar.getHigh());
                ++barsAnalyzed;
            }

            if (minPrice < maxPrice)
            {
                double padding = (maxPrice - minPrice) * 0.05; // 5% padding
                m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(minPrice - padding, maxPrice + padding);
            }
        }

        m_initialYAxisRangeSet = true;

        // Immediately sync the current time line's Y coords to the just-applied range.
        // Without this, the debounced onAxisRangeChanged would update the coords only
        // after the next event loop tick — after replot() below — making the line
        // temporarily invisible (its Y coords would still span the reset (0,100) range
        // rather than the actual price range).
        updateCurrentTimeLine();
    }

    //#warning TODO: optimize redraws
    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    m_customPlot->replot();

    // Now that we have bars, load historical orders, positions, and log markers
    loadHistoricalOrders();
    loadHistoricalPositions();
    loadStrategyLogMarkers();
}

/**
 * @brief Handles the failure of a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsFailed()
{
    DEBUG << "Missing bars request failed, releasing semaphore";

    // Sanity check: semaphore should be acquired (count == 0) when we receive the failure notification
    OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 0);
    m_missingBarsRequestSemaphore.release();
}

/**
 * @brief Updates the horizontal last price line.
 * The line is "sticky" - when the price is outside the visible Y-axis range,
 * it sticks to the top or bottom edge of the chart to maintain visual awareness.
 */
void StockPriceChart::redrawLastPriceLine()
{
    if (m_latestBarIndex == -1)
    {
        m_priceLabel->setVisible(false);
        return;
    }

    double lastPrice = m_latestBar.getClose();
    QColor lineColor = (m_latestBar.getClose() >= m_latestBar.getOpen()) ? Qt::green : Qt::red;

    // Get the visible Y-axis range
    QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();

    // Clamp the line position to visible range (sticky behavior)
    double displayPrice = lastPrice;
    if (lastPrice < yRange.lower)
    {
        displayPrice = yRange.lower;
    }
    else if (lastPrice > yRange.upper)
    {
        displayPrice = yRange.upper;
    }

    // Update line at clamped position
    m_lastPriceLine->start->setCoords(m_customPlot->xAxis->range().lower, displayPrice);
    m_lastPriceLine->end->setCoords(m_customPlot->xAxis->range().upper, displayPrice);
    m_lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update label at clamped position, but show actual price value
    m_priceLabel->setText(QString::number(lastPrice, 'f', 2));
    m_priceLabel->setColor(lineColor);
    m_priceLabel->position->setCoords(m_customPlot->xAxis->range().upper, displayPrice);
    m_priceLabel->setVisible(true);
}

/**
 * @brief Checks if the current view requires missing bars to be loaded.
 */
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime)
{
    Q_UNUSED(viewEndTime);

    // Try to acquire the semaphore - if it fails, a request is already in progress
    if (!m_missingBarsRequestSemaphore.tryAcquire())
    {
        DEBUG << "Missing bars request already in progress, skipping";
        return;
    }

    if (!m_index0Timestamp.isValid())
    {
        m_missingBarsRequestSemaphore.release();
        return;
    }

    DEBUG << "Check for missing bars for view range:" << viewStartTime.toString(Qt::ISODate) << "to"
          << viewEndTime.toString(Qt::ISODate);

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to the nearest minute
    viewStartTimeRounded = viewStartTimeRounded.addSecs(-viewStartTimeRounded.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());

    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);

    // Determine the earliest bar time we already have loaded
    // If no bars loaded yet, treat index 0 as the boundary (need everything before it)
    QDateTime firstBarTime;
    if (!timestampToIndex.isEmpty())
    {
        firstBarTime = timestampToIndex.firstKey();
    }
    else
    {
        firstBarTime = m_index0Timestamp;
    }
    OBJ_ASSUME_TRUE(firstBarTime.timeZone() == TradingHours::MARKET_TIMEZONE);

    if (viewStartTimeRounded >= firstBarTime)
    {
        // View is within available bars - release semaphore before returning
        m_missingBarsRequestSemaphore.release();
        return;
    }

    DEBUG << "Chart view extends beyond available bars:";
    DEBUG << "  Last :" << firstBarTime;
    DEBUG << "  First:" << viewStartTimeRounded;
    DEBUG << "firstBarInNY:" << firstBarTime << "viewStartInNY:" << viewStartTimeRounded;
    DEBUG << "firstBarInNY.date():" << firstBarTime.date() << "viewStartInNY.date():" << viewStartTimeRounded.date();
    DEBUG << "Date comparison:" << (viewStartTimeRounded.date() < firstBarTime.date());

    QDateTime requestStartTime;
    QDateTime requestEndTime;

    if (viewStartTimeRounded.date() < firstBarTime.date())
    {
        requestStartTime = viewStartTimeRounded;
        requestStartTime.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);

        requestEndTime = viewStartTimeRounded;
        requestEndTime.setTime(TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

        DEBUG << "Requesting previous day from" << requestStartTime << "to" << requestEndTime;
    }
    else
    {
        requestStartTime = firstBarTime;
        requestStartTime.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);

        requestEndTime = firstBarTime;
        requestEndTime = requestEndTime.addSecs(-60);

        DEBUG << "Requesting for same day from" << requestStartTime << "to" << requestEndTime;
    }

    OBJ_ASSUME_LT(requestStartTime, requestEndTime);

    emit requestMissingBars(requestStartTime, requestEndTime);
}

/**
 * @brief Clears all data and resets the chart for a new symbol.
 */
void StockPriceChart::clearSymbol()
{
    m_candlesticks->data()->clear();
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    clearBackgroundRects();

    indexToBar.clear();
    timestampToIndex.clear();
    m_index0Timestamp = QDateTime(); // Reset time anchor

    m_latestBarIndex = -1;
    m_latestBar = Bar();
    m_chartLiveBarIndex = -1;
    m_chartLiveBarIsUp = true;
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);

    // Reset state flags for new symbol
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;
    m_preservedYRange.reset(); // Discard any saved range — new symbol, fresh start

    // Reset semaphore to available state (1) for new symbol
    // If it was acquired (count == 0), release it; if already available, do nothing
    if (m_missingBarsRequestSemaphore.available() == 0)
    {
        DEBUG << "Releasing semaphore during clearSymbol - previous request was in-flight";
        m_missingBarsRequestSemaphore.release();
    }

    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_customPlot->replot();
}

void StockPriceChart::clearChart()
{
    INFO << "Clearing chart data for replay mode";

    // Clear order visualizations immediately
    clearOrderVisualizations();

    // Clear all candlestick and volume data
    m_candlesticks->data()->clear();
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    clearBackgroundRects();

    // Clear index mappings
    indexToBar.clear();
    timestampToIndex.clear();
    m_index0Timestamp = QDateTime(); // Reset time anchor

    // Reset bar tracking
    m_latestBarIndex = -1;
    m_latestBar = Bar();
    m_chartLiveBarIndex = -1;
    m_chartLiveBarIsUp = true;

    // Hide price label but keep symbol watermark (same symbol in replay)
    m_priceLabel->setVisible(false);

    // Stop current time line (replay has its own time)
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);

    // Reset state flags
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;

    // Reset semaphore to available state
    if (m_missingBarsRequestSemaphore.available() == 0)
    {
        DEBUG << "Releasing semaphore during clearChart - previous request was in-flight";
        m_missingBarsRequestSemaphore.release();
    }

    // Reset view range
    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_customPlot->replot();

    // Re-initialize time anchor (needed for replay restart and live mode re-entry)
    if (!m_symbol.isEmpty())
    {
        initializeTimeAnchor();
    }

    DEBUG << "Chart cleared for replay mode";
}
