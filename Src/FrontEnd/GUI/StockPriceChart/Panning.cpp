#include "StockPriceChart.h"

/**
 * @brief Handles widget resize events.
 * 
 * Updates the chart geometry and repositions the price label and background
 * rectangles when the widget is resized.
 * 
 * @param event The QResizeEvent containing size information.
 */
void StockPriceChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    // Update the chart's geometry
    chart->resize(event->size());
    
    // Update price label and backgrounds
    updatePriceLabelPosition();
    updateAfterHoursBackground();
}



/**
 * @brief Event filter for handling mouse interactions on the chart view.
 *
 * Processes mouse button press, release, and move events to implement
 * panning and right-click recentering functionality. Only handles events
 * from the chart view's viewport.
 *
 * @param object The object that generated the event.
 * @param event The event to process.
 * @return true if the event was handled, false otherwise.
 */
bool StockPriceChart::eventFilter(QObject* object, QEvent* event) {
    if (object != chartView->viewport()) {
        return QWidget::eventFilter(object, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        return handleMouseButtonPress(mouseEvent);
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        return handleMouseButtonRelease(mouseEvent);
    }

    case QEvent::MouseMove: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        return handleMouseMove(mouseEvent);
    }

    default:
        return QWidget::eventFilter(object, event);
    }
}

/**
 * @brief Handles mouse button press events.
 *
 * Processes right-click for recentering the view and left-click for starting panning.
 *
 * @param mouseEvent The mouse event containing button and position information.
 * @return true if the event was handled, false otherwise.
 */
bool StockPriceChart::handleMouseButtonPress(QMouseEvent* mouseEvent) {
    if (mouseEvent->button() == Qt::RightButton) {
        // Handle right click - recenter view

        Q_ASSERT(!indexToTimestamp.isEmpty());

        int lastIndex = indexToTimestamp.lastKey();
        int startIndex = lastIndex - 30;  // Show last 30 bars (can be negative now)
        int endIndex = lastIndex + 1;

        // Reset the horizontal axis
        axisX->setRange(startIndex, endIndex);

        // Reset the vertical axis to fit visible bars
        double minPrice = std::numeric_limits<double>::max();
        double maxPrice = std::numeric_limits<double>::lowest();
        double currentPrice = 0.0;

        // Retrieve the iterator of the indexToTimestamp map from the startIndex to endIndex
        const auto endIt = indexToTimestamp.upperBound(endIndex);
   
        // Find min/max prices for bars in the visible range
        for (auto it = indexToTimestamp.lowerBound(startIndex); it != endIt; ++it) {
            int index = it.key();
            Q_ASSERT(index >= startIndex && index <= endIndex);

            const QDateTime& timestamp = it.value();

            // Check completed bars
            if (completedBars.contains(timestamp)) {
                const Bar& bar = completedBars[timestamp];
                minPrice = qMin(minPrice, bar.getLow());
                maxPrice = qMax(maxPrice, bar.getHigh());
                currentPrice = bar.getClose();
            }

        }

        // Check open bar
        if (hasOpenBar) {
            minPrice = qMin(minPrice, currentOpenBar.getLow());
            maxPrice = qMax(maxPrice, currentOpenBar.getHigh());
            currentPrice = currentOpenBar.getClose();
        }

        // Set vertical range if we found any bars
        if (minPrice != std::numeric_limits<double>::max()) {
            // Add padding
            double padding = currentPrice * 0.0002; // 0.02% padding
            // Ensure minimum range
            double minRange = currentPrice * 0.0005; // 0.05% of current price
            if (maxPrice - minPrice < minRange) {
                maxPrice = currentPrice + (minRange / 2);
                minPrice = currentPrice - (minRange / 2);
            }
            axisY->setRange(qMax(0.0, minPrice - padding), maxPrice + padding);
        }

        updateAfterHoursBackground();
        updateLastPriceLine();
        
        updateAxisLabelsDensity();

        return true;
    }
    if (mouseEvent->button() != Qt::LeftButton) {
        return false;
    }
    isPanning = true;
    lastMousePos = mouseEvent->pos();
    chartView->setCursor(Qt::ClosedHandCursor);
    return true;
}

/**
 * @brief Handles mouse button release events.
 *
 * Stops panning mode when left mouse button is released.
 *
 * @param mouseEvent The mouse event containing button information.
 * @return true if the event was handled, false otherwise.
 */
bool StockPriceChart::handleMouseButtonRelease(QMouseEvent* mouseEvent) {
    if (mouseEvent->button() != Qt::LeftButton || !isPanning) {
        return false;
    }
    isPanning = false;
    chartView->setCursor(Qt::ArrowCursor);
    if (hasOpenBar) {
        updateLastPriceLine();
    }
    return true;
}

/**
 * @brief Handles mouse move events during panning.
 *
 * Updates chart axes based on mouse movement when in panning mode.
 *
 * @param mouseEvent The mouse event containing position information.
 * @return true if the event was handled, false otherwise.
 */
bool StockPriceChart::handleMouseMove(QMouseEvent* mouseEvent) {
    if (!isPanning) {
        return false;
    }
    handlePanning(mouseEvent);
    return true;
}


/**
 * @brief Handles mouse panning movement.
 * 
 * Updates the chart axes based on mouse movement delta, converting pixel
 * movement to appropriate index and price units. Checks for missing bars
 * when panning to earlier times.
 * 
 * @param mouseEvent The QMouseEvent containing mouse position information.
 */
void StockPriceChart::handlePanning(QMouseEvent* mouseEvent) {
    QPoint delta = mouseEvent->pos() - lastMousePos;
    lastMousePos = mouseEvent->pos();

    // Convert pixel movement to index units for X axis
    qreal indexPerPixel = (axisX->max() - axisX->min()) / chartView->width();
    qreal indexOffset = -delta.x() * indexPerPixel;

    // Convert pixel movement to price units for Y axis
    qreal pricePerPixel = (axisY->max() - axisY->min()) / chartView->height();
    qreal priceOffset = delta.y() * pricePerPixel;

    // Update axes ranges
    qreal newMin = axisX->min() + indexOffset;
    qreal newMax = axisX->max() + indexOffset;
    
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
    axisY->setRange(qMax(0.0, axisY->min() + priceOffset), axisY->max() + priceOffset);

    // When panning, we don't update the axis labels to avoid jitter since they dont change,
    // only the range shifts.
    //updateAxisLabels();

    // Update the price label position and last price line
    updatePriceLabelPosition();
    if (hasOpenBar) {
        updateLastPriceLine();
    }
}