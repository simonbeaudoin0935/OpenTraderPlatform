#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QVBoxLayout>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QFont>
#include <QDebug>
#include <QTimeZone>

#include "StockPriceChart.h"
#include "../Misc/MarketHours.h"

#define CANDLESTICK_BODY_WIDTH 0.9 // 90% of available space

Q_LOGGING_CATEGORY(ChartLog, "Chart");

StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent) {
    lastPriceLine = new QLineSeries();
    lastPriceLine->setPen(QPen(Qt::green, 1, Qt::DashLine)); // Start with green, dashed line

    candlestickSeries = new QCandlestickSeries();
    candlestickSeries->setPen(QPen(QColor(Qt::black)));
    candlestickSeries->setIncreasingColor(QColor(Qt::green));
    candlestickSeries->setDecreasingColor(QColor(Qt::red));
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH);

    voidBarSeries = new QScatterSeries();
    voidBarSeries->setName("Void Bars");
    voidBarSeries->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    voidBarSeries->setMarkerSize(10.0);  // Size of the X marker
    voidBarSeries->setPen(QPen(QColor(Qt::red), 2));  // Red X with thicker lines
    voidBarSeries->setBrush(Qt::NoBrush);  // No fill, just the X outline

    chart = new QChart();
    chart->addSeries(candlestickSeries);
    chart->addSeries(lastPriceLine);
    chart->addSeries(voidBarSeries);  // Add the void bar series to the chart

    // Add margins to ensure price label is visible
    chart->setMargins(QMargins(5, 5, 50, 5));  // Left, Top, Right, Bottom
    
    // Apply dark theme to chart
    chart->setBackgroundBrush(QBrush(QColor(65, 65, 70)));
    chart->setBackgroundPen(QPen(QColor(25, 25, 25)));
    chart->setTitleBrush(QBrush(QColor(255, 255, 255)));
    chart->setTitleFont(QFont("Arial", 10, QFont::Bold));
    chart->legend()->setLabelBrush(QBrush(QColor(255, 255, 255)));
    chart->legend()->setBackgroundVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setColor(QColor(65, 65, 70, 150));

    chartView = new QChartView(chart, this);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setRubberBand(QChartView::NoRubberBand);  // Disable default rubber band
    chartView->setMouseTracking(true);  // Enable mouse tracking
    chartView->viewport()->installEventFilter(this);  // Install event filter on the viewport
    chartView->setBackgroundBrush(QBrush(QColor(45, 45, 50)));

    // Create price label as a child of the chart view's scene
    priceLabel = chartView->scene()->addText("");
    priceLabel->setDefaultTextColor(Qt::white);
    QFont font = priceLabel->font();
    font.setBold(true);
    priceLabel->setFont(font);

    setSymbol("");

    axisX = new QValueAxis();  // Changed from QDateTimeAxis - now uses indices
    axisX->setTitleText("Time");
    axisX->setGridLineVisible(true);
    axisX->setMinorGridLineVisible(false);
    axisX->setLabelsAngle(-45); // Angle the time labels for better readability
    axisX->setGridLineColor(QColor(70, 70, 70));
    axisX->setLabelsColor(QColor(220, 220, 220));
    axisX->setTitleBrush(QBrush(QColor(220, 220, 220)));
    axisX->setLabelFormat("");  // We'll use custom labels
    axisX->setTickCount(7);  // Default tick count

    chart->addAxis(axisX, Qt::AlignBottom);
    candlestickSeries->attachAxis(axisX);
    lastPriceLine->attachAxis(axisX);
    voidBarSeries->attachAxis(axisX);  // Attach void bar series to X axis

    axisY = new QValueAxis();
    axisY->setLabelFormat("%.2f");
    axisY->setTitleText("Price");
    axisY->setGridLineColor(QColor(70, 70, 70));
    axisY->setLabelsColor(QColor(220, 220, 220));
    axisY->setTitleBrush(QBrush(QColor(220, 220, 220)));
    
    chart->addAxis(axisY, Qt::AlignLeft);
    candlestickSeries->attachAxis(axisY);
    lastPriceLine->attachAxis(axisY);
    voidBarSeries->attachAxis(axisY);  // Attach void bar series to Y axis

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);  // Remove widget margins
    layout->addWidget(chartView);
    setLayout(layout);

    // Connect to the axis range changed signal
    connect(axisX, &QDateTimeAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
    connect(axisX, &QDateTimeAxis::rangeChanged, this, &StockPriceChart::updateLastPriceLineIfNeeded);
    connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
    connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::updateLastPriceLineIfNeeded);
}

