// Bar management and drawing functionality for StockPriceChart
// Contains: addLiveBar, updateCandlestickData, rescaleVolumeAxisToVisibleRange,
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
#include "BarUtils.h"
#include "SQL/StockPriceChartQueries.h"
#include "LTTng/LTTngTracepoints.h"
#include "Indicators/VolumeIndicator.h"
#include "BarCache.h"
#include "MainApp.h"
#include "Order.h"
#include "Position.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"
#define LOGGING_CATEGORY ChartLog

namespace
{
    [[nodiscard]] bool isItemOwnedByPlot(const QCustomPlot* p_plot, const QCPAbstractItem* p_item)
    {
        if (p_plot == nullptr || p_item == nullptr)
        {
            return false;
        }

        const int itemCount = p_plot->itemCount();
        for (int i = 0; i < itemCount; ++i)
        {
            if (p_plot->item(i) == p_item)
            {
                return true;
            }
        }

        return false;
    }
} // namespace

void StockPriceChart::addLiveBar(const QString& symbol, const Bar& bar)
{
    // Its a bug if we receive a new bar for a different symbol than current
    OBJ_ASSUME_EQUAL(symbol, m_symbol);
    OBJ_ASSUME_TRUE(bar.isValid());
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());

    DEBUG << "Received bar for" << symbol << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
          << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "O:" << bar.getOpen() << "H:" << bar.getHigh()
          << "L:" << bar.getLow() << "C:" << bar.getClose();

    m_isReplayNoDataState = false;

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
    const int index = ChartTimeUtils::timestampToChartIndex(bar.getTimeStamp(), m_index0Timestamp, m_displayTimeFrame);
    LTTnG_TP(opentraderplatform, gui_chart_add_bar, symbol.toUtf8().constData(), index);

    // QCustomPlot centers candlesticks on their key. To align the left edge of the candle
    // with the bar's open time, we offset the key by half the candle width in index-space.
    // The x-axis is in minute slots (e.g. 5m bar spans 5 slots → offset = 2.5).
    const double keyOffset = chartIndexKeyOffset(m_displayTimeFrame);
    const double displayKey = index + keyOffset;

    indexToBar[index] = bar;
    timestampToIndex[bar.getTimeStamp()] = index;
    m_latestBar = bar;
    m_latestBarIndex = index;

    // Incremental QCP data update — O(1) in the common case.
    // Closed bars already in the containers never change; only the live bar ticks.
    // Volume is rebuilt by the volume indicator from indexToBar.
    const Bar::BarStatus status = bar.getBarStatus();
    if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
    {
        if (index == m_chartLiveBarIndex && m_candlesticks->data()->size() > 0)
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
        }
        else
        {
            // --- New bar index: just append (O(1) since key > all existing keys) ---
            QCPFinancialData candleData;
            candleData.key = displayKey;
            candleData.open = bar.getOpen();
            candleData.high = bar.getHigh();
            candleData.low = bar.getLow();
            candleData.close = bar.getClose();
            m_candlesticks->data()->add(candleData);

            m_chartLiveBarIndex = index;
        }

        rescaleVolumeAxisToVisibleRange();
    }

    if (!m_datesWithBackgrounds.contains(bar.getTimeStamp().date()))
    {
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});
    }

    refreshIndicators();
    redrawLastPriceLine();

    if (m_currentOpenPosition && !m_currentOpenPosition->isClosed)
    {
        double currentPrice = bar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    if (m_activeBracketOverlay.has_value())
    {
        updateBracketOverlayVisuals();
    }

    if (m_pendingRecenterToPriceAction)
    {
        recenterToCurrentPriceAction(false);
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

    // QCustomPlot centers candlesticks on their key. Offset by half width to align left edge.
    const double keyOffset = chartIndexKeyOffset(m_displayTimeFrame);

    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();

        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            QCPFinancialData barData;
            barData.key = index + keyOffset;
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
 * @brief Rescales the volume Y-axis using the active volume indicator settings.
 */
void StockPriceChart::rescaleVolumeAxisToVisibleRange()
{
    ASSUME_TRUE(m_volumeIndicator != nullptr);
    if (!m_volumeIndicator->isVisible())
    {
        return;
    }

    m_volumeIndicator->rescaleVisibleRange(indexToBar, m_customPlot->xAxis->range());
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
void StockPriceChart::drawSessionBackgroundsForDate(const QDate& date)
{
    if (m_datesWithBackgrounds.contains(date))
    {
        return;
    }

    // Draw early pre-market rectangle (4:00am - 5:59am ET) - paler orange
    drawFixedBackgroundRect(date,
                            TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                            TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION,
                            m_earlyPreMarketBackgroundColor,
                            m_earlyPreMarketRects);

    // Draw pre-market rectangle (6:00am - 9:29am ET)
    drawFixedBackgroundRect(date,
                            TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION,
                            TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION,
                            m_preMarketBackgroundColor,
                            m_preMarketRects);

    // Draw after-hours rectangle (4:00pm - 6:59pm ET)
    drawFixedBackgroundRect(date,
                            TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION,
                            TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                            m_afterHoursBackgroundColor,
                            m_afterHoursRects);

    m_datesWithBackgrounds.insert(date);
    DEBUG << "Created background rectangles for date" << date.toString();
}

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
        drawSessionBackgroundsForDate(date);
    }
}

/**
 * @brief Clears all background rectangles from the chart.
 */
void StockPriceChart::clearBackgroundRects()
{
    // Collect all pointers first, then clear owning containers immediately so we never re-enter
    // this path with stale raw pointers left behind.
    QList<QCPAbstractItem*> itemsToRemove;
    itemsToRemove.reserve(m_earlyPreMarketRects.size() + m_preMarketRects.size() + m_afterHoursRects.size() +
                          m_holidayDayRects.size() + m_holidayLabels.size());

    auto collectItems = [&itemsToRemove](const auto& p_items)
    {
        for (auto* item: p_items)
        {
            if (item != nullptr)
            {
                itemsToRemove.append(item);
            }
        }
    };

    collectItems(m_earlyPreMarketRects);
    collectItems(m_preMarketRects);
    collectItems(m_afterHoursRects);
    collectItems(m_holidayDayRects);
    collectItems(m_holidayLabels);

    m_earlyPreMarketRects.clear();
    m_preMarketRects.clear();
    m_afterHoursRects.clear();
    m_holidayDayRects.clear();
    m_holidayLabels.clear();

    // Remove each live item only once. This avoids deleting through duplicate references and
    // avoids passing stale pointers that are no longer owned by the plot.
    QSet<QCPAbstractItem*> seenItems;
    for (QCPAbstractItem* item: itemsToRemove)
    {
        if (seenItems.contains(item))
        {
            continue;
        }

        seenItems.insert(item);

        if (!isItemOwnedByPlot(m_customPlot, item))
        {
            continue;
        }

        const bool removed = m_customPlot->removeItem(item);
        if (!removed)
        {
            WARNING << "Failed to remove background item" << item << "(not owned by plot anymore)";
        }
    }

    // Clear the tracking set so backgrounds can be redrawn
    m_datesWithBackgrounds.clear();
}

void StockPriceChart::updateSessionBackgroundRectBrushes()
{
    for (QCPItemRect* rect: m_earlyPreMarketRects)
    {
        Q_CHECK_PTR(rect);
        rect->setBrush(QBrush(m_earlyPreMarketBackgroundColor));
    }

    for (QCPItemRect* rect: m_preMarketRects)
    {
        Q_CHECK_PTR(rect);
        rect->setBrush(QBrush(m_preMarketBackgroundColor));
    }

    for (QCPItemRect* rect: m_afterHoursRects)
    {
        Q_CHECK_PTR(rect);
        rect->setBrush(QBrush(m_afterHoursBackgroundColor));
    }
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

    // Bars are left-aligned: candle at index N is drawn from N to N+1 (key set to N+0.5 so
    // QCustomPlot centers it correctly). The rectangle must align with those left edges.
    sessionEndIndex += 1.0; // Extend to the right edge of the last candle

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
 * @brief Draws a grey full-day background and a centered holiday name watermark for a day with no trading.
 *
 * Called when a missing-bars request for a known NYSE holiday returns zero bars.
 * The day still appears as a slot on the timeline so the surrounding days remain
 * visually connected, but is clearly marked as a non-trading day.
 */
void StockPriceChart::drawHolidayDayMarker(const QDate& date, const QString& holidayName)
{
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());

    if (m_datesWithBackgrounds.contains(date))
        return; // Already drawn

    // Compute index range for the entire trading-hours span of this day
    QDateTime dayStart(date, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
    QDateTime dayEnd(date, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);

    const double startIndex = static_cast<double>(getIndexForTimestamp(dayStart));
    const double endIndex = static_cast<double>(getIndexForTimestamp(dayEnd)) + 1.0;
    const double midIndex = (startIndex + endIndex) / 2.0;

    // Grey background spanning the full day
    QCPItemRect* rect = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(rect);
    rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    rect->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    rect->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->topLeft->setCoords(startIndex, 0);
    rect->bottomRight->setCoords(endIndex, 1);
    rect->setPen(Qt::NoPen);
    rect->setBrush(QBrush(QColor(50, 50, 55))); // Dark grey — distinct from normal session colors
    rect->setLayer("background");
    m_holidayDayRects.append(rect);

    // Holiday name watermark centered vertically and horizontally on the day
    QCPItemText* label = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(label);
    label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    label->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    label->position->setType(QCPItemPosition::ptPlotCoords);
    label->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
    label->position->setCoords(midIndex, 0.5);
    label->setText(holidayName);
    label->setFont(QFont(font().family(), 11, QFont::Bold));
    label->setColor(QColor(200, 200, 200, 160));
    label->setRotation(90); // Vertical text fits narrow day columns
    label->setLayer("background");
    m_holidayLabels.append(label);

    m_datesWithBackgrounds.insert(date);
    DEBUG << "Drew holiday marker for" << date << ":" << holidayName;
}


void StockPriceChart::onRequestedMissingBarsReceived(const QString& p_symbol,
                                                     uint64_t p_requestToken,
                                                     const std::shared_ptr<QVector<Bar>>& barsPtr)
{
    LTTnG_TP(opentraderplatform, gui_chart_backfill_received, static_cast<int>(barsPtr->size()));

    DEBUG << "Received missing bars response with" << barsPtr->size() << "bars";

    if (!isExpectedMissingBarsRequest(p_symbol, p_requestToken))
    {
        DEBUG << "Ignoring stale missing bars response for" << p_symbol << "token" << p_requestToken
              << "(current symbol:" << m_symbol << "current token:" << m_currentMissingBarsRequestToken.load() << ")";
        return;
    }

    // Mark request as completed
    m_currentMissingBarsRequestToken.store(0);
    stopLoadingSpinner();
    m_isReplayNoDataState = false;
    m_nextHistoryRetryTime = {};
    m_historyRetryDelayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;

    addHistoricalBarsToIndexMapping(barsPtr);

    int tradableBarCount = 0;
    for (const Bar& bar: *barsPtr)
    {
        if (bar.getBarStatus() == Bar::BarStatus::Open || bar.getBarStatus() == Bar::BarStatus::Closed)
            ++tradableBarCount;
    }

    DEBUG << "Missing bars response tradable bar count:" << tradableBarCount
          << "null/uninitialized count:" << (barsPtr->size() - tradableBarCount);

    DEBUG << "After addHistoricalBarsToIndexMapping, index range:"
          << QString("%1 to %2").arg(indexToBar.firstKey()).arg(indexToBar.lastKey());

    // Draw background rectangles for the visible range
    drawBackgroundsForReceivedBars(*barsPtr);

    // Only set initial Y-axis range on the first batch of historical bars
    if (!m_initialYAxisRangeSet)
    {
        bool appliedInitialYRange = false;
        if (m_preservedYRange.has_value())
        {
            // User had previously zoomed — restore their range instead of auto-computing
            m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(m_preservedYRange.value());
            m_preservedYRange.reset();
            appliedInitialYRange = true;
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
                appliedInitialYRange = true;
            }
        }

        if (appliedInitialYRange)
        {
            m_initialYAxisRangeSet = true;

            // Immediately sync the current time line's Y coords to the just-applied range.
            // Without this, the debounced onAxisRangeChanged would update the coords only
            // after the next event loop tick — after replot() below — making the line
            // temporarily invisible (its Y coords would still span the reset (0,100) range
            // rather than the actual price range).
            updateCurrentTimeLine();
        }
        else
        {
            DEBUG << "Initial Y range not set yet because this historical batch contained no tradable bars";
        }
    }

    //#warning TODO: optimize redraws
    // Update candlestick data
    updateCandlestickData();
    refreshIndicators();
    rescaleVolumeAxisToVisibleRange();
    redrawLastPriceLine();

    if (m_pendingRecenterToPriceAction)
    {
        recenterToCurrentPriceAction(false);
    }

    m_customPlot->replot();

    if (MainApp::isInReplayMode() && tradableBarCount == 0 && m_latestBarIndex == -1 && m_index0Timestamp.isValid() &&
        !m_symbol.isEmpty())
    {
        const QDate earliestLoadedDate =
            timestampToIndex.isEmpty() ? m_index0Timestamp.date() : timestampToIndex.firstKey().date();
        const QDateTime previousDayProbe(earliestLoadedDate.addDays(-1),
                                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                         TradingHours::MARKET_TIMEZONE);

        DEBUG
            << "Historical batch contained no tradable bars; continuing replay startup backfill into prior trading day from"
            << previousDayProbe.toString(Qt::ISODate);

        checkForMissingBars(previousDayProbe, m_index0Timestamp);
        return;
    }

    if (!MainApp::getInstance()->isReplayFreshSessionPending())
    {
        // Now that we have bars, load historical orders, positions, and log markers
        loadHistoricalOrders();
        loadHistoricalPositions();
        loadStrategyLogMarkers();
        loadStrategyBracketOverlays();
    }
    else
    {
        DEBUG << "Skipping historical replay trading state load while waiting for a fresh replay session start";
    }

    // Re-check whether the view still extends beyond the newly loaded bars.
    // This drives the automatic day-by-day backfill when the user has zoomed
    // out far enough to expose multiple missing days: each arriving day releases
    // the token and immediately schedules the next request without requiring any
    // further user interaction.
    onAxisRangeChanged();
}

/**
 * @brief Handles the failure of a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsError(const QString& p_symbol,
                                                  uint64_t p_requestToken,
                                                  bool p_terminal,
                                                  const QString& p_message)
{
    if (!isExpectedMissingBarsRequest(p_symbol, p_requestToken))
    {
        DEBUG << "Ignoring stale historical error for" << p_symbol << "token" << p_requestToken;
        return;
    }
    m_currentMissingBarsRequestToken.store(0);
    stopLoadingSpinner();
    WARNING << "Historical request failed for" << p_symbol << "terminal=" << p_terminal << p_message;
    showChartStatusMessage(p_message);
    if (p_terminal)
    {
        m_historyRequestRejected = true;
        return;
    }
    const int delay = m_historyRetryDelayMs;
    m_historyRetryDelayMs = qMin(delay * 2, StreamConstants::LIVE_RETRY_MAX_DELAY_MS);
    m_nextHistoryRetryTime = QDateTime::currentDateTime().addMSecs(delay);
    QTimer::singleShot(delay,
                       this,
                       [this, p_symbol]()
                       {
                           if (m_symbol == p_symbol && !m_historyRequestRejected)
                           {
                               onAxisRangeChanged();
                           }
                       });
}

void StockPriceChart::onRequestedMissingBarsFailed(const QString& p_symbol, uint64_t p_requestToken)
{
    DEBUG << "Missing bars request failed";

    if (!isExpectedMissingBarsRequest(p_symbol, p_requestToken))
    {
        DEBUG << "Ignoring stale missing bars failure for" << p_symbol << "token" << p_requestToken
              << "(current symbol:" << m_symbol << "current token:" << m_currentMissingBarsRequestToken.load() << ")";
        return;
    }

    // Mark request as completed
    m_currentMissingBarsRequestToken.store(0);
    stopLoadingSpinner();

    m_nextHistoryRetryTime = {};
    m_historyRetryDelayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;

    if (!MainApp::isInReplayMode() && !MainApp::isInReviewMode() && !TSClient::getInstance()->isAuthenticated())
    {
        showChartStatusMessage("TradeStation authentication required - reconnect in Credentials");
        return;
    }

    // Record the failed date as a known-empty day (holiday or non-trading day)
    if (m_lastRequestedDate.isValid())
    {
        DEBUG << "Recording" << m_lastRequestedDate << "as known-empty (no bars returned)";
        m_knownEmptyDates.insert(m_lastRequestedDate);

        // If this date is a known NYSE holiday, draw a grey background + name watermark
        // so the user sees "Presidents' Day" etc. rather than a blank gap.
        const QString name = MarketCalendar::getHolidayName(m_lastRequestedDate);
        if (!name.isEmpty() && m_index0Timestamp.isValid())
        {
            drawHolidayDayMarker(m_lastRequestedDate, name);
            m_customPlot->replot();
        }
    }

    if (MainApp::isInReplayMode() && indexToBar.isEmpty() && timestampToIndex.isEmpty() &&
        m_index0Timestamp.isValid() && !m_symbol.isEmpty())
    {
        const QDate earliestLoadedDate =
            timestampToIndex.isEmpty() ? m_index0Timestamp.date() : timestampToIndex.firstKey().date();
        const QDateTime previousDayProbe(earliestLoadedDate.addDays(-1),
                                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                         TradingHours::MARKET_TIMEZONE);

        DEBUG << "Replay startup still has no bars after empty fetch; continuing backfill into prior trading day from"
              << previousDayProbe.toString(Qt::ISODate);

        checkForMissingBars(previousDayProbe, m_index0Timestamp);
        return;
    }

    // Continue the automatic backfill chain: re-check whether the view still
    // extends left — the next iteration will skip the just-failed date.
    onAxisRangeChanged();
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
    const double ySpan = qMax(0.01, yRange.upper - yRange.lower);
    const double yPadding = qMax(0.01, ySpan * 0.02);

    // Clamp the line position to visible range (sticky behavior)
    const double linePrice = qBound(yRange.lower, lastPrice, yRange.upper);

    double labelPrice = linePrice;
    Qt::Alignment labelAlignment = Qt::AlignRight | Qt::AlignVCenter;
    QString edgeSuffix;
    if (lastPrice > yRange.upper - yPadding)
    {
        labelPrice = yRange.upper - yPadding;
        labelAlignment = Qt::AlignRight | Qt::AlignTop;
        edgeSuffix = QStringLiteral(" ↑");
    }
    else if (lastPrice < yRange.lower + yPadding)
    {
        labelPrice = yRange.lower + yPadding;
        labelAlignment = Qt::AlignRight | Qt::AlignBottom;
        edgeSuffix = QStringLiteral(" ↓");
    }

    // Update line at clamped position
    m_lastPriceLine->start->setCoords(m_customPlot->xAxis->range().lower, linePrice);
    m_lastPriceLine->end->setCoords(m_customPlot->xAxis->range().upper, linePrice);
    m_lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update label at clamped position, but show actual price value
    m_priceLabel->setText(QString("%1%2").arg(lastPrice, 0, 'f', 2).arg(edgeSuffix));
    m_priceLabel->setColor(lineColor);
    m_priceLabel->setPositionAlignment(labelAlignment);
    m_priceLabel->position->setCoords(m_customPlot->xAxis->range().upper - 0.1, labelPrice);
    m_priceLabel->setVisible(true);
}

void StockPriceChart::startLoadingSpinner()
{
    m_loadingSpinnerFrame = 0;
    m_loadingSpinner->setText("⠋ Loading…");
    m_loadingSpinner->setVisible(true);
    m_loadingSpinnerTimer->start();
}

void StockPriceChart::stopLoadingSpinner()
{
    m_loadingSpinnerTimer->stop();
    m_loadingSpinner->setVisible(false);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::showChartStatusMessage(const QString& p_message)
{
    m_loadingSpinnerTimer->stop();
    m_loadingSpinner->setText(p_message);
    m_loadingSpinner->setVisible(true);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

/**
 * @brief Checks if the current view requires missing bars to be loaded.
 */
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime)
{
    Q_UNUSED(viewEndTime);

    if (m_historyRequestRejected || QDateTime::currentDateTime() < m_nextHistoryRetryTime)
    {
        return;
    }
    if (!MainApp::isInReplayMode() && !MainApp::isInReviewMode() && !TSClient::getInstance()->isAuthenticated())
    {
        return;
    }

    if (m_isReplayNoDataState)
    {
        DEBUG << "Skipping missing bars check while chart is in replay no-data state";
        return;
    }

    // Check if a request is already in progress (token != 0 means request pending)
    if (m_currentMissingBarsRequestToken.load() != 0)
    {
        DEBUG << "Missing bars request already in progress, skipping";
        return;
    }

    if (!m_index0Timestamp.isValid() || m_symbol.isEmpty())
    {
        return;
    }

    DEBUG << "Check for missing bars for view range:" << viewStartTime.toString(Qt::ISODate) << "to"
          << viewEndTime.toString(Qt::ISODate);
    const int barStepSeconds = BarUtils::secondsPerBar(m_displayTimeFrame);
    ASSUME_GT(barStepSeconds, 0);

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to timeframe boundary (1m/5m...) in market time.
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());

    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);
    const QTime roundedTime = viewStartTimeRounded.time();
    if (roundedTime <= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        viewStartTimeRounded.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    }
    else if (roundedTime >= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        viewStartTimeRounded.setTime(TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);
    }
    else
    {
        const int secondsFromOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.secsTo(
            QTime(roundedTime.hour(), roundedTime.minute(), roundedTime.second()));
        const int flooredSecondsFromOpen = (secondsFromOpen / barStepSeconds) * barStepSeconds;
        viewStartTimeRounded.setTime(
            TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.addSecs(flooredSecondsFromOpen));
    }

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
        // View is within available bars - no request needed
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

    const QDateTime replayAnchorDayStart(firstBarTime.date(),
                                         TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                         TradingHours::MARKET_TIMEZONE);
    const bool shouldLoadReplayAnchorDayFirst = MainApp::isInReplayMode() && timestampToIndex.isEmpty() &&
                                                replayAnchorDayStart < firstBarTime &&
                                                !m_knownEmptyDates.contains(firstBarTime.date());
    const bool shouldLoadLiveAnchorDayFirst = !MainApp::isInReplayMode() && timestampToIndex.isEmpty() &&
                                              replayAnchorDayStart < firstBarTime &&
                                              !m_knownEmptyDates.contains(firstBarTime.date());

    if (shouldLoadReplayAnchorDayFirst || shouldLoadLiveAnchorDayFirst)
    {
        requestStartTime = replayAnchorDayStart;
        requestEndTime = firstBarTime.addSecs(-barStepSeconds);

        m_lastRequestedDate = firstBarTime.date();
        if (MainApp::isInReplayMode())
        {
            DEBUG << "Replay chart is empty; requesting replay anchor day first from" << requestStartTime << "to"
                  << requestEndTime;
        }
        else
        {
            DEBUG << "Live chart is empty; requesting current anchor day first from" << requestStartTime << "to"
                  << requestEndTime;
        }
    }
    else if (viewStartTimeRounded.date() < firstBarTime.date())
    {
        // Always request the trading day immediately before our earliest loaded day.
        // Using viewStartTimeRounded directly would skip all intermediate days when
        // the user pans left by more than one day in a single gesture: the far day
        // would load, firstKey() would jump there, and the in-between days would
        // never be requested (view appears within available bars). By anchoring on
        // firstBarTime we fill the gap chain one trading day at a time.
        //
        // Also skip weekends AND m_knownEmptyDates (holidays / non-trading days
        // that previously returned zero bars) AND statically-known NYSE holidays
        // so we never send an API request for a day that can never have bars.
        QDate requestDate = firstBarTime.date().addDays(-1);
        while (requestDate.dayOfWeek() > 5 || m_knownEmptyDates.contains(requestDate) ||
               !MarketCalendar::getHolidayName(requestDate).isEmpty())
        {
            // Draw a holiday marker inline as we skip past it, so the chart
            // never shows a blank gap — even on the first visit.
            if (requestDate.dayOfWeek() <= 5 && !m_knownEmptyDates.contains(requestDate))
            {
                const QString name = MarketCalendar::getHolidayName(requestDate);
                if (!name.isEmpty())
                {
                    m_knownEmptyDates.insert(requestDate);
                    if (m_index0Timestamp.isValid())
                        drawHolidayDayMarker(requestDate, name);
                }
            }
            requestDate = requestDate.addDays(-1);
        }

        requestStartTime = QDateTime(requestDate,
                                     TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                     TradingHours::MARKET_TIMEZONE);
        requestEndTime =
            QDateTime(requestDate, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);

        m_lastRequestedDate = requestDate;
        DEBUG << "Requesting day before first loaded bar:" << requestStartTime << "to" << requestEndTime;
    }
    else
    {
        if (m_knownEmptyDates.contains(firstBarTime.date()))
        {
            return;
        }
        requestStartTime = firstBarTime;
        requestStartTime.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);

        requestEndTime = firstBarTime;
        requestEndTime = requestEndTime.addSecs(-barStepSeconds);

        m_lastRequestedDate = firstBarTime.date();
        DEBUG << "Requesting for same day from" << requestStartTime << "to" << requestEndTime;
    }

    OBJ_ASSUME_LT(requestStartTime, requestEndTime);

    const uint64_t requestToken = m_missingBarsRequestSequence.fetch_add(1);
    OBJ_ASSUME_GTE(requestToken, uint64_t{1});
    m_currentMissingBarsRequestToken.store(requestToken);
    startLoadingSpinner();

    LTTnG_TP(opentraderplatform,
             chart_missing_bars_request,
             m_symbol.toUtf8().constData(),
             static_cast<int>(m_displayTimeFrame),
             requestStartTime.toString(Qt::ISODate).toUtf8().constData(),
             requestEndTime.toString(Qt::ISODate).toUtf8().constData());

    emit requestMissingBars(m_symbol, requestStartTime, requestEndTime, requestToken);
}

