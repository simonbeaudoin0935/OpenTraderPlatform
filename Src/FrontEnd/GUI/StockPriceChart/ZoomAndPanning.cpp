#include "StockPriceChart.h"
#include "BarUtils.h"
#include "Logging.h"
#include "Indicators/VolumeIndicator.h"
#include "Misc/Settings.h"
#include "MainApp.h"
#include <QToolTip>
#include <cmath>
#include <limits>

#define LOGGING_CATEGORY ChartLog

bool StockPriceChart::recenterToCurrentPriceAction(const bool p_deferIfNoData)
{
    if (indexToBar.isEmpty())
    {
        m_pendingRecenterToPriceAction = p_deferIfNoData;
        return false;
    }

    int lookbackBars = ChartFocusConstants::DEFAULT_LOOKBACK_BARS;
    int rightPaddingBars = ChartFocusConstants::DEFAULT_RIGHT_PADDING_BARS;
    bool includeBboInYRange = ChartFocusConstants::DEFAULT_INCLUDE_BBO_IN_Y_RANGE;
    double yPaddingPercent = ChartFocusConstants::DEFAULT_Y_PADDING_PERCENT;
    double minRangePercent = ChartFocusConstants::DEFAULT_MIN_RANGE_PERCENT;
    double anchorPaddingPercent = ChartFocusConstants::DEFAULT_ANCHOR_PADDING_PERCENT;
    if (appStateSettings != nullptr)
    {
        lookbackBars =
            appStateSettings
                ->value(ChartFocusConstants::SETTINGS_KEY_LOOKBACK_BARS, ChartFocusConstants::DEFAULT_LOOKBACK_BARS)
                .toInt();
        rightPaddingBars = appStateSettings
                               ->value(ChartFocusConstants::SETTINGS_KEY_RIGHT_PADDING_BARS,
                                       ChartFocusConstants::DEFAULT_RIGHT_PADDING_BARS)
                               .toInt();
        includeBboInYRange = appStateSettings
                                 ->value(ChartFocusConstants::SETTINGS_KEY_INCLUDE_BBO_IN_Y_RANGE,
                                         ChartFocusConstants::DEFAULT_INCLUDE_BBO_IN_Y_RANGE)
                                 .toBool();
        yPaddingPercent = appStateSettings
                              ->value(ChartFocusConstants::SETTINGS_KEY_Y_PADDING_PERCENT,
                                      ChartFocusConstants::DEFAULT_Y_PADDING_PERCENT)
                              .toDouble();
        minRangePercent = appStateSettings
                              ->value(ChartFocusConstants::SETTINGS_KEY_MIN_RANGE_PERCENT,
                                      ChartFocusConstants::DEFAULT_MIN_RANGE_PERCENT)
                              .toDouble();
        anchorPaddingPercent = appStateSettings
                                   ->value(ChartFocusConstants::SETTINGS_KEY_ANCHOR_PADDING_PERCENT,
                                           ChartFocusConstants::DEFAULT_ANCHOR_PADDING_PERCENT)
                                   .toDouble();
    }
    lookbackBars =
        std::clamp(lookbackBars, ChartFocusConstants::MIN_LOOKBACK_BARS, ChartFocusConstants::MAX_LOOKBACK_BARS);
    rightPaddingBars = std::clamp(rightPaddingBars,
                                  ChartFocusConstants::MIN_RIGHT_PADDING_BARS,
                                  ChartFocusConstants::MAX_RIGHT_PADDING_BARS);
    yPaddingPercent = std::clamp(yPaddingPercent,
                                 ChartFocusConstants::MIN_Y_PADDING_PERCENT,
                                 ChartFocusConstants::MAX_Y_PADDING_PERCENT);
    minRangePercent = std::clamp(minRangePercent,
                                 ChartFocusConstants::MIN_MIN_RANGE_PERCENT,
                                 ChartFocusConstants::MAX_MIN_RANGE_PERCENT);
    anchorPaddingPercent = std::clamp(anchorPaddingPercent,
                                      ChartFocusConstants::MIN_ANCHOR_PADDING_PERCENT,
                                      ChartFocusConstants::MAX_ANCHOR_PADDING_PERCENT);

    double xAnchorIndex = 0.0;
    if (m_index0Timestamp.isValid())
    {
        QDateTime currentTime = MainApp::getCurrentAppTime();
        qint64 secondsDiff = m_index0Timestamp.secsTo(currentTime);

        const QTime currentTimeOfDay = currentTime.time();
        const QTime marketOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION;
        const QTime marketClose = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

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

        xAnchorIndex = static_cast<double>(secondsDiff) / chartSecondsPerIndexUnit(m_displayTimeFrame);
    }
    else if (m_currentTimeLine->visible())
    {
        xAnchorIndex = m_currentTimeLine->start->coords().x();
    }
    else
    {
        xAnchorIndex = static_cast<double>((m_latestBarIndex != -1 && indexToBar.contains(m_latestBarIndex))
                                               ? m_latestBarIndex
                                               : indexToBar.lastKey());
    }

    // Keep the same total recenter window width as before, but center it on the
    // current-time anchor so the now-line sits in the middle.
    const double recenterWindowBars = static_cast<double>(lookbackBars + rightPaddingBars);
    const double halfRecenterWindowBars = recenterWindowBars / 2.0;
    const double startIndex = xAnchorIndex - halfRecenterWindowBars;
    const double endIndex = xAnchorIndex + halfRecenterWindowBars;
    const int visibleStartIndex = static_cast<int>(std::floor(startIndex));
    const int visibleEndIndex = static_cast<int>(std::ceil(endIndex));

    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();
    bool hasBarsInVisibleWindow = false;

    for (auto it = indexToBar.constBegin(); it != indexToBar.constEnd(); ++it)
    {
        const int index = it.key();
        if (index < visibleStartIndex || index > visibleEndIndex)
        {
            continue;
        }

        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();
        if (status != Bar::BarStatus::Open && status != Bar::BarStatus::Closed)
        {
            continue;
        }

        hasBarsInVisibleWindow = true;
        minPrice = qMin(minPrice, bar.getLow());
        maxPrice = qMax(maxPrice, bar.getHigh());
    }

    std::optional<Bar> latestTradableBar;
    for (auto it = indexToBar.constEnd(); it != indexToBar.constBegin();)
    {
        --it;
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            latestTradableBar = bar;
            break;
        }
    }

    if (!latestTradableBar.has_value())
    {
        m_pendingRecenterToPriceAction = p_deferIfNoData;
        return false;
    }

    if (!hasBarsInVisibleWindow)
    {
        minPrice = latestTradableBar->getLow();
        maxPrice = latestTradableBar->getHigh();
    }

    if (includeBboInYRange && m_bestBidPrice.has_value() && m_bestAskPrice.has_value())
    {
        const double bestBid = m_bestBidPrice.value();
        const double bestAsk = m_bestAskPrice.value();
        if (bestBid > 0.0 && bestAsk > 0.0)
        {
            minPrice = qMin(minPrice, qMin(bestBid, bestAsk));
            maxPrice = qMax(maxPrice, qMax(bestBid, bestAsk));
        }
    }

    double anchorPrice = latestTradableBar->getClose();
    if (anchorPrice <= 0.0)
    {
        anchorPrice = qMax(maxPrice, ChartFocusConstants::MIN_RANGE_ABSOLUTE_DOLLARS);
    }

    const double minRange =
        qMax(anchorPrice * (minRangePercent / 100.0), ChartFocusConstants::MIN_RANGE_ABSOLUTE_DOLLARS);
    if ((maxPrice - minPrice) < minRange)
    {
        maxPrice = anchorPrice + (minRange / 2.0);
        minPrice = anchorPrice - (minRange / 2.0);
    }

    const double padding =
        qMax(anchorPrice * (anchorPaddingPercent / 100.0), (maxPrice - minPrice) * (yPaddingPercent / 100.0));

    m_customPlot->xAxis->setRange(startIndex, endIndex);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(qMax(0.0, minPrice - padding), maxPrice + padding);

    m_pendingRecenterToPriceAction = false;
    redrawLastPriceLine();
    refreshOpenPositionVisualsFromLatestBar();
    rescaleVolumeAxisToVisibleRange();
    if (m_currentTimeLine->visible())
    {
        updateCurrentTimeLine();
    }
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);

    return true;
}