StockPriceChart::~StockPriceChart() {
    // No need to delete chart, series, etc.—handled by Qt parent hierarchy
}

void StockPriceChart::setSymbol(const QString& symbol) {
    this->symbol = symbol;
    candlestickSeries->setName(symbol + " (Bars)");
    chart->setTitle("Stock Price: " + symbol);
}

void StockPriceChart::addBar(const Bar& bar) {
    if (!bar.isValid()) {
        return;
    }

    if (bar.getBarStatus() == Bar::BarStatus::Closed) {
        handleClosedBar(bar);
    } else if (bar.getBarStatus() == Bar::BarStatus::Open) {
        handleOpenBar(bar);
    } else {
        qCritical("StockPriceChart: Received bar with unknown status");
    }

    updateChart();
}

void StockPriceChart::onRequestedMissingBarsReceived(const QVector<Bar>& bars) {
    // If this flag isn't true, it means there is a logic bug somewhere
    Q_ASSERT(currentGetBarsRequestInProcess == true);

    // Important to reset so more requests can be made in the future
    currentGetBarsRequestInProcess = false;

    if (bars.isEmpty()) {
        qCCritical(ChartLog) << "Requested missing bars empty";
        return;
    }
    //Q_ASSERT(!bars.isEmpty());

    if (lastValidClosePrice == 0.0) {
        lastValidClosePrice = completedBars.first().getOpen();
    }

    // Insert all received bars into the map
    for (const Bar& bar : bars) {
        if (bar.getBarStatus() != Bar::BarStatus::Null) {
            completedBars.insert(bar.getTimeStamp(), bar);
            // Update the last valid close price
            lastValidClosePrice = bar.getClose();
        } else {
            // Add void bar to the scatter series
            voidBarSeries->append(bar.getTimeStamp().toMSecsSinceEpoch(), lastValidClosePrice);
        }
    }

    // Maintain the bar limit
    maintainBarLimit();

    // Update the chart to display the new bars
    updateChart();

    // Update the last price line if needed
    if (!bars.isEmpty()) {
        const Bar& lastBar = bars.last();
        updateLastPriceLine(lastBar.getClose(), lastBar.getClose() >= lastBar.getOpen());
    }

    // Update the after-hours background
    updateAfterHoursBackground();
}

void StockPriceChart::handleClosedBar(const Bar& bar) {
    // Store current view state (using indices)
    qreal currentMinIndex = axisX->min();
    qreal currentMaxIndex = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = (currentMaxIndex - currentMinIndex) > 0;

    if (!hasOpenBar) {
        completedBars.insert(bar.getTimeStamp(), bar);
        maintainBarLimit();
        return;
    }

    QDateTime openBarTime = currentOpenBar.getTimeStamp();
    QDateTime closedBarTime = bar.getTimeStamp();

    if (openBarTime != closedBarTime) {
        completedBars.insert(openBarTime, currentOpenBar);
    }
    
    hasOpenBar = false;
    completedBars.insert(closedBarTime, bar);
    maintainBarLimit();

    // Restore view state if it was initialized
    if (hadInitialView) {
        axisX->setRange(currentMinIndex, currentMaxIndex);
        axisY->setRange(currentYMin, currentYMax);
    }
}

void StockPriceChart::maintainBarLimit() {
    while (completedBars.size() > MAX_BARS) {
        // Remove the oldest bar (first key in the map)
        completedBars.erase(completedBars.begin());
    }
}