/**
 * @brief Clears all data and resets the chart for a new symbol.
 */
void StockPriceChart::clearSymbol()
{
    if (m_indicatorManager != nullptr)
    {
        m_indicatorManager->clearAll();
    }

    m_candlesticks->data()->clear();
    clearBackgroundRects();

    indexToBar.clear();
    timestampToIndex.clear();
    m_index0Timestamp = QDateTime(); // Reset time anchor

    m_latestBarIndex = -1;
    m_latestBar = Bar();
    m_chartLiveBarIndex = -1;
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);
    m_bestBidPrice.reset();
    m_bestAskPrice.reset();
    m_latestLevel2Snapshot.reset();
    m_level2DepthSizeSamples.clear();
    updateBboOverlay();
    updateLevel2DepthOverlay();
    m_activeStrategyStatus.reset();
    clearStrategyStatusVisual();

    // Reset state flags for new symbol
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;
    m_preservedYRange.reset(); // Discard any saved range — new symbol, fresh start

    // Cancel any in-flight missing bars request (stale responses will be ignored)
    m_currentMissingBarsRequestToken.store(0);
    m_lastRequestedDate = QDate();
    m_knownEmptyDates.clear();
    m_historyRequestRejected = false;
    m_nextHistoryRetryTime = {};
    m_historyRetryDelayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;
    m_isReplayNoDataState = false;
    m_pendingRecenterToPriceAction = false;
    stopLoadingSpinner();
    resetBracketWheelMode(QStringLiteral("clear-symbol"));

    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_customPlot->replot();
}