/**
 * @brief Slot called when axis ranges change.
 */
void StockPriceChart::onAxisRangeChanged()
{
    if (m_isReplayNoDataState)
    {
        updateAxisLabelsDensity();
        if (m_currentTimeLine->visible())
        {
            QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
            double currentX = m_currentTimeLine->start->coords().x();
            m_currentTimeLine->start->setCoords(currentX, yRange.lower);
            m_currentTimeLine->end->setCoords(currentX, yRange.upper);
            updateBboOverlay();
            updateLevel2DepthOverlay();
        }
        updateBracketOverlayVisuals();
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    if (indexToBar.isEmpty() && !m_index0Timestamp.isValid())
        return;

    // Debounce: if a deferred update is already queued, just let it run.
    // This coalesces rapid successive rangeChanged signals (e.g. X and Y both
    // firing in a single wheel event, or the bidirectional X-axis sync round-trip).
    if (m_axisRangeChangePending)
        return;

    m_axisRangeChangePending = true;
    QTimer::singleShot(0,
                       this,
                       [this]()
                       {
                           m_axisRangeChangePending = false;

                           if (m_isReplayNoDataState)
                           {
                               updateAxisLabelsDensity();
                               if (m_currentTimeLine->visible())
                               {
                                   QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
                                   double currentX = m_currentTimeLine->start->coords().x();
                                   m_currentTimeLine->start->setCoords(currentX, yRange.lower);
                                   m_currentTimeLine->end->setCoords(currentX, yRange.upper);
                                   updateBboOverlay();
                                   updateLevel2DepthOverlay();
                               }
                               updateBracketOverlayVisuals();
                               m_customPlot->replot(QCustomPlot::rpQueuedReplot);
                               return;
                           }

                           updateAxisLabelsDensity();
                           redrawLastPriceLine();
                           refreshOpenPositionVisualsFromLatestBar();
                           rescaleVolumeAxisToVisibleRange();

                           // Update the current time line's Y coordinates to match new Y-axis range
                           if (m_currentTimeLine->visible())
                           {
                               QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
                               double currentX = m_currentTimeLine->start->coords().x();
                               m_currentTimeLine->start->setCoords(currentX, yRange.lower);
                               m_currentTimeLine->end->setCoords(currentX, yRange.upper);
                               updateBboOverlay();
                               updateLevel2DepthOverlay();
                           }
                           updateBracketOverlayVisuals();

                           // Note: Background rectangles are created once when bars are received,
                           // QCustomPlot handles clipping to visible range automatically.
                           // No need to recreate them on every axis change.

                           // Check for missing bars when view extends beyond available data
                           double minIndex = m_customPlot->xAxis->range().lower;
                           if (!indexToBar.isEmpty() && minIndex < indexToBar.firstKey())
                           {
                               QDateTime requestTime = getTimestampForIndex(static_cast<int>(minIndex));
                               checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
                           }
                           else if (indexToBar.isEmpty() && m_index0Timestamp.isValid())
                           {
                               // No bars loaded yet — request from view start to index 0
                               QDateTime requestTime = getTimestampForIndex(static_cast<int>(minIndex));
                               checkForMissingBars(requestTime, m_index0Timestamp);
                           }

                           // Auto-timescale switching
                           if (chartToolbar->isAutoTimeFrameEnabled())
                               checkAutoTimeFrame();

                           // The geometry above (price/time lines, overlays) was just recomputed but not yet
                           // painted — without this, stale visuals would linger until some unrelated replot.
                           m_customPlot->replot(QCustomPlot::rpQueuedReplot);
                       });
}

/**
 * @brief Slot called when volume chart visibility changes.
 */
void StockPriceChart::onVolumeChartVisibilityChanged(bool visible)
{
    if (m_volumeIndicator != nullptr)
    {
        m_volumeIndicator->setVisible(visible);
    }

    updateLowerPaneLayout();

    if (visible)
    {
        refreshIndicators();
        rescaleVolumeAxisToVisibleRange();
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

/**
 * @brief Slot called when volume settings change.
 */
void StockPriceChart::onVolumeSettingsChanged(const bool autoScaleEnabled, const int autoScaleMode)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_VOLUME_AUTO_SCALE_ENABLED, autoScaleEnabled);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_VOLUME_AUTO_SCALE_MODE, autoScaleMode);
    appStateSettings->sync();

    if (m_volumeIndicator != nullptr)
    {
        VolumeIndicator::Settings settings;
        settings.autoScaleEnabled = autoScaleEnabled;
        settings.autoScaleMode = (autoScaleMode == static_cast<int>(VolumeIndicator::AutoScaleMode::HighestBar))
                                     ? VolumeIndicator::AutoScaleMode::HighestBar
                                     : VolumeIndicator::AutoScaleMode::SecondHighestBar;
        m_volumeIndicator->setSettings(settings);
    }

    // Refresh the strip-local volume legend immediately for any settings change.
    rescaleVolumeAxisToVisibleRange();

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onOrderVisualizationsVisibilityChanged(bool visible)
{
    m_orderVisualizationsVisible = visible;
    if (!visible)
    {
        resetBracketWheelMode(QStringLiteral("order-visualizations-hidden"));
    }
    else
    {
        updateBracketWheelModeBadge();
    }
    updateOrderVisualizationsVisibility();
    m_customPlot->replot();
}

/**
 * @brief Handles widget resize events.
 */
void StockPriceChart::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_strategyStatusLabel != nullptr)
    {
        repositionStrategyStatusVisual();
    }
    onAxisRangeChanged();
}