void StockPriceChart::handleOpenBar(const Bar& bar) {
    QDateTime newBarTime = bar.getTimeStamp();
    double newPrice = bar.getClose();

    if (!hasOpenBar) {
        currentOpenBar = bar;
        hasOpenBar = true;
        updateLastPriceLine(newPrice, newPrice >= lastPrice);
        return;
    }

    QDateTime currentBarTime = currentOpenBar.getTimeStamp();
    if (newBarTime != currentBarTime) {
        completedBars.insert(currentBarTime, currentOpenBar);
    }

    currentOpenBar = bar;
    updateLastPriceLine(newPrice, newPrice >= currentOpenBar.getOpen());
}

void StockPriceChart::updateChart() {
    // Store current view state (now using indices)
    qreal currentMinIndex = axisX->min();
    qreal currentMaxIndex = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = (currentMaxIndex - currentMinIndex) > 0;

    // Rebuild the index mapping
    rebuildIndexMapping();

    // Clear existing candlesticks
    candlestickSeries->clear();
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH); // Reset body width after clearing

    // Calculate marker size based on candlestick width
    qreal markerSize = CANDLESTICK_BODY_WIDTH * 0.8; // Make it slightly smaller than candlestick width
    voidBarSeries->setMarkerSize(markerSize);

    // Add completed bars using their indices
    int index = 0;
    for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
        const Bar& bar = it.value();
        auto set = new QCandlestickSet();
        set->setTimestamp(index);  // Use index instead of timestamp
        set->setOpen(bar.getOpen());
        set->setHigh(bar.getHigh());
        set->setLow(bar.getLow());
        set->setClose(bar.getClose());
        candlestickSeries->append(set);
        index++;
    }

    // Add current open bar if it exists
    if (hasOpenBar) {
        auto set = new QCandlestickSet();
        set->setTimestamp(index);  // Use index instead of timestamp
        set->setOpen(currentOpenBar.getOpen());
        set->setHigh(currentOpenBar.getHigh());
        set->setLow(currentOpenBar.getLow());
        set->setClose(currentOpenBar.getClose());
        candlestickSeries->append(set);
    }

    // Only update time axis range if this is the initial setup
    if (!hadInitialView && candlestickSeries->count() > 0) {
        int lastIndex = index;  // Last bar index
        int startIndex = qMax(0, lastIndex - 30);  // Show last 30 bars
        int endIndex = lastIndex + 1;  // Small buffer
        
        axisX->setRange(startIndex, endIndex);
    } else {
        // Restore the previous view
        axisX->setRange(currentMinIndex, currentMaxIndex);
        axisY->setRange(currentYMin, currentYMax);
    }

    // Calculate current visible price range
    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();
    double currentPrice = 0.0;

    // Find min/max prices for visible bars only
    int visibleStartIndex = static_cast<int>(axisX->min());
    int visibleEndIndex = static_cast<int>(axisX->max());
    
    int currentIndex = 0;
    for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
        if (currentIndex >= visibleStartIndex && currentIndex <= visibleEndIndex) {
            const Bar& bar = it.value();
            minPrice = qMin(minPrice, bar.getLow());
            maxPrice = qMax(maxPrice, bar.getHigh());
            currentPrice = bar.getClose();
        }
        currentIndex++;
    }

    // Check open bar if visible
    if (hasOpenBar && currentIndex >= visibleStartIndex && currentIndex <= visibleEndIndex) {
        minPrice = qMin(minPrice, currentOpenBar.getLow());
        maxPrice = qMax(maxPrice, currentOpenBar.getHigh());
        currentPrice = currentOpenBar.getClose();
    }

    // Only update Y axis range if necessary and if we're not preserving the view
    if (!hadInitialView && minPrice != std::numeric_limits<double>::max()) {
        // Add padding
        double padding = currentPrice * 0.0002; // 0.02% padding
        // Ensure minimum range
        double minRange = currentPrice * 0.0005; // 0.05% of current price
        if (maxPrice - minPrice < minRange) {
            maxPrice = currentPrice + (minRange / 2);
            minPrice = currentPrice - (minRange / 2);
        }
        axisY->setRange(minPrice - padding, maxPrice + padding);
    }
}

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

