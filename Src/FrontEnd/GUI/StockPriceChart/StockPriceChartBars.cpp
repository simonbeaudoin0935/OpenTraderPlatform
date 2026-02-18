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

    DEBUG << "Received bar for" << symbol << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
          << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "isEndOfHistory:" << bar.getIsEndOfHistory()
          << "O:" << bar.getOpen() << "H:" << bar.getHigh() << "L:" << bar.getLow() << "C:" << bar.getClose();

    // Putting unlikely because only at the start will this condition be true,
    // so optimizing for the cruising case
    if (startedReceivingRealtimeBars == false) [[unlikely]]
    {
        if (bar.getIsEndOfHistory())
        {
            OBJ_ASSUME_FALSE(bar.getIsRealtime());
            startedReceivingRealtimeBars = true;
        }
    }
    else if (bar.getIsRealtime() == false) [[unlikely]]
    {
        WARNING << "We received a double of the last bar of the day for symbol" << m_symbol
                << "at timestamp:" << bar.getTimeStamp()
                << "- Experimentally, this has proven to be possible from the API."
                   " It seems to be a little glitch from their side when the app sits idle after hours.";

        return; // Ignore this bar
    }

    // Is this the first bar ever received for this chart
    if (indexToBar.size() == 0) [[unlikely]]
    {
        // Should be the case becase we call openBarStream() with barsback=1,
        // so we always get at least one historical bar first
        if (MainApp::isInReplayMode() == false)
        {
            OBJ_ASSUME_TRUE(bar.getIsEndOfHistory() == true);
        }

        // Sanity check: semaphore should be available (count == 1) for the first bar
        OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 1);

        // Important that this be aquired here to block further requests until we finish processing this first bar.
        // the ->setRange() calls below trigger a checkForMissingBars() immediately due to the direct connection
        // of the signal/slot
        bool acquired = m_missingBarsRequestSemaphore.tryAcquire();
        OBJ_ASSUME_TRUE(acquired); // Should always succeed for first bar

        DEBUG << "Received first bar " << bar;

        // Now that we have the first bar, we can set up the custom time ticker
        // that converts index values to time labels
        QSharedPointer<IndexToTimeTicker> indexToTimeTicker(new IndexToTimeTicker);
        indexToTimeTicker->setTimeFormat("hh:mm");
        indexToTimeTicker->setIndexToTimestampFunction([this](int index) { return this->getTimestampForIndex(index); });
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(true);

        // Also set the same ticker on main chart's X-axis so grid lines align with nice times
        m_customPlot->xAxis->setTicker(indexToTimeTicker);

        OBJ_ASSUME_EQUAL(timestampToIndex.size(), 0);

        // The indexd of the first bar received when opening the stream is always 0
        // Historical bars fetched will go in the negative indices, and future bars in positive indices
        const int index = 0;

        timestampToIndex[bar.getTimeStamp()] = index;
        indexToBar[index] = bar;
        m_latestBar = bar;
        m_latestBarIndex = index;

        // Update candlestick data
        updateCandlestickData();
        updateVolumeData();

        // Center index 0 with 1 hour (60 bars) on each side
        m_customPlot->xAxis->setRange(-60, 60);

        double newPrice = bar.getClose();
        double padding = newPrice * 0.0002;
        double minRange = newPrice * 0.0005;
        m_customPlot->axisRect()
            ->axis(QCPAxis::atRight)
            ->setRange(qMax(0.0, newPrice - minRange / 2 - padding), newPrice + minRange / 2 + padding);

        m_customPlot->replot();

        // Draw background rectangles for the session
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});

        // Start the timer for updating the current time line
        m_currentTimeLine->setVisible(true);
        m_timeLineTimer->start();
        updateCurrentTimeLine(); // Update immediately

        // Update replay boundary lines if in replay mode and we have cached times
        if (m_isReplayModeActive && m_replayDayStart.isValid() && m_replayDayEnd.isValid())
        {
            updateReplayDayBoundaryLines();
        }

        // Here we will fetch the bars from the beginning of the day up to this bar to fill in history
        QDateTime first = QDateTime(bar.getTimeStamp().date(),
                                    TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                    TradingHours::MARKET_TIMEZONE);

        QDateTime last = bar.getTimeStamp().addSecs(-60);

        // Only fetch history if there are bars before the first streaming bar
        if (last >= first)
        {
            DEBUG << "Requesting whole day bars from" << first.toString(Qt::ISODate) << "to"
                  << last.toString(Qt::ISODate);
            emit requestMissingBars(first, last);
        }
        else
        {
            DEBUG << "First bar is the earliest candle, no history to fetch";
            m_missingBarsRequestSemaphore.release();
        }

        return;
    }

    if (MainApp::isInReplayMode() == false)
    {
        // Any live bar after the first one shall have the isEndOfHistory flag false
        OBJ_ASSUME_TRUE(bar.getIsEndOfHistory() == false);
    }

    switch (bar.getBarStatus())
    {
    case Bar::BarStatus::Uninitialized:
        // Should not receive uninitialized bars
        Q_UNREACHABLE();
        break;

    case Bar::BarStatus::Null:
        // Tradestation doesnt send 'null' bars, it is a construct that we created in this program
        Q_UNREACHABLE();
        break;

    case Bar::BarStatus::Closed:
    {
        if (startedReceivingRealtimeBars)
        {
            //                Q_ASSERT(m_latestBar.getBarStatus() == Bar::BarStatus::Open);
            // TradeStation sends a 'closed' bar to close the current candle. Therefore, its timestamp is the one
            // from the current candle
            //                OBJ_ASSUME_EQUAL(bar.getTimeStamp(), m_latestBar.getTimeStamp());
        }
        else
        {
            OBJ_ASSUME_TRUE(m_latestBar.getBarStatus() == Bar::BarStatus::Closed);
            OBJ_ASSUME_GT(bar.getTimeStamp(), m_latestBar.getTimeStamp());
        }

        // When we receive a closed bar, we shall not increase the latestBarIndex,
        // because the next bar to be received will be an open bar for the next candle
        // Therefore, we just replace the existing latest bar at latestBarIndex
        indexToBar[m_latestBarIndex] = bar;

        // Especially here, we need to store this bar with status = closed, because the
        // next open bar will need to know that the previous bar was closed.
        m_latestBar = bar;
    }
    break;

    case Bar::BarStatus::Open:
        if (m_latestBar.getBarStatus() == Bar::BarStatus::Closed)
        {
#warning I hit this assert. fuck tradestation
            // New bar after previous one was closed
            OBJ_ASSUME_GT(bar.getTimeStamp(), m_latestBar.getTimeStamp());

            const int newIndex = indexToBar.lastKey() + 1;

            timestampToIndex[bar.getTimeStamp()] = newIndex;
            indexToBar[newIndex] = bar;
            m_latestBar = bar;
            m_latestBarIndex = newIndex;
        }
        else
        {
            // Updating existing open bar
            if (bar.getTimeStamp() != m_latestBar.getTimeStamp())
            {
                CRITICAL << "Timestamp mismatch when updating existing open bar: new bar timestamp"
                         << bar.getTimeStamp().toString(Qt::ISODate) << "does not match latest bar timestamp"
                         << m_latestBar.getTimeStamp().toString(Qt::ISODate);

                return;
            }

            indexToBar[m_latestBarIndex] = bar;
            m_latestBar = bar;
        }
        break;
    }

    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    // Draw session backgrounds if this bar belongs to a new date (e.g., next day's pre-market just started)
    if (!m_datesWithBackgrounds.contains(bar.getTimeStamp().date()))
    {
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});
    }

    redrawLastPriceLine();

    // Update open position dynamic line and P&L box with current price
    if (m_currentOpenPosition && !m_currentOpenPosition->isClosed)
    {
        double currentPrice = bar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    m_customPlot->replot();
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

        // Draw early pre-market rectangle (4:01am - 6:00am ET) - paler orange
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 90),
                                m_earlyPreMarketRects);

        // Draw pre-market rectangle (6:01am - 9:30am ET)
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 180),
                                m_preMarketRects);

        // Draw after-hours rectangle (4pm - 8:00pm ET)
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
    OBJ_ASSUME_FALSE(indexToBar.isEmpty());
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

        m_initialYAxisRangeSet = true;
    }

    //#warning TODO: optimize redraws
    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    m_customPlot->replot();

    // Now that we have bars, load historical orders and positions
    loadHistoricalOrders();
    loadHistoricalPositions();
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

    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    DEBUG << "Check for missing bars for view range:" << viewStartTime.toString(Qt::ISODate) << "to"
          << viewEndTime.toString(Qt::ISODate);

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to the nearest minute
    viewStartTimeRounded = viewStartTimeRounded.addSecs(-viewStartTimeRounded.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());

    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);

    const QDateTime firstBarTime = timestampToIndex.firstKey();
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

    m_latestBarIndex = -1;
    m_latestBar = Bar();

    m_priceLabel->setVisible(false);
    m_symbolWatermark->setText("");

    // Stop and hide the current time line
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);

    // Reset state flags for new symbol
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;

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

    // Reset bar tracking
    m_latestBarIndex = -1;
    m_latestBar = Bar();

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

    DEBUG << "Chart cleared for replay mode";
}