/**
 * @brief Event filter to intercept events from child widgets.
 */
bool StockPriceChart::eventFilter(QObject* obj, QEvent* event)
{
    constexpr double BRACKET_DRAG_HIT_RADIUS_PX = 8.0;
    constexpr double BRACKET_MIN_TICK = 0.01;

    if (obj == m_customPlot && event->type() == QEvent::Wheel)
    {
        wheelEvent(static_cast<QWheelEvent*>(event));
        return true;
    }

    if (obj == m_customPlot && event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if (mouseEvent->button() == Qt::MiddleButton)
        {
            if (m_orderVisualizationsVisible && m_activeBracketOverlay.has_value())
            {
                cycleBracketWheelMode();
                event->accept();
                return true;
            }
            return QWidget::eventFilter(obj, event);
        }

        if (mouseEvent->button() == Qt::LeftButton && m_orderVisualizationsVisible &&
            m_activeBracketOverlay.has_value())
        {
            const auto& state = m_activeBracketOverlay.value();

            const auto labelHitDistance = [&](QCPItemText* p_label) -> double
            {
                if (p_label == nullptr || !p_label->visible())
                {
                    return std::numeric_limits<double>::infinity();
                }
                const double d = p_label->selectTest(mouseEvent->position(), false);
                return d >= 0.0 ? d : std::numeric_limits<double>::infinity();
            };

            const auto lineHitDistance = [&](QCPItemLine* p_line) -> double
            {
                if (p_line == nullptr || !p_line->visible())
                {
                    return std::numeric_limits<double>::infinity();
                }

                const QPointF startPx = p_line->start->pixelPosition();
                const QPointF endPx = p_line->end->pixelPosition();
                const QPointF mousePx = mouseEvent->position();

                // Don't allow grabbing by Y-only coincidence: click must be near the
                // visible horizontal span of the rendered bracket segment.
                const double minX = qMin(startPx.x(), endPx.x()) - BRACKET_DRAG_HIT_RADIUS_PX;
                const double maxX = qMax(startPx.x(), endPx.x()) + BRACKET_DRAG_HIT_RADIUS_PX;
                if (mousePx.x() < minX || mousePx.x() > maxX)
                {
                    return std::numeric_limits<double>::infinity();
                }

                const double dx = endPx.x() - startPx.x();
                const double dy = endPx.y() - startPx.y();
                const double segmentLenSq = dx * dx + dy * dy;
                if (segmentLenSq <= 1e-9)
                {
                    return QLineF(mousePx, startPx).length();
                }

                const double projection =
                    ((mousePx.x() - startPx.x()) * dx + (mousePx.y() - startPx.y()) * dy) / segmentLenSq;
                const double t = std::clamp(projection, 0.0, 1.0);
                const QPointF projectedPoint(startPx.x() + (t * dx), startPx.y() + (t * dy));
                return QLineF(mousePx, projectedPoint).length();
            };

            const double stopLabelDist = labelHitDistance(m_bracketStopLabel);
            const double takeLabelDist = labelHitDistance(m_bracketTakeLabel);
            const double stopLineDist = lineHitDistance(m_bracketStopLine);
            const double takeLineDist = lineHitDistance(m_bracketTakeLine);
            const bool hasLabelHit = (stopLabelDist <= BRACKET_DRAG_HIT_RADIUS_PX * 1.5) ||
                                     (takeLabelDist <= BRACKET_DRAG_HIT_RADIUS_PX * 1.5);

            BracketDragTarget target = BracketDragTarget::None;
            if (hasLabelHit)
            {
                target = (stopLabelDist <= takeLabelDist) ? BracketDragTarget::Stop : BracketDragTarget::Take;
            }
            else if (stopLineDist <= BRACKET_DRAG_HIT_RADIUS_PX || takeLineDist <= BRACKET_DRAG_HIT_RADIUS_PX)
            {
                target = (stopLineDist <= takeLineDist) ? BracketDragTarget::Stop : BracketDragTarget::Take;
            }

            if (target != BracketDragTarget::None)
            {
                m_bracketDragActive = true;
                m_bracketDragTarget = target;
                m_bracketDragOriginalStopPrice = state.stopPrice;
                m_bracketDragOriginalTakePrice = state.takePrice;
                m_interactionsBeforeBracketDrag = m_customPlot->interactions();
                m_customPlot->setInteractions(m_interactionsBeforeBracketDrag & ~QCP::iRangeDrag);
                m_customPlot->setCursor(Qt::SizeVerCursor);
                event->accept();
                return true;
            }
        }

        if (mouseEvent->button() == Qt::RightButton)
        {
            if (m_bracketDragActive && m_activeBracketOverlay.has_value())
            {
                m_activeBracketOverlay->stopPrice = m_bracketDragOriginalStopPrice;
                m_activeBracketOverlay->takePrice = m_bracketDragOriginalTakePrice;
                m_customPlot->setInteractions(m_interactionsBeforeBracketDrag);
                m_customPlot->unsetCursor();
                m_bracketDragActive = false;
                m_bracketDragTarget = BracketDragTarget::None;
                updateBracketOverlayVisuals();
                m_customPlot->replot(QCustomPlot::rpQueuedReplot);
                event->accept();
                return true;
            }

            const bool recentered = recenterToCurrentPriceAction(false);
            logInputEvent(u"StockPriceChart",
                          u"recenter-price-view",
                          {inputDetail(u"symbol", m_symbol), inputDetail(u"applied", recentered)});
            event->accept();
            return true;
        }
    }

    if (obj == m_customPlot && event->type() == QEvent::MouseButtonRelease)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_bracketDragActive)
        {
            if (m_activeBracketOverlay.has_value())
            {
                const bool stopChanged =
                    std::abs(m_activeBracketOverlay->stopPrice - m_bracketDragOriginalStopPrice) > 0.000001;
                const bool takeChanged =
                    std::abs(m_activeBracketOverlay->takePrice - m_bracketDragOriginalTakePrice) > 0.000001;
                if (stopChanged || takeChanged)
                {
                    if (m_activeBracketOverlay->isManualArmedPreview)
                    {
                        emit manualArmedBracketAdjusted(m_symbol,
                                                        m_activeBracketOverlay->stopPrice,
                                                        m_activeBracketOverlay->takePrice);
                    }
                    else
                    {
                        emit adjustManagedBracketRequested(m_symbol,
                                                           stopChanged,
                                                           m_activeBracketOverlay->stopPrice,
                                                           takeChanged,
                                                           m_activeBracketOverlay->takePrice);
                    }
                }
            }

            m_customPlot->setInteractions(m_interactionsBeforeBracketDrag);
            m_customPlot->unsetCursor();
            m_bracketDragActive = false;
            m_bracketDragTarget = BracketDragTarget::None;
            updateBracketOverlayVisuals();
            m_customPlot->replot(QCustomPlot::rpQueuedReplot);
            event->accept();
            return true;
        }
    }

    if (obj == m_customPlot && event->type() == QEvent::MouseMove)
    {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_bracketDragActive && m_activeBracketOverlay.has_value())
        {
            BracketOverlayState& state = m_activeBracketOverlay.value();
            QCPAxis* const priceAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
            double draggedPrice = priceAxis->pixelToCoord(me->position().y());
            if (!std::isfinite(draggedPrice))
            {
                return true;
            }
            draggedPrice = std::max(BRACKET_MIN_TICK, std::round(draggedPrice * 100.0) / 100.0);

            if (m_bracketDragTarget == BracketDragTarget::Stop)
            {
                state.stopPrice = draggedPrice;
            }
            else if (m_bracketDragTarget == BracketDragTarget::Take)
            {
                state.takePrice = draggedPrice;
            }

            if (state.side == StrategyBracketOverlayEntry::Side::Long && state.stopPrice >= state.takePrice)
            {
                if (m_bracketDragTarget == BracketDragTarget::Stop)
                {
                    state.stopPrice = std::max(BRACKET_MIN_TICK, state.takePrice - BRACKET_MIN_TICK);
                }
                else
                {
                    state.takePrice = state.stopPrice + BRACKET_MIN_TICK;
                }
            }
            if (state.side == StrategyBracketOverlayEntry::Side::Short && state.stopPrice <= state.takePrice)
            {
                if (m_bracketDragTarget == BracketDragTarget::Stop)
                {
                    state.stopPrice = state.takePrice + BRACKET_MIN_TICK;
                }
                else
                {
                    state.takePrice = std::max(BRACKET_MIN_TICK, state.stopPrice - BRACKET_MIN_TICK);
                }
            }

            updateBracketOverlayVisuals();
            m_customPlot->replot(QCustomPlot::rpQueuedReplot);
            event->accept();
            return true;
        }

        const QPointF mousePos = me->position();
        constexpr double HIT_RADIUS_PX = 12.0;

        bool hitFound = false;
        for (const HoverTarget& target: m_hoverTargets)
        {
            const QPointF itemPos = target.getPos();
            const double dist = QLineF(mousePos, itemPos).length();
            if (dist <= HIT_RADIUS_PX)
            {
                QToolTip::showText(me->globalPosition().toPoint(), target.tooltip, m_customPlot);
                hitFound = true;
                break;
            }
        }
        if (!hitFound)
        {
            QToolTip::hideText();
        }
        // Don't consume the event — let QCustomPlot handle panning etc.
    }

    return QWidget::eventFilter(obj, event);
}