void StockPriceChart::handleVerticalPanning(QWheelEvent* event) {
    qreal currentMin = axisY->min();
    qreal currentMax = axisY->max();
    qreal priceRange = currentMax - currentMin;

    qreal shiftAmount = priceRange * 0.05;
    if (event->angleDelta().y() < 0) {
        shiftAmount = -shiftAmount;
    }

    axisY->setRange(currentMin + shiftAmount, currentMax + shiftAmount);
    updateLastPriceLineIfNeeded();
}

void StockPriceChart::handleHorizontalPanning(QWheelEvent* event) {
    qreal currentMin = axisX->min();
    qreal currentMax = axisX->max();
    qreal indexRange = currentMax - currentMin;
    qreal shiftAmount = indexRange * 0.05;
    shiftAmount = -shiftAmount * (event->angleDelta().x() > 0 ? 1 : -1);

    qreal newMin = currentMin + shiftAmount;
    qreal newMax = currentMax + shiftAmount;
    
    // Check for missing bars BEFORE constraining
    if (newMin < 0 && !completedBars.isEmpty()) {
        QDateTime firstBarTime = completedBars.firstKey();
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, firstBarTime);
    }
    
    // Constrain to available data
    newMin = qMax(0.0, newMin);
    
    axisX->setRange(newMin, newMax);
    updateLastPriceLineIfNeeded();
}

void StockPriceChart::handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    qreal currentMin = axisX->min();
    qreal currentMax = axisX->max();
    qreal indexRange = currentMax - currentMin;
    qreal centerIndex = currentMin + (indexRange / 2);

    qreal newIndexRange = indexRange * zoomFactor;
    qreal newMin = centerIndex - (newIndexRange / 2);
    qreal newMax = centerIndex + (newIndexRange / 2);
    
    // Check for missing bars BEFORE constraining
    if (newMin < 0 && !completedBars.isEmpty()) {
        QDateTime firstBarTime = completedBars.firstKey();
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, firstBarTime);
    }
    
    // Constrain to available data
    newMin = qMax(0.0, newMin);

    axisX->setRange(newMin, newMax);
    updateLastPriceLineIfNeeded();
}

void StockPriceChart::handleVerticalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    qreal currentMin = axisY->min();
    qreal currentMax = axisY->max();
    qreal range = currentMax - currentMin;
    qreal center = (currentMax + currentMin) / 2;

    qreal newRange = range * zoomFactor;
    qreal newMin = center - (newRange / 2);
    qreal newMax = center + (newRange / 2);

    axisY->setRange(newMin, newMax);
    updateLastPriceLineIfNeeded();
}

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
    
    // Check for missing bars BEFORE constraining
    if (newMin < 0 && !completedBars.isEmpty()) {
        QDateTime firstBarTime = completedBars.firstKey();
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, firstBarTime);
    }
    
    // Constrain to available data
    newMin = qMax(0.0, newMin);

    // Price axis zoom
    qreal currentMinPrice = axisY->min();
    qreal currentMaxPrice = axisY->max();
    qreal priceRange = currentMaxPrice - currentMinPrice;
    qreal centerPrice = (currentMaxPrice + currentMinPrice) / 2;

    qreal newPriceRange = priceRange * zoomFactor;
    qreal newMinPrice = centerPrice - (newPriceRange / 2);
    qreal newMaxPrice = centerPrice + (newPriceRange / 2);

    axisX->setRange(newMin, newMax);
    axisY->setRange(newMinPrice, newMaxPrice);

    updateLastPriceLineIfNeeded();
}

