#include "StockPriceChart.h"
#include "BarUtils.h"
#include "Logging.h"
#include <QToolTip>

#define LOGGING_CATEGORY ChartLog

/**
 * @brief Slot called when axis ranges change.
 */
void StockPriceChart::onAxisRangeChanged()
{
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

                           updateAxisLabelsDensity();
                           redrawLastPriceLine();
                           rescaleVolumeAxisToVisibleRange();

                           // Update the current time line's Y coordinates to match new Y-axis range
                           if (m_currentTimeLine->visible())
                           {
                               QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
                               double currentX = m_currentTimeLine->start->coords().x();
                               m_currentTimeLine->start->setCoords(currentX, yRange.lower);
                               m_currentTimeLine->end->setCoords(currentX, yRange.upper);
                           }

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
                       });
}

/**
 * @brief Slot called when volume chart visibility changes.
 */
void StockPriceChart::onVolumeChartVisibilityChanged(bool visible)
{
    if (visible)
    {
        // Show volume chart
        m_volumeAxisRect->setMinimumSize(QSize(0, 0));
        m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX,
                                               100)); // Restore height
        m_volumeAxisRect->setVisible(true);
        // Restore spacing and margins
        m_customPlot->plotLayout()->setRowSpacing(0);
        m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
        m_volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
    }
    else
    {
        // Hide volume chart by collapsing its height
        m_volumeAxisRect->setMinimumSize(QSize(0, 0));
        m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX,
                                               0)); // Collapse height
        m_volumeAxisRect->setVisible(false);
    }

    // Force layout update
    m_customPlot->plotLayout()->updateLayout();
    // Replot to update layout
    m_customPlot->replot();
}

/**
 * @brief Slot called when volume auto-rescale state changes.
 */
void StockPriceChart::onVolumeAutoRescaleChanged(bool enabled)
{
    m_volumeAutoRescaleEnabled = enabled;

    if (enabled)
    {
        // Immediately rescale to visible range
        rescaleVolumeAxisToVisibleRange();
        m_customPlot->replot();
    }
}

void StockPriceChart::onOrderVisualizationsVisibilityChanged(bool visible)
{
    m_orderVisualizationsVisible = visible;
    updateOrderVisualizationsVisibility();
    m_customPlot->replot();
}

/**
 * @brief Handles widget resize events.
 */
void StockPriceChart::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    onAxisRangeChanged();
}

/**
 * @brief Event filter to intercept events from child widgets.
 */
bool StockPriceChart::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_customPlot && event->type() == QEvent::Wheel)
    {
        wheelEvent(static_cast<QWheelEvent*>(event));
        return true;
    }

    if (obj == m_customPlot && event->type() == QEvent::MouseMove)
    {
        auto* me = static_cast<QMouseEvent*>(event);
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
        handleVerticalZoom(event, isOverVolumeChart, zoomFactor);
    }
    else
    {
        handleBothAxesZoom(event, isOverVolumeChart, zoomFactor);
    }

    event->accept();
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
    m_customPlot->replot();
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

    m_customPlot->xAxis->setRange(newMin, newMax);
    m_customPlot->replot();
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

    m_customPlot->xAxis->setRange(newMin, newMax);
    m_customPlot->replot();
}

void StockPriceChart::handleVerticalZoom(QWheelEvent* event, bool isOverVolumeChart, qreal zoomFactor)
{
    Q_UNUSED(event);

    if (isOverVolumeChart)
    {
        // When over volume chart, only zoom the volume Y axis
        // Keep lower bound at 0, only adjust upper bound
        QCPRange range = m_volumeAxisRect->axis(QCPAxis::atRight)->range();
        qreal newMax = range.upper * zoomFactor;

        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, newMax);
    }
    else
    {
        // When over price chart, only zoom the price Y axis
        QCPRange range = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
        qreal center = range.center();

        qreal newSize = range.size() * zoomFactor;
        qreal newMin = center - (newSize / 2);
        qreal newMax = center + (newSize / 2);

        m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(qMax(0.0, newMin), newMax);
    }
    m_customPlot->replot();
}

void StockPriceChart::handleBothAxesZoom(QWheelEvent* event, bool isOverVolumeChart, qreal zoomFactor)
{
    Q_UNUSED(event);

    if (isOverVolumeChart)
    {
        // When over volume chart, only zoom Y axis of volume chart
        // Keep lower bound at 0, only adjust upper bound
        QCPRange range = m_volumeAxisRect->axis(QCPAxis::atRight)->range();
        qreal newMax = range.upper * zoomFactor;

        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, newMax);
    }
    else
    {
        // When over price chart, zoom both axes
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
    m_customPlot->replot();
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

    static const QVector<TimeFrame> TF_ORDER = {TimeFrame::ONE_MINUTE,
                                                TimeFrame::FIVE_MINUTES,
                                                TimeFrame::FIFTEEN_MINUTES,
                                                TimeFrame::THIRTY_MINUTES,
                                                TimeFrame::ONE_HOUR,
                                                TimeFrame::FOUR_HOURS,
                                                TimeFrame::ONE_DAY,
                                                TimeFrame::ONE_WEEK,
                                                TimeFrame::ONE_MONTH};

    const int minutesVisible = static_cast<int>(m_customPlot->xAxis->range().size());

    // Thresholds in MINUTES (not bars) to avoid oscillation when switching timeframes.
    // These define the ideal visible time range for each timeframe.
    // When we exceed the upper threshold, switch to a coarser TF.
    // When we go below the lower threshold, switch to a finer TF.
    auto getThresholdsForTimeFrame = [](TimeFrame tf) -> std::pair<int, int>
    {
        // Returns {lowerMinutes, upperMinutes} - the comfortable range for this TF
        switch (tf)
        {
        case TimeFrame::ONE_MINUTE:
            return {30, 150}; // 30min to 2.5h feels good at 1m
        case TimeFrame::FIVE_MINUTES:
            return {120, 480}; // 2h to 8h feels good at 5m
        case TimeFrame::FIFTEEN_MINUTES:
            return {240, 960}; // 4h to 16h feels good at 15m
        case TimeFrame::THIRTY_MINUTES:
            return {480, 1440}; // 8h to 24h feels good at 30m
        case TimeFrame::ONE_HOUR:
            return {720, 2880}; // 12h to 2 days feels good at 1h
        case TimeFrame::FOUR_HOURS:
            return {1440, 10080}; // 1 day to 1 week feels good at 4h
        default:
            return {0, INT_MAX}; // Don't auto-switch for daily+
        }
    };

    const TimeFrame currentTf = chartToolbar->getCurrentTimeFrame();
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
        // Update the toolbar (blocks signals to prevent double-trigger, then emits manually)
        const QSignalBlocker blocker(chartToolbar);
        chartToolbar->setCurrentTimeFrame(targetTf);
        // Re-emit so GUIFrontend receives the change
        emit chartToolbar->timeFrameChanged(targetTf);
    }
}