/**
 * @brief Handles mouse wheel events for zooming.
 */
void StockPriceChart::wheelEvent(QWheelEvent* event)
{
    if (handleBracketWheelAdjustment(event))
    {
        event->accept();
        return;
    }

    qreal zoomFactor = event->angleDelta().y() > 0 ? (1.0 - 0.1 * wheelZoomRatio) : (1.0 + 0.1 * wheelZoomRatio);

    // Detect which axis rect the mouse is over
    // Handle Qt version differences: Qt 5.14+ uses position() instead of pos()
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    const QPointF mousePos = event->pos();
#else
    const QPointF mousePos = event->position();
#endif
    QCPAxisRect* axisRectUnderMouse = m_customPlot->axisRectAt(mousePos);
    bool isOverVolumeChart = (axisRectUnderMouse == m_volumeAxisRect);
    bool isOverMacdChart = (axisRectUnderMouse == m_macdAxisRect);
    bool isOverRsiChart = (axisRectUnderMouse == m_rsiAxisRect);

    if ((event->modifiers() & Qt::ShiftModifier) && (event->modifiers() & Qt::ControlModifier))
    {
        handleVerticalPanning(event);
    }
    else if (event->modifiers() & Qt::AltModifier)
    {
        handleHorizontalPanning(event);
    }
    else if (event->modifiers() & Qt::ControlModifier)
    {
        handleHorizontalZoom(event, zoomFactor);
    }
    else if (event->modifiers() & Qt::ShiftModifier)
    {
        handleVerticalZoom(event, isOverVolumeChart, isOverMacdChart, isOverRsiChart, zoomFactor);
    }
    else
    {
        handleBothAxesZoom(event, isOverVolumeChart, isOverMacdChart, isOverRsiChart, zoomFactor);
    }

    event->accept();
}