void StockPriceChart::updateLastPriceLineIfNeeded() {
    // Update the price line regardless of whether there's an open bar
    if (candlestickSeries->count() > 0) {
        auto lastSet = candlestickSeries->sets().last();
        double closePrice = lastSet->close();
        double openPrice = lastSet->open();
        updateLastPriceLine(closePrice, closePrice >= openPrice);
    }
}

void StockPriceChart::updatePriceLabelPosition() {
    if (lastPriceLine->points().isEmpty() || lastPriceLine->points().size() < 2) {
        return;
    }

    qreal endIndex = lastPriceLine->points().last().x();
    double price = lastPriceLine->points().last().y();

    // Get the price point in view coordinates
    QPointF pricePoint(endIndex, price);
    QPointF viewPoint = chart->mapToPosition(pricePoint, lastPriceLine);

    // Calculate position in scene coordinates
    QRectF plotArea = chart->plotArea();
    QRectF chartRect = chart->geometry();
    
    // Position label in the right margin
    qreal labelX = chartRect.right() - priceLabel->boundingRect().width() - 15;
    qreal labelY = viewPoint.y() - (priceLabel->boundingRect().height() / 2);

    // Keep label within plot area bounds vertically
    labelY = qMax(labelY, plotArea.top());
    labelY = qMin(labelY, plotArea.bottom() - priceLabel->boundingRect().height());

    // Convert to scene coordinates
    QPointF scenePos = chart->mapToScene(QPointF(labelX, labelY));
    priceLabel->setPos(scenePos);
}

void StockPriceChart::updateLastPriceLine(double price, bool isUpTick) {
    lastPriceLine->clear();

    // Get the current visible range in indices
    qreal startIndex = axisX->min();
    qreal endIndex = axisX->max();

    // Create two points for the horizontal line spanning the visible range
    lastPriceLine->append(startIndex, price);
    lastPriceLine->append(endIndex, price);

    // Update the line color based on price movement
    QColor lineColor = isUpTick ? Qt::green : Qt::red;
    lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update price label
    priceLabel->setPlainText(QString::number(price, 'f', 2));
    priceLabel->setDefaultTextColor(lineColor);

    // Update the label position
    updatePriceLabelPosition();

    lastPrice = price;
}

void StockPriceChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    // Update the chart's geometry
    chart->resize(event->size());
    
    // Update price label and backgrounds
    updatePriceLabelPosition();
    updateAfterHoursBackground();
}

bool StockPriceChart::isAfterMarketHours(const QDateTime& localTime) {
    // Convert local time to New York time
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = localTime.toTimeZone(nyZone);
    
    // Check if it's after 4 PM (16:00) NY time
    return nyTime.time().hour() >= 16;
}

