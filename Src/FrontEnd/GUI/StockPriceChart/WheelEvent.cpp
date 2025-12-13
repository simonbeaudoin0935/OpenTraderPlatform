#include "StockPriceChart.h"

/**
 * @brief Handles mouse wheel events for chart interaction.
 * 
 * Provides different zoom and pan behaviors based on modifier keys:
 * - Ctrl+Shift: Vertical panning
 * - Alt: Horizontal panning  
 * - Ctrl: Horizontal zooming
 * - Shift: Vertical zooming
 * - No modifiers: Both axes zooming
 * 
 * @param event The QWheelEvent containing wheel movement information.
 */
void StockPriceChart::wheelEvent(QWheelEvent* event) {
    if (!chartView->rect().contains(event->position().toPoint())) {
        event->ignore();
        return;
    }

    // Calculate zoom factor based on scroll direction
    qreal zoomFactor = event->angleDelta().y() > 0 ? 0.9 : 1.1;

    if ((event->modifiers() & Qt::ShiftModifier) && (event->modifiers() & Qt::ControlModifier)) {
        handleVerticalPanning(event);
    } else if (event->modifiers() & Qt::AltModifier) {
        handleHorizontalPanning(event);
    } else if (event->modifiers() & Qt::ControlModifier) {
        handleHorizontalZoom(event, zoomFactor);
    } else if (event->modifiers() & Qt::ShiftModifier) {
        handleVerticalZoom(event, zoomFactor);
    } else {
        handleBothAxesZoom(event, zoomFactor);
    }

    // Update the price label position
    updatePriceLabelPosition();
    event->accept();
}



/**
 * @brief Handles vertical panning with mouse wheel.
 * 
 * Pans the Y-axis (price) up or down based on wheel direction.
 * Used when Ctrl+Shift modifiers are held.
 * 
 * @param event The QWheelEvent containing wheel movement information.
 */
void StockPriceChart::handleVerticalPanning(QWheelEvent* event) {
    qreal currentMin = axisY->min();
    qreal currentMax = axisY->max();
    qreal priceRange = currentMax - currentMin;

    qreal shiftAmount = priceRange * 0.05;
    if (event->angleDelta().y() < 0) {
        shiftAmount = -shiftAmount;
    }

    axisY->setRange(qMax(0.0, currentMin + shiftAmount), currentMax + shiftAmount);
    updateLastPriceLineIfNeeded();
}

/**
 * @brief Handles horizontal panning with mouse wheel.
 * 
 * Pans the X-axis (time) left or right based on wheel direction.
 * Checks for missing bars when panning to earlier times.
 * Used when Alt modifier is held.
 * 
 * @param event The QWheelEvent containing wheel movement information.
 */
void StockPriceChart::handleHorizontalPanning(QWheelEvent* event) {
    qreal currentMin = axisX->min();
    qreal currentMax = axisX->max();
    qreal indexRange = currentMax - currentMin;
    qreal shiftAmount = indexRange * 0.05;
    shiftAmount = -shiftAmount * (event->angleDelta().x() > 0 ? 1 : -1);

    qreal newMin = currentMin + shiftAmount;
    qreal newMax = currentMax + shiftAmount;
    
    // Check for missing bars when panning beyond the first available bar
    if (!completedBars.isEmpty() && !indexToTimestamp.isEmpty()) {
        int firstAvailableIndex = indexToTimestamp.firstKey();
        if (newMin < firstAvailableIndex) {
            QDateTime firstBarTime = completedBars.firstKey();
            QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
            checkForMissingBars(requestTime, firstBarTime);
        }
    }
    
    axisX->setRange(newMin, newMax);
    updateLastPriceLineIfNeeded();
    updateAxisLabels();
}

/**
 * @brief Handles horizontal zooming with mouse wheel.
 * 
 * Zooms in/out on the X-axis (time) centered on current view.
 * Checks for missing bars when zooming out to earlier times.
 * Used when Ctrl modifier is held.
 * 
 * @param event The QWheelEvent containing wheel movement information.
 * @param zoomFactor The zoom multiplier (typically 0.9 for zoom in, 1.1 for zoom out).
 */