[[nodiscard]] bool StockPriceChart::canAdjustBracketTakeFromOverlay() const
{
    if (!m_orderVisualizationsVisible || !m_activeBracketOverlay.has_value())
    {
        return false;
    }

    const BracketOverlayState& state = m_activeBracketOverlay.value();
    if (state.referenceEntryPrice <= 0.0)
    {
        return false;
    }

    return true;
}

void StockPriceChart::updateBracketWheelModeBadge()
{
    if (m_bracketWheelModeBadge == nullptr)
    {
        return;
    }

    if (m_bracketWheelMode == BracketWheelMode::Normal || !m_orderVisualizationsVisible ||
        !m_activeBracketOverlay.has_value())
    {
        m_bracketWheelModeBadge->setVisible(false);
        return;
    }

    QString text;
    QColor textColor(220, 220, 220);
    if (m_bracketWheelMode == BracketWheelMode::Take)
    {
        text = QStringLiteral("WHEEL: TAKE");
        textColor = QColor(120, 230, 140);
    }
    else
    {
        text = QStringLiteral("WHEEL: STOP %");
        textColor = QColor(255, 205, 120);
    }

    m_bracketWheelModeBadge->setColor(textColor);
    m_bracketWheelModeBadge->setText(text);
    m_bracketWheelModeBadge->setVisible(true);
}

void StockPriceChart::setBracketWheelMode(const BracketWheelMode p_mode, const QString& p_reason)
{
    BracketWheelMode normalizedMode = p_mode;
    if (normalizedMode == BracketWheelMode::Take && !canAdjustBracketTakeFromOverlay())
    {
        normalizedMode = BracketWheelMode::Stop;
    }

    if (normalizedMode != BracketWheelMode::Normal &&
        (!m_orderVisualizationsVisible || !m_activeBracketOverlay.has_value()))
    {
        normalizedMode = BracketWheelMode::Normal;
    }
    if (normalizedMode != BracketWheelMode::Normal && m_activeBracketOverlay.has_value() &&
        m_activeBracketOverlay->referenceEntryPrice <= 0.0)
    {
        normalizedMode = BracketWheelMode::Normal;
    }

    if (m_bracketWheelMode == normalizedMode)
    {
        updateBracketWheelModeBadge();
        return;
    }

    m_bracketWheelMode = normalizedMode;
    updateBracketWheelModeBadge();

    QString modeText = QStringLiteral("normal");
    if (normalizedMode == BracketWheelMode::Take)
    {
        modeText = QStringLiteral("take");
    }
    else if (normalizedMode == BracketWheelMode::Stop)
    {
        modeText = QStringLiteral("stop");
    }

    QStringList details{inputDetail(u"symbol", m_symbol), inputDetail(u"mode", modeText)};
    if (!p_reason.isEmpty())
    {
        details.append(inputDetail(u"reason", p_reason));
    }
    logInputEvent(u"StockPriceChart", u"set-bracket-wheel-mode", details);
}

void StockPriceChart::resetBracketWheelMode(const QString& reason)
{
    setBracketWheelMode(BracketWheelMode::Normal, reason.isEmpty() ? QStringLiteral("reset") : reason);
}