void StockPriceChart::updateAfterHoursBackground() {
    // Clear existing rectangles
    clearBackgroundRects();

    // Update the chart's geometry
    chart->resize(chartView->size());
    
    if (indexToTimestamp.isEmpty()) {
        return;
    }

    // Get visible range in indices
    qreal minIndex = axisX->min();
    qreal maxIndex = axisX->max();
    
    // Get the timestamps for visible range
    QDateTime startTime = getTimestampForIndex(static_cast<int>(minIndex));
    QDateTime endTime = getTimestampForIndex(static_cast<int>(maxIndex));
    
    // Convert times to NY timezone for date calculations
    QTimeZone nyZone("America/New_York");
    QDateTime nyStartTime = startTime.toTimeZone(nyZone);
    QDateTime nyEndTime = endTime.toTimeZone(nyZone);

    // Get all dates in the range, including the day before the start
    QDate currentDate = nyStartTime.date().addDays(-1);  // Start from previous day
    QDate endDate = nyEndTime.date();

    // Create session boundaries for each day
    while (currentDate <= endDate) {
        // Create base time in NY timezone
        QDateTime currentDateTime = QDateTime(currentDate, QTime(0, 0), nyZone);
        QDateTime nextDayDateTime = QDateTime(currentDate.addDays(1), QTime(0, 0), nyZone);

        // Handle weekends - show closed market background
        if (currentDate.dayOfWeek() > 5) {  // Saturday = 6, Sunday = 7
            drawBackgroundForTimeRange(currentDateTime.toLocalTime(), 
                                      nextDayDateTime.toLocalTime(),
                                      QColor(40, 40, 50, 120), -2, closedMarketRects);
            currentDate = currentDate.addDays(1);
            continue;
        }

        // Handle each hour of the day
        for (int hour = 0; hour < 24; hour++) {
            QDateTime hourStart = QDateTime(currentDate, QTime(hour, 0), nyZone);
            QDateTime hourEnd = hour == 23 ? 
                QDateTime(currentDate, QTime(23, 59, 59), nyZone) : 
                QDateTime(currentDate, QTime(hour + 1, 0), nyZone);

            // Convert to local time
            QDateTime localHourStart = hourStart.toLocalTime();
            QDateTime localHourEnd = hourEnd.toLocalTime();

            if (MarketHours::isPreMarket(hourStart)) {
                drawBackgroundForTimeRange(localHourStart, localHourEnd,
                                          QColor(90, 60, 30, 100), -1, preMarketRects);
            }
            else if (MarketHours::isAfterHours(hourStart)) {
                drawBackgroundForTimeRange(localHourStart, localHourEnd,
                                          QColor(50, 50, 80, 100), -1, afterHoursRects);
            }
            else if (!MarketHours::isRegularHours(hourStart)) {
                drawBackgroundForTimeRange(localHourStart, localHourEnd,
                                          QColor(40, 40, 50, 120), -2, closedMarketRects);
            }
        }
        currentDate = currentDate.addDays(1);
    }
}

QGraphicsRectItem* StockPriceChart::createBackgroundRect(const QColor& color, int zValue) {
    QGraphicsRectItem* rect = new QGraphicsRectItem(chart);
    rect->setBrush(color);
    rect->setPen(Qt::NoPen);
    rect->setZValue(zValue);
    return rect;
}

void StockPriceChart::clearBackgroundRects() {
    // Delete and clear after-hours rectangles
    for (auto rect : afterHoursRects) {
        chartView->scene()->removeItem(rect);
        delete rect;
    }
    afterHoursRects.clear();

    // Delete and clear pre-market rectangles
    for (auto rect : preMarketRects) {
        chartView->scene()->removeItem(rect);
        delete rect;
    }
    preMarketRects.clear();

    // Delete and clear closed market rectangles
    for (auto rect : closedMarketRects) {
        chartView->scene()->removeItem(rect);
        delete rect;
    }
    closedMarketRects.clear();
}

bool StockPriceChart::eventFilter(QObject* object, QEvent* event) {
    if (object != chartView->viewport()) {
        return QWidget::eventFilter(object, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            // Handle right click - recenter view
            if (candlestickSeries->count() > 0) {
                int lastIndex = indexToTimestamp.lastKey();
                int startIndex = qMax(0, lastIndex - 30);  // Show last 30 bars
                int endIndex = lastIndex + 1;

                // Reset the horizontal axis
                axisX->setRange(startIndex, endIndex);

                // Reset the vertical axis to fit visible bars
                double minPrice = std::numeric_limits<double>::max();
                double maxPrice = std::numeric_limits<double>::lowest();
                double currentPrice = 0.0;
                
                // Find min/max prices for bars in the visible range
                int currentIndex = 0;
                for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
                    if (currentIndex >= startIndex && currentIndex <= endIndex) {
                        const Bar& bar = it.value();
                        minPrice = qMin(minPrice, bar.getLow());
                        maxPrice = qMax(maxPrice, bar.getHigh());
                        currentPrice = bar.getClose();
                    }
                    currentIndex++;
                }
                
                // Check open bar
                if (hasOpenBar && currentIndex >= startIndex && currentIndex <= endIndex) {
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
                    axisY->setRange(minPrice - padding, maxPrice + padding);
                }
                
                updateAfterHoursBackground();
                updateLastPriceLineIfNeeded();
                return true;
            }
            return false;
        }
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }
        isPanning = true;
        lastMousePos = mouseEvent->pos();
        chartView->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton || !isPanning) {
            return false;
        }
        isPanning = false;
        chartView->setCursor(Qt::ArrowCursor);
        if (hasOpenBar) {
            updateLastPriceLine(currentOpenBar.getClose(),
                              currentOpenBar.getClose() >= currentOpenBar.getOpen());
        }
        return true;
    }

    case QEvent::MouseMove: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!isPanning) {
            return false;
        }
        handlePanning(mouseEvent);
        return true;
    }

    default:
        return QWidget::eventFilter(object, event);
    }
}

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
    
    // Check for missing bars BEFORE constraining
    if (newMin < 0 && !completedBars.isEmpty()) {
        QDateTime firstBarTime = completedBars.firstKey();
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, firstBarTime);
    }
    
    // Constrain to available data
    newMin = qMax(0.0, newMin);
    
    axisX->setRange(newMin, newMax);
    axisY->setRange(axisY->min() + priceOffset, axisY->max() + priceOffset);

    // Update the price label position and last price line
    updatePriceLabelPosition();
    if (hasOpenBar) {
        updateLastPriceLine(currentOpenBar.getClose(),
                          currentOpenBar.getClose() >= currentOpenBar.getOpen());
    }
}