void StockPriceChart::handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    qreal currentMin = axisX->min();
    qreal currentMax = axisX->max();
    qreal indexRange = currentMax - currentMin;
    qreal centerIndex = currentMin + (indexRange / 2);

    qreal newIndexRange = indexRange * zoomFactor;
    qreal newMin = centerIndex - (newIndexRange / 2);
    qreal newMax = centerIndex + (newIndexRange / 2);
    
    // Check for missing bars when zooming beyond the first available bar
    if (!completedBars.isEmpty() && !indexToTimestamp.isEmpty()) {
        int firstAvailableIndex = indexToTimestamp.firstKey();
        if (newMin < firstAvailableIndex) {
            QDateTime firstBarTime = completedBars.firstKey();
            QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
            checkForMissingBars(requestTime, firstBarTime);
        }
    }

    axisX->setRange(newMin, newMax);
    updateLastPriceLineIfNeeded();
    updateAxisLabels();
}

/**
 * @brief Handles vertical zooming with mouse wheel.
 * 
 * Zooms in/out on the Y-axis (price) centered on current view.
 * Used when Shift modifier is held.
 * 
 * @param event The QWheelEvent containing wheel movement information.
 * @param zoomFactor The zoom multiplier (typically 0.9 for zoom in, 1.1 for zoom out).
 */
void StockPriceChart::handleVerticalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    qreal currentMin = axisY->min();
    qreal currentMax = axisY->max();
    qreal range = currentMax - currentMin;
    qreal center = (currentMax + currentMin) / 2;

    qreal newRange = range * zoomFactor;
    qreal newMin = center - (newRange / 2);
    qreal newMax = center + (newRange / 2);

    axisY->setRange(qMax(0.0, newMin), newMax);
    updateLastPriceLineIfNeeded();
}

/**
 * @brief Handles simultaneous zooming on both axes with mouse wheel.
 * 
 * Zooms in/out on both X-axis (time) and Y-axis (price) centered on current view.
 * Checks for missing bars when zooming out on time axis.
 * Used with no modifier keys held.
 * 
 * @param event The QWheelEvent containing wheel movement information.
 * @param zoomFactor The zoom multiplier (typically 0.9 for zoom in, 1.1 for zoom out).
 */
void StockPriceChart::handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    // Index axis zoom
    qreal currentMin = axisX->min();
    qreal currentMax = axisX->max();
    qreal indexRange = currentMax - currentMin;
    qreal centerIndex = currentMin + (indexRange / 2);

    qreal newIndexRange = indexRange * zoomFactor;
    qreal newMin = centerIndex - (newIndexRange / 2);
    qreal newMax = centerIndex + (newIndexRange / 2);
    
    // Check for missing bars when zooming beyond the first available bar
    if (!completedBars.isEmpty() && !indexToTimestamp.isEmpty()) {
        int firstAvailableIndex = indexToTimestamp.firstKey();
        if (newMin < firstAvailableIndex) {
            QDateTime firstBarTime = completedBars.firstKey();
            QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
            checkForMissingBars(requestTime, firstBarTime);
        }
    }

    // Price axis zoom
    qreal currentMinPrice = axisY->min();
    qreal currentMaxPrice = axisY->max();
    qreal priceRange = currentMaxPrice - currentMinPrice;
    qreal centerPrice = (currentMaxPrice + currentMinPrice) / 2;

    qreal newPriceRange = priceRange * zoomFactor;
    qreal newMinPrice = centerPrice - (newPriceRange / 2);
    qreal newMaxPrice = centerPrice + (newPriceRange / 2);

    axisX->setRange(newMin, newMax);
    axisY->setRange(qMax(0.0, newMinPrice), newMaxPrice);

    updateLastPriceLineIfNeeded();
    updateAxisLabels();
}