void StockPriceChart::cycleBracketWheelMode()
{
    if (!m_orderVisualizationsVisible || !m_activeBracketOverlay.has_value())
    {
        resetBracketWheelMode(QStringLiteral("cycle-no-overlay"));
        return;
    }

    BracketWheelMode nextMode = BracketWheelMode::Normal;
    switch (m_bracketWheelMode)
    {
    case BracketWheelMode::Normal:
        nextMode = BracketWheelMode::Stop;
        break;
    case BracketWheelMode::Stop:
        nextMode = canAdjustBracketTakeFromOverlay() ? BracketWheelMode::Take : BracketWheelMode::Normal;
        break;
    case BracketWheelMode::Take:
        nextMode = BracketWheelMode::Normal;
        break;
    }

    setBracketWheelMode(nextMode, QStringLiteral("middle-click"));
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

[[nodiscard]] bool StockPriceChart::handleBracketWheelAdjustment(QWheelEvent* event)
{
    if (m_bracketWheelMode == BracketWheelMode::Normal)
    {
        return false;
    }

    if (!m_orderVisualizationsVisible || !m_activeBracketOverlay.has_value())
    {
        resetBracketWheelMode(QStringLiteral("wheel-no-overlay"));
        return false;
    }

    BracketOverlayState& state = m_activeBracketOverlay.value();
    const double entryPrice = state.referenceEntryPrice;
    if (entryPrice <= 0.0)
    {
        resetBracketWheelMode(QStringLiteral("wheel-missing-entry-reference"));
        return false;
    }

    const int deltaY = event->angleDelta().y();
    if (deltaY == 0)
    {
        // While in bracket wheel mode we consume wheel events to prevent chart zoom/pan.
        return true;
    }

    constexpr double MIN_TICK = 0.01;
    const auto roundToCent = [](const double p_value) { return std::round(p_value * 100.0) / 100.0; };

    Q_CHECK_PTR(appStateSettings);
    const double ratioStep = std::clamp(
        appStateSettings
            ->value(BracketWheelConstants::SETTINGS_KEY_RATIO_STEP_R, BracketWheelConstants::DEFAULT_RATIO_STEP_R)
            .toDouble(),
        BracketWheelConstants::MIN_RATIO_STEP_R,
        BracketWheelConstants::MAX_RATIO_STEP_R);
    const double stopStepPercent = std::clamp(appStateSettings
                                                  ->value(BracketWheelConstants::SETTINGS_KEY_STOP_STEP_PERCENT,
                                                          BracketWheelConstants::DEFAULT_STOP_STEP_PERCENT)
                                                  .toDouble(),
                                              BracketWheelConstants::MIN_STOP_STEP_PERCENT,
                                              BracketWheelConstants::MAX_STOP_STEP_PERCENT);

    double ratioMin = std::clamp(
        appStateSettings->value(BracketWheelConstants::SETTINGS_KEY_RATIO_MIN, BracketWheelConstants::DEFAULT_RATIO_MIN)
            .toDouble(),
        BracketWheelConstants::MIN_RATIO_BOUND,
        BracketWheelConstants::MAX_RATIO_BOUND);
    double ratioMax = std::clamp(
        appStateSettings->value(BracketWheelConstants::SETTINGS_KEY_RATIO_MAX, BracketWheelConstants::DEFAULT_RATIO_MAX)
            .toDouble(),
        BracketWheelConstants::MIN_RATIO_BOUND,
        BracketWheelConstants::MAX_RATIO_BOUND);
    if (ratioMin > ratioMax)
    {
        std::swap(ratioMin, ratioMax);
    }

    double stopPctMin = std::clamp(appStateSettings
                                       ->value(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MIN,
                                               BracketWheelConstants::DEFAULT_STOP_PERCENT_MIN)
                                       .toDouble(),
                                   BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                   BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    double stopPctMax = std::clamp(appStateSettings
                                       ->value(BracketWheelConstants::SETTINGS_KEY_STOP_PERCENT_MAX,
                                               BracketWheelConstants::DEFAULT_STOP_PERCENT_MAX)
                                       .toDouble(),
                                   BracketWheelConstants::MIN_STOP_PERCENT_BOUND,
                                   BracketWheelConstants::MAX_STOP_PERCENT_BOUND);
    if (stopPctMin > stopPctMax)
    {
        std::swap(stopPctMin, stopPctMax);
    }

    double wheelSteps = static_cast<double>(deltaY) / 120.0;
    if (std::abs(wheelSteps) < 0.01)
    {
        wheelSteps = deltaY > 0 ? 1.0 : -1.0;
    }

    const auto enforceNoCross = [&](double& p_stop, double& p_take)
    {
        if (state.side == StrategyBracketOverlayEntry::Side::Long && p_stop >= p_take)
        {
            p_take = p_stop + MIN_TICK;
        }
        if (state.side == StrategyBracketOverlayEntry::Side::Short && p_stop <= p_take)
        {
            p_take = std::max(MIN_TICK, p_stop - MIN_TICK);
        }
    };

    const double oldStopPrice = state.stopPrice;
    const double oldTakePrice = state.takePrice;

    const double currentRiskAbs = std::max(MIN_TICK, std::abs(entryPrice - state.stopPrice));
    const double currentRatio = std::clamp(std::abs(state.takePrice - entryPrice) / currentRiskAbs, ratioMin, ratioMax);

    bool stopChanged = false;
    bool takeChanged = false;

    if (m_bracketWheelMode == BracketWheelMode::Take)
    {
        const double newRatio = std::clamp(currentRatio + (ratioStep * wheelSteps), ratioMin, ratioMax);
        if (std::abs(newRatio - currentRatio) < 0.0000001)
        {
            return true;
        }

        double takePrice = (state.side == StrategyBracketOverlayEntry::Side::Long)
                               ? entryPrice + (currentRiskAbs * newRatio)
                               : entryPrice - (currentRiskAbs * newRatio);
        takePrice = std::max(MIN_TICK, roundToCent(takePrice));

        enforceNoCross(state.stopPrice, takePrice);
        state.takePrice = takePrice;
        takeChanged = std::abs(state.takePrice - oldTakePrice) > 0.000001;
    }
    else if (m_bracketWheelMode == BracketWheelMode::Stop)
    {
        const double currentStopPercent = std::clamp((currentRiskAbs / entryPrice) * 100.0, stopPctMin, stopPctMax);
        const double newStopPercent =
            std::clamp(currentStopPercent - (stopStepPercent * wheelSteps), stopPctMin, stopPctMax);
        if (std::abs(newStopPercent - currentStopPercent) < 0.0000001)
        {
            return true;
        }

        double stopPrice = (state.side == StrategyBracketOverlayEntry::Side::Long)
                               ? entryPrice * (1.0 - (newStopPercent / 100.0))
                               : entryPrice * (1.0 + (newStopPercent / 100.0));
        stopPrice = std::max(MIN_TICK, roundToCent(stopPrice));

        const double newRiskAbs = std::max(MIN_TICK, std::abs(entryPrice - stopPrice));
        double takePrice = (state.side == StrategyBracketOverlayEntry::Side::Long)
                               ? entryPrice + (newRiskAbs * currentRatio)
                               : entryPrice - (newRiskAbs * currentRatio);
        takePrice = std::max(MIN_TICK, roundToCent(takePrice));

        enforceNoCross(stopPrice, takePrice);
        state.stopPrice = stopPrice;
        state.takePrice = takePrice;
        stopChanged = std::abs(state.stopPrice - oldStopPrice) > 0.000001;
        takeChanged = std::abs(state.takePrice - oldTakePrice) > 0.000001;
    }
    else
    {
        return false;
    }

    if (!stopChanged && !takeChanged)
    {
        return true;
    }

    updateBracketOverlayVisuals();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);

    if (state.isManualArmedPreview)
    {
        emit manualArmedBracketAdjusted(m_symbol, state.stopPrice, state.takePrice);
    }
    else
    {
        emit adjustManagedBracketRequested(m_symbol, stopChanged, state.stopPrice, takeChanged, state.takePrice);
    }

    return true;
}

void StockPriceChart::handleVerticalPanning(QWheelEvent* event)
{
    DEBUG << "Vertical panning with wheel";

    QCPRange range = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    qreal priceRange = range.size();

    qreal shiftAmount = priceRange * 0.03;
    if (event->angleDelta().y() < 0)
    {
        shiftAmount = -shiftAmount;
    }

    m_customPlot->axisRect()
        ->axis(QCPAxis::atRight)
        ->setRange(qMax(0.0, range.lower + shiftAmount), range.upper + shiftAmount);
    // atRight::rangeChanged is connected to onAxisRangeChanged(), which owns the repaint
    // after recomputing line/overlay geometry — see handleHorizontalPanning() below.
}


void StockPriceChart::handleHorizontalPanning(QWheelEvent* event)
{
    DEBUG << "Horizontal panning with wheel";

    QCPRange range = m_customPlot->xAxis->range();
    qreal indexRange = range.size();
    qreal shiftAmount = indexRange * 0.05;
    shiftAmount = -shiftAmount * (event->angleDelta().x() > 0 ? 1 : -1);

    qreal newMin = range.lower + shiftAmount;
    qreal newMax = range.upper + shiftAmount;

    if (!indexToBar.isEmpty() && newMin < indexToBar.firstKey())
    {
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
    }

    // xAxis::rangeChanged is connected to onAxisRangeChanged(), which recomputes the
    // price/time-line geometry and overlays before repainting. Replotting here too would
    // paint one frame of stale geometry first, causing a visible flicker.
    m_customPlot->xAxis->setRange(newMin, newMax);
}

void StockPriceChart::handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor)
{
    Q_UNUSED(event);

    QCPRange range = m_customPlot->xAxis->range();
    qreal centerIndex = range.center();

    qreal newSize = range.size() * zoomFactor;
    qreal newMin = centerIndex - (newSize / 2);
    qreal newMax = centerIndex + (newSize / 2);

    if (!indexToBar.isEmpty() && newMin < indexToBar.firstKey())
    {
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
    }

    // See handleHorizontalPanning() above: onAxisRangeChanged() owns the repaint here.
    m_customPlot->xAxis->setRange(newMin, newMax);
}