void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime) {
    Q_UNUSED(viewEndTime);

    QDateTime viewStartTimeRounded = viewStartTime.addSecs(-viewStartTime.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTime.time().msec());
    QDateTime firstBarTime;

    if (completedBars.isEmpty()) {
        firstBarTime = QDateTime::currentDateTime();
    } else {
        firstBarTime = completedBars.firstKey();
    }
        
    if (viewStartTimeRounded < firstBarTime) {
        qCDebug(ChartLog) << "Chart view extends beyond available bars:";
        qCDebug(ChartLog) << "  Last :" << firstBarTime;
        qCDebug(ChartLog) << "  First:" << viewStartTimeRounded;
    } else {
        return;
    }

    // If there is already a getBars request to the bar cache, suck it up and wait to receive the data
    if (currentGetBarsRequestInProcess) {
        qCDebug(ChartLog) << "current get bars request already in progress";
        return;
    } else {
        currentGetBarsRequestInProcess = true;

        emit requestMissingBars(viewStartTimeRounded, firstBarTime);
    }
}

void StockPriceChart::clearSymbol() {
    // Clear the candlestick series
    candlestickSeries->clear();
    
    // Clear the completed bars
    completedBars.clear();
    
    // Reset the current open bar
    hasOpenBar = false;
    currentOpenBar = Bar();
    
    // Clear the last price line
    lastPriceLine->clear();
    lastPrice = 0.0;
    
    // Clear background rectangles
    clearBackgroundRects();
    
    // Reset the price label
    priceLabel->setPlainText("");
    
    // Reset the chart title
    chart->setTitle("Stock Price: " + symbol);
    
    // Clear index mappings
    indexToTimestamp.clear();
    timestampToIndex.clear();
    
    // Reset the axes ranges to default
    axisX->setRange(0, 30);
    axisY->setRange(0, 100);
}

void StockPriceChart::rebuildIndexMapping() {
    indexToTimestamp.clear();
    timestampToIndex.clear();
    
    int index = 0;
    for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
        const QDateTime& timestamp = it.key();
        indexToTimestamp[index] = timestamp;
        timestampToIndex[timestamp] = index;
        index++;
    }
    
    // Add open bar if it exists
    if (hasOpenBar) {
        const QDateTime& timestamp = currentOpenBar.getTimeStamp();
        indexToTimestamp[index] = timestamp;
        timestampToIndex[timestamp] = index;
    }
}

int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const {
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end()) {
        return it.value();
    }
    
    // If exact timestamp not found, find the closest index
    if (timestampToIndex.isEmpty()) {
        return 0;
    }
    
    // Find the first timestamp greater than or equal to the given timestamp
    auto upper = timestampToIndex.upperBound(timestamp);
    if (upper == timestampToIndex.begin()) {
        return 0;  // Before all bars
    }
    
    // Return the index of the previous bar
    --upper;
    return upper.value();
}

