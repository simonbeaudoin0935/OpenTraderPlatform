#include "StockPriceChart.h"
#include "Logging.h"

#define LOGGING_CATEGORY ChartLog

/**
 * @brief Slot called when axis ranges change.
 */
void StockPriceChart::onAxisRangeChanged()
{
    if (indexToBar.isEmpty() && !m_index0Timestamp.isValid())
        return;

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