void StockPriceChart::handleVerticalZoom(QWheelEvent* event,
                                         const bool isOverVolumeChart,
                                         const bool isOverMacdChart,
                                         const bool isOverRsiChart,
                                         const qreal zoomFactor)
{
    Q_UNUSED(event);

    if (isOverVolumeChart)
    {
        // When over volume chart, only zoom the volume Y axis
        // Keep lower bound at 0, only adjust upper bound
        QCPRange range = m_volumeAxisRect->axis(QCPAxis::atRight)->range();
        qreal newMax = range.upper * zoomFactor;

        // Volume/MACD/RSI Y-axes aren't wired to onAxisRangeChanged(), so they must repaint
        // themselves here.
        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else if (isOverMacdChart)
    {
        // When over MACD chart, only zoom MACD Y-axis
        QCPRange range = m_macdAxisRect->axis(QCPAxis::atRight)->range();
        const qreal center = range.center();

        const qreal newSize = range.size() * zoomFactor;
        const qreal newMin = center - (newSize / 2);
        const qreal newMax = center + (newSize / 2);

        m_macdAxisRect->axis(QCPAxis::atRight)->setRange(newMin, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else if (isOverRsiChart)
    {
        // When over RSI chart, only zoom RSI Y-axis
        QCPRange range = m_rsiAxisRect->axis(QCPAxis::atRight)->range();
        const qreal center = range.center();
        const qreal newSize = range.size() * zoomFactor;
        const qreal newMin = center - (newSize / 2);
        const qreal newMax = center + (newSize / 2);

        m_rsiAxisRect->axis(QCPAxis::atRight)->setRange(newMin, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else
    {
        // When over price chart, only zoom the price Y axis. Its rangeChanged is wired to
        // onAxisRangeChanged(), which owns the repaint after recomputing line/overlay geometry.
        QCPRange range = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
        qreal center = range.center();

        qreal newSize = range.size() * zoomFactor;
        qreal newMin = center - (newSize / 2);
        qreal newMax = center + (newSize / 2);

        m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(qMax(0.0, newMin), newMax);
    }
}

void StockPriceChart::handleBothAxesZoom(QWheelEvent* event,
                                         const bool isOverVolumeChart,
                                         const bool isOverMacdChart,
                                         const bool isOverRsiChart,
                                         const qreal zoomFactor)
{
    Q_UNUSED(event);

    if (isOverVolumeChart)
    {
        // When over volume chart, only zoom Y axis of volume chart
        // Keep lower bound at 0, only adjust upper bound
        QCPRange range = m_volumeAxisRect->axis(QCPAxis::atRight)->range();
        qreal newMax = range.upper * zoomFactor;

        // Volume/MACD/RSI Y-axes aren't wired to onAxisRangeChanged(), so they must repaint
        // themselves here.
        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else if (isOverMacdChart)
    {
        // When over MACD chart, only zoom MACD Y axis
        QCPRange range = m_macdAxisRect->axis(QCPAxis::atRight)->range();
        const qreal center = range.center();
        const qreal newSize = range.size() * zoomFactor;
        const qreal newMin = center - (newSize / 2);
        const qreal newMax = center + (newSize / 2);
        m_macdAxisRect->axis(QCPAxis::atRight)->setRange(newMin, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else if (isOverRsiChart)
    {
        // When over RSI chart, only zoom RSI Y axis
        QCPRange range = m_rsiAxisRect->axis(QCPAxis::atRight)->range();
        const qreal center = range.center();
        const qreal newSize = range.size() * zoomFactor;
        const qreal newMin = center - (newSize / 2);
        const qreal newMax = center + (newSize / 2);
        m_rsiAxisRect->axis(QCPAxis::atRight)->setRange(newMin, newMax);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    else
    {
        // When over price chart, zoom both axes. Both are wired to onAxisRangeChanged(),
        // which owns the repaint after recomputing line/overlay geometry.
        // X-axis zoom (this will automatically transfer to volume chart via connected signals)
        QCPRange xRange = m_customPlot->xAxis->range();
        qreal centerIndex = xRange.center();
        qreal newXSize = xRange.size() * zoomFactor;
        qreal newMinX = centerIndex - (newXSize / 2);
        qreal newMaxX = centerIndex + (newXSize / 2);

        if (!indexToBar.isEmpty() && newMinX < indexToBar.firstKey())
        {
            QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMinX));
            checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
        }

        // Y-axis zoom (only for price chart)
        QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
        qreal centerPrice = yRange.center();
        qreal newYSize = yRange.size() * zoomFactor;
        qreal newMinY = centerPrice - (newYSize / 2);
        qreal newMaxY = centerPrice + (newYSize / 2);

        m_customPlot->xAxis->setRange(newMinX, newMaxX);
        m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(qMax(0.0, newMinY), newMaxY);
    }
}

/**
 * @brief Checks whether the auto-timescale logic should switch to a different TimeFrame
 *        based on how many bars are currently visible in the chart x-axis range.
 *
 * Thresholds (with built-in hysteresis band):
 *   visible > 200  →  switch to the next coarser TimeFrame
 *   visible <  60  →  switch to the next finer   TimeFrame
 *   60 ≤ visible ≤ 200  →  keep the current TimeFrame
 *
 * The gap between 60 and 200 is the hysteresis band that prevents rapid oscillation:
 * switching up to a coarser TF instantly halves (or more) the visible bar count,
 * landing it comfortably inside the band.
 */
void StockPriceChart::checkAutoTimeFrame()
{
    // Don't auto-switch while the chart is empty — the reset range (0,30) after clearChart
    // would immediately trigger a "too few bars" downgrade back to 1m.
    if (indexToBar.isEmpty())
        return;

    static const QVector<TimeFrame> TF_ORDER = {TimeFrame::TEN_SECONDS,
                                                TimeFrame::ONE_MINUTE,
                                                TimeFrame::FIVE_MINUTES,
                                                TimeFrame::FIFTEEN_MINUTES,
                                                TimeFrame::THIRTY_MINUTES,
                                                TimeFrame::ONE_HOUR,
                                                TimeFrame::FOUR_HOURS,
                                                TimeFrame::ONE_DAY,
                                                TimeFrame::ONE_WEEK,
                                                TimeFrame::ONE_MONTH};

    // Convert the x-axis range (in index slots) to minutes for threshold comparison.
    // For 10s TF: each slot = 10 seconds → multiply by 10/60 to get minutes.
    // For minute+ TFs: each slot = 1 minute (no conversion needed).
    const TimeFrame currentTf = chartToolbar->getCurrentTimeFrame();
    const double slotsVisible = m_customPlot->xAxis->range().size();
    const int minutesVisible =
        static_cast<int>(slotsVisible * static_cast<double>(BarUtils::secondsPerBar(currentTf)) / 60.0);

    // Read thresholds from settings (with defaults) for each timeframe.
    // These define the ideal visible time range for each timeframe.
    // When we exceed the upper threshold, switch to a coarser TF.
    // When we go below the lower threshold, switch to a finer TF.
    auto getThresholdsForTimeFrame = [](TimeFrame tf) -> std::pair<int, int>
    {
        // Default thresholds (lower, upper) in minutes
        static const QMap<TimeFrame, std::pair<int, int>> defaults = {
            {TimeFrame::TEN_SECONDS, {0, 30}},
            {TimeFrame::ONE_MINUTE, {30, 150}},
            {TimeFrame::FIVE_MINUTES, {120, 480}},
            {TimeFrame::FIFTEEN_MINUTES, {240, 960}},
            {TimeFrame::THIRTY_MINUTES, {480, 1440}},
            {TimeFrame::ONE_HOUR, {720, 2880}},
            {TimeFrame::FOUR_HOURS, {1440, 10080}},
        };

        if (!defaults.contains(tf))
            return {0, INT_MAX}; // Don't auto-switch for daily+

        auto defaultVals = defaults.value(tf);
        QString tfKey = timeFrameToString(tf);

        Q_CHECK_PTR(appStateSettings);
        int lower = appStateSettings->value("Config/AutoTF/" + tfKey + "/Lower", defaultVals.first).toInt();
        int upper = appStateSettings->value("Config/AutoTF/" + tfKey + "/Upper", defaultVals.second).toInt();

        return {lower, upper};
    };

    const int currentIdx = TF_ORDER.indexOf(currentTf);
    auto [lowerMins, upperMins] = getThresholdsForTimeFrame(currentTf);

    TimeFrame targetTf = currentTf;
    if (minutesVisible > upperMins && currentIdx < TF_ORDER.size() - 1)
        targetTf = TF_ORDER[currentIdx + 1];
    else if (minutesVisible < lowerMins && currentIdx > 0)
        targetTf = TF_ORDER[currentIdx - 1];

    if (targetTf != currentTf)
    {
        DEBUG << "Auto-TF: switching from" << timeFrameToString(currentTf) << "to" << timeFrameToString(targetTf)
              << "(visible range:" << minutesVisible << "min, thresholds:" << lowerMins << "-" << upperMins << ")";

        // Update the toolbar visually without triggering its signal
        // (setCurrentTimeFrame would normally emit timeFrameChanged via onComboBoxChanged)
        {
            const QSignalBlocker blocker(chartToolbar);
            chartToolbar->setCurrentTimeFrame(targetTf);
        }
        // Now emit manually after the blocker is destroyed
        emit chartToolbar->timeFrameChanged(targetTf);
    }
}