QDateTime StockPriceChart::getTimestampForIndex(int index) const {
    auto it = indexToTimestamp.find(index);
    if (it != indexToTimestamp.end()) {
        return it.value();
    }
    
    // If index not found, interpolate or extrapolate
    if (indexToTimestamp.isEmpty()) {
        return QDateTime::currentDateTime();
    }
    
    if (index < 0) {
        // Extrapolate backwards - assume 1-minute bars
        int firstIndex = indexToTimestamp.firstKey();
        QDateTime firstTime = indexToTimestamp.first();
        int deltaIndex = firstIndex - index;
        return firstTime.addSecs(-deltaIndex * 60);
    }
    
    int lastIndex = indexToTimestamp.lastKey();
    if (index > lastIndex) {
        // Extrapolate forwards - assume 1-minute bars
        QDateTime lastTime = indexToTimestamp.last();
        int deltaIndex = index - lastIndex;
        return lastTime.addSecs(deltaIndex * 60);
    }
    
    // Should not reach here, but return current time as fallback
    return QDateTime::currentDateTime();
}

void StockPriceChart::updateAxisLabels() {
    // Get visible range in indices
    qreal minIndex = axisX->min();
    qreal maxIndex = axisX->max();
    qreal range = maxIndex - minIndex;
    
    if (range <= 0 || indexToTimestamp.isEmpty()) {
        return;
    }
    
    // Determine number of labels based on range
    int numLabels = 7;  // Default
    if (range < 10) {
        numLabels = qMax(3, static_cast<int>(range) + 1);
    } else if (range < 30) {
        numLabels = 5;
    } else if (range < 60) {
        numLabels = 7;
    } else {
        numLabels = 10;
    }
    
    axisX->setTickCount(numLabels);
    
    // Qt doesn't provide easy custom labels for QValueAxis, so we'll rely on
    // the automatic labeling showing indices. For a production version, you
    // could use QCategoryAxis or custom drawing, but that's beyond minimal changes.
    // The axis will show index numbers which is acceptable for now.
}

void StockPriceChart::drawBackgroundForTimeRange(const QDateTime& rangeStart, const QDateTime& rangeEnd,
                                                   const QColor& color, int zValue,
                                                   QList<QGraphicsRectItem*>& rectList) {
    if (indexToTimestamp.isEmpty()) {
        return;
    }
    
    // Find the indices that correspond to this time range
    int startIndex = -1;
    int endIndex = -1;
    
    // Iterate through all bars to find which ones fall in this time range
    for (auto it = indexToTimestamp.constBegin(); it != indexToTimestamp.constEnd(); ++it) {
        const QDateTime& barTime = it.value();
        if (barTime >= rangeStart && barTime <= rangeEnd) {
            if (startIndex == -1) {
                startIndex = it.key();
            }
            endIndex = it.key();
        }
    }
    
    // If we found bars in this range, draw the background
    if (startIndex != -1 && endIndex != -1) {
        qreal visibleMinIndex = axisX->min();
        qreal visibleMaxIndex = axisX->max();
        
        // Clip to visible range
        qreal clippedStart = qMax(static_cast<qreal>(startIndex), visibleMinIndex);
        qreal clippedEnd = qMin(static_cast<qreal>(endIndex + 1), visibleMaxIndex);
        
        if (clippedStart < clippedEnd) {
            QPointF topLeft = chart->mapToPosition(QPointF(clippedStart, axisY->max()));
            QPointF bottomRight = chart->mapToPosition(QPointF(clippedEnd, axisY->min()));
            
            auto rect = createBackgroundRect(color, zValue);
            rect->setRect(topLeft.x(), topLeft.y(),
                         bottomRight.x() - topLeft.x(),
                         bottomRight.y() - topLeft.y());
            rectList.append(rect);
        }
    }
}