void StockPriceChart::clearChart(bool p_replot)
{
    INFO << "Clearing chart data for replay mode";

    if (m_indicatorManager != nullptr)
    {
        m_indicatorManager->clearAll();
    }

    // Clear order visualizations immediately
    clearOrderVisualizations();

    // Clear all candlestick and volume data
    m_candlesticks->data()->clear();
    clearBackgroundRects();

    // Clear index mappings
    indexToBar.clear();
    timestampToIndex.clear();
    m_index0Timestamp = QDateTime(); // Reset time anchor

    // Reset bar tracking
    m_latestBarIndex = -1;
    m_latestBar = Bar();
    m_chartLiveBarIndex = -1;

    // Hide price label but keep symbol watermark (same symbol in replay)
    m_priceLabel->setVisible(false);

    // Stop current time line (replay has its own time)
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);
    m_bestBidPrice.reset();
    m_bestAskPrice.reset();
    m_latestLevel2Snapshot.reset();
    m_level2DepthSizeSamples.clear();
    updateBboOverlay();
    updateLevel2DepthOverlay();

    // Reset state flags
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;

    // Cancel any in-flight missing bars request (stale responses will be ignored)
    m_currentMissingBarsRequestToken.store(0);
    m_lastRequestedDate = QDate();
    m_knownEmptyDates.clear();
    m_isReplayNoDataState = false;
    m_pendingRecenterToPriceAction = false;
    stopLoadingSpinner();

    // Reset view range (unless preserved for timescale switch)
    if (!m_preservedXRange.has_value())
    {
        m_customPlot->xAxis->setRange(0, 30);
    }
    if (!m_preservedYRange.has_value())
    {
        m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);
    }

    if (p_replot)
        m_customPlot->replot();

    // Re-initialize time anchor (needed for replay restart and live mode re-entry)
    if (!m_symbol.isEmpty())
    {
        initializeTimeAnchor();
    }

    DEBUG << "Chart cleared for replay mode";
}
