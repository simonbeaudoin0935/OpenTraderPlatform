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

    axisX = new QDateTimeAxis();
    // We want labels only at 5-minute marks
    axisX->setFormat("hh:mm"); // Show only hours and minutes
    axisX->setTitleText("Time");
    axisX->setGridLineVisible(true);
    axisX->setMinorGridLineVisible(false);
    axisX->setLabelsAngle(-45); // Angle the time labels for better readability
    axisX->setGridLineColor(QColor(70, 70, 70));
    axisX->setLabelsColor(QColor(220, 220, 220));
    axisX->setTitleBrush(QBrush(QColor(220, 220, 220)));

    // Calculate number of ticks for 30-minute view
    // We want a tick every 5 minutes, so for 30 minutes we need 7 ticks (0,5,10,15,20,25,30)
    axisX->setTickCount(7);

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
    // Store current view state
    QDateTime currentMin = axisX->min();
    QDateTime currentMax = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = currentMin != currentMax && currentMin.toMSecsSinceEpoch() != 0;

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
        axisX->setRange(currentMin, currentMax);
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
    // Store current view state
    QDateTime currentMin = axisX->min();
    QDateTime currentMax = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = currentMin != currentMax && currentMin.toMSecsSinceEpoch() != 0;

    // Clear existing candlesticks
    candlestickSeries->clear();
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH); // Reset body width after clearing

    // Calculate marker size based on candlestick width
    qreal markerSize = CANDLESTICK_BODY_WIDTH * 0.8; // Make it slightly smaller than candlestick width
    voidBarSeries->setMarkerSize(markerSize);

    // Add completed bars
    for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
        const QDateTime& timestamp = it.key();
        const Bar& bar = it.value();
        auto set = new QCandlestickSet();
        set->setTimestamp(timestamp.toMSecsSinceEpoch());
        set->setOpen(bar.getOpen());
        set->setHigh(bar.getHigh());
        set->setLow(bar.getLow());
        set->setClose(bar.getClose());
        candlestickSeries->append(set);
    }

    // Add current open bar if it exists
    if (hasOpenBar) {
        QDateTime timestamp = currentOpenBar.getTimeStamp();
        auto set = new QCandlestickSet();
        set->setTimestamp(timestamp.toMSecsSinceEpoch());
        set->setOpen(currentOpenBar.getOpen());
        set->setHigh(currentOpenBar.getHigh());
        set->setLow(currentOpenBar.getLow());
        set->setClose(currentOpenBar.getClose());
        candlestickSeries->append(set);
    }

    // Only update time axis range if this is the initial setup
    if (!hadInitialView && candlestickSeries->count() > 0) {
        QDateTime currentBarTime;
        if (hasOpenBar) {
            currentBarTime = currentOpenBar.getTimeStamp();
        } else {
            currentBarTime = completedBars.lastKey();
        }

        // Round current time down to the nearest 5-minute mark
        int currentMinute = currentBarTime.time().minute();
        int baseMinute = (currentMinute / 5) * 5;
        QDateTime baseTime = currentBarTime;
        baseTime.setTime(QTime(currentBarTime.time().hour(), baseMinute, 0));

        // Set start time to 30 minutes before the base time
        QDateTime startTime = baseTime.addSecs(-30 * 60);
        // Set end time to the next 5-minute mark after base time plus a small buffer
        QDateTime endTime = baseTime.addSecs(5 * 60 + 60); // Next 5-min mark + 1 min buffer

        axisX->setRange(startTime, endTime);
    } else {
        // Restore the previous view
        axisX->setRange(currentMin, currentMax);
        // Only restore Y axis if we don't need initial price focus
        if (!needsInitialPriceFocus) {
            axisY->setRange(currentYMin, currentYMax);
        }
    }

    // Calculate current visible price range
    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();
    double currentPrice = 0.0;

    // Find min/max prices and current price for visible bars only
    QDateTime visibleStart = axisX->min();
    QDateTime visibleEnd = axisX->max();
    
    for (auto it = completedBars.lowerBound(visibleStart); 
         it != completedBars.end() && it.key() <= visibleEnd; 
         ++it) {
        const Bar& bar = it.value();
        minPrice = qMin(minPrice, bar.getLow());
        maxPrice = qMax(maxPrice, bar.getHigh());
        if (it == --completedBars.end()) {
            currentPrice = bar.getClose();
        }
    }

    // Update Y axis range if this is initial focus or if necessary and we're not preserving the view
    bool shouldUpdateYAxis = needsInitialPriceFocus || 
                            (!hadInitialView && (minPrice < currentYMin || maxPrice > currentYMax || currentYMin == currentYMax));
    
    if (shouldUpdateYAxis && minPrice != std::numeric_limits<double>::max()) {
        // Add padding
        double padding = currentPrice * 0.0002; // 0.02% padding
        // Ensure minimum range
        double minRange = currentPrice * 0.0005; // 0.05% of current price
        if (maxPrice - minPrice < minRange) {
            maxPrice = currentPrice + (minRange / 2);
            minPrice = currentPrice - (minRange / 2);
        }
        axisY->setRange(minPrice - padding, maxPrice + padding);
        
        // Clear the flag after initial focus is done
        if (needsInitialPriceFocus) {
            needsInitialPriceFocus = false;
        }
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
    QDateTime currentMin = axisX->min();
    QDateTime currentMax = axisX->max();
    qint64 timeRange = currentMax.toMSecsSinceEpoch() - currentMin.toMSecsSinceEpoch();
    qint64 shiftAmount = timeRange * 0.05;
    shiftAmount = -shiftAmount * (event->angleDelta().x() > 0 ? 1 : -1);

    QDateTime newMinTime = QDateTime::fromMSecsSinceEpoch(currentMin.toMSecsSinceEpoch() + shiftAmount);
    QDateTime newMaxTime = QDateTime::fromMSecsSinceEpoch(currentMax.toMSecsSinceEpoch() + shiftAmount);
    axisX->setRange(newMinTime, newMaxTime);
    updateLastPriceLineIfNeeded();
}

void StockPriceChart::handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    QDateTime currentMin = axisX->min();
    QDateTime currentMax = axisX->max();
    qint64 timeRange = currentMax.toMSecsSinceEpoch() - currentMin.toMSecsSinceEpoch();
    qint64 centerTime = currentMin.toMSecsSinceEpoch() + (timeRange / 2);

    qint64 newTimeRange = timeRange * zoomFactor;
    QDateTime newMinTime = QDateTime::fromMSecsSinceEpoch(centerTime - (newTimeRange / 2));
    QDateTime newMaxTime = QDateTime::fromMSecsSinceEpoch(centerTime + (newTimeRange / 2));

    axisX->setRange(newMinTime, newMaxTime);
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

    // Time axis zoom
    QDateTime currentMin = axisX->min();
    QDateTime currentMax = axisX->max();
    qint64 timeRange = currentMax.toMSecsSinceEpoch() - currentMin.toMSecsSinceEpoch();
    qint64 centerTime = currentMin.toMSecsSinceEpoch() + (timeRange / 2);

    qint64 newTimeRange = timeRange * zoomFactor;
    QDateTime newMinTime = QDateTime::fromMSecsSinceEpoch(centerTime - (newTimeRange / 2));
    QDateTime newMaxTime = QDateTime::fromMSecsSinceEpoch(centerTime + (newTimeRange / 2));

    // Price axis zoom
    qreal currentMinPrice = axisY->min();
    qreal currentMaxPrice = axisY->max();
    qreal priceRange = currentMaxPrice - currentMinPrice;
    qreal centerPrice = (currentMaxPrice + currentMinPrice) / 2;

    qreal newPriceRange = priceRange * zoomFactor;
    qreal newMinPrice = centerPrice - (newPriceRange / 2);
    qreal newMaxPrice = centerPrice + (newPriceRange / 2);

    axisX->setRange(newMinTime, newMaxTime);
    axisY->setRange(newMinPrice, newMaxPrice);

    // Check if we need more bars
    checkForMissingBars(newMinTime, newMaxTime);

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

    QDateTime endTime = QDateTime::fromMSecsSinceEpoch(lastPriceLine->points().last().x());
    double price = lastPriceLine->points().last().y();

    // Get the price point in view coordinates
    QPointF pricePoint(endTime.toMSecsSinceEpoch(), price);
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

    // Get the current visible range
    QDateTime startTime = axisX->min();
    QDateTime endTime = axisX->max();

    // Create two points for the horizontal line spanning the visible range
    lastPriceLine->append(startTime.toMSecsSinceEpoch(), price);
    lastPriceLine->append(endTime.toMSecsSinceEpoch(), price);

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

    QDateTime startTime = axisX->min();
    QDateTime endTime = axisX->max();
    
    // Convert the range to points on the chart
    QPointF topLeft = chart->mapToPosition(QPointF(startTime.toMSecsSinceEpoch(), axisY->max()));
    QPointF bottomRight = chart->mapToPosition(QPointF(endTime.toMSecsSinceEpoch(), axisY->min()));

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
            QDateTime visibleStart = qMax(startTime, currentDateTime.toLocalTime());
            QDateTime visibleEnd = qMin(endTime, nextDayDateTime.toLocalTime());

            if (visibleStart < visibleEnd) {
                QPointF sessionStart = chart->mapToPosition(QPointF(
                    visibleStart.toMSecsSinceEpoch(), axisY->max()));
                QPointF sessionEnd = chart->mapToPosition(QPointF(
                    visibleEnd.toMSecsSinceEpoch(), axisY->max()));
                
                auto rect = createBackgroundRect(QColor(40, 40, 50, 120), -2);
                rect->setRect(sessionStart.x(), topLeft.y(),
                            sessionEnd.x() - sessionStart.x(),
                            bottomRight.y() - topLeft.y());
                closedMarketRects.append(rect);
            }
            currentDate = currentDate.addDays(1);
            continue;
        }

        // Handle each hour of the day
        for (int hour = 0; hour < 24; hour++) {
            QDateTime hourStart = QDateTime(currentDate, QTime(hour, 0), nyZone);
            QDateTime hourEnd = hour == 23 ? 
                QDateTime(currentDate, QTime(23, 59, 59), nyZone) : 
                QDateTime(currentDate, QTime(hour + 1, 0), nyZone);

            // Convert to local time for visibility check
            QDateTime localHourStart = hourStart.toLocalTime();
            QDateTime localHourEnd = hourEnd.toLocalTime();

            // Skip if this hour is not visible
            if (localHourEnd < startTime || localHourStart > endTime) {
                continue;
            }

            QDateTime visibleStart = qMax(startTime, localHourStart);
            QDateTime visibleEnd = qMin(endTime, localHourEnd);

            if (MarketHours::isPreMarket(hourStart)) {
                auto rect = createBackgroundRect(QColor(90, 60, 30, 100), -1);
                QPointF sessionStart = chart->mapToPosition(QPointF(
                    visibleStart.toMSecsSinceEpoch(), axisY->max()));
                QPointF sessionEnd = chart->mapToPosition(QPointF(
                    visibleEnd.toMSecsSinceEpoch(), axisY->max()));
                
                rect->setRect(sessionStart.x(), topLeft.y(),
                            sessionEnd.x() - sessionStart.x(),
                            bottomRight.y() - topLeft.y());
                preMarketRects.append(rect);
            }
            else if (MarketHours::isAfterHours(hourStart)) {
                auto rect = createBackgroundRect(QColor(50, 50, 80, 100), -1);
                QPointF sessionStart = chart->mapToPosition(QPointF(
                    visibleStart.toMSecsSinceEpoch(), axisY->max()));
                QPointF sessionEnd = chart->mapToPosition(QPointF(
                    visibleEnd.toMSecsSinceEpoch(), axisY->max()));
                
                rect->setRect(sessionStart.x(), topLeft.y(),
                            sessionEnd.x() - sessionStart.x(),
                            bottomRight.y() - topLeft.y());
                afterHoursRects.append(rect);
            }
            else if (!MarketHours::isRegularHours(hourStart)) {
                auto rect = createBackgroundRect(QColor(40, 40, 50, 120), -2);
                QPointF sessionStart = chart->mapToPosition(QPointF(
                    visibleStart.toMSecsSinceEpoch(), axisY->max()));
                QPointF sessionEnd = chart->mapToPosition(QPointF(
                    visibleEnd.toMSecsSinceEpoch(), axisY->max()));
                
                rect->setRect(sessionStart.x(), topLeft.y(),
                            sessionEnd.x() - sessionStart.x(),
                            bottomRight.y() - topLeft.y());
                closedMarketRects.append(rect);
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
                QDateTime currentBarTime;
                if (hasOpenBar) {
                    currentBarTime = currentOpenBar.getTimeStamp();
                } else {
                    currentBarTime = completedBars.lastKey();
                }

                // Round current time down to the nearest 5-minute mark
                int currentMinute = currentBarTime.time().minute();
                int baseMinute = (currentMinute / 5) * 5;
                QDateTime baseTime = currentBarTime;
                baseTime.setTime(QTime(currentBarTime.time().hour(), baseMinute, 0));

                // Set start time to 30 minutes before the base time
                QDateTime startTime = baseTime.addSecs(-30 * 60);
                // Set end time to the next 5-minute mark after base time plus a small buffer
                QDateTime endTime = baseTime.addSecs(5 * 60 + 60); // Next 5-min mark + 1 min buffer

                // Reset the horizontal axis
                axisX->setRange(startTime, endTime);

                // Reset the vertical axis to fit visible bars
                double minPrice = std::numeric_limits<double>::max();
                double maxPrice = std::numeric_limits<double>::lowest();
                double currentPrice = 0.0;
                
                // Find min/max prices for bars in the new time range
                for (auto it = completedBars.lowerBound(startTime); 
                     it != completedBars.end() && it.key() <= endTime; 
                     ++it) {
                    const Bar& bar = it.value();
                    minPrice = qMin(minPrice, bar.getLow());
                    maxPrice = qMax(maxPrice, bar.getHigh());
                    if (it == --completedBars.end()) {
                        currentPrice = bar.getClose();
                    }
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

    // Convert pixel movement to time units for X axis
    qreal timePerPixel = (axisX->max().toMSecsSinceEpoch() - axisX->min().toMSecsSinceEpoch()) / chartView->width();
    qint64 timeOffset = -delta.x() * timePerPixel;

    // Convert pixel movement to price units for Y axis
    qreal pricePerPixel = (axisY->max() - axisY->min()) / chartView->height();
    qreal priceOffset = delta.y() * pricePerPixel;

    // Update axes ranges
    QDateTime newMinTime = QDateTime::fromMSecsSinceEpoch(axisX->min().toMSecsSinceEpoch() + timeOffset);
    QDateTime newMaxTime = QDateTime::fromMSecsSinceEpoch(axisX->max().toMSecsSinceEpoch() + timeOffset);
    axisX->setRange(newMinTime, newMaxTime);
    axisY->setRange(axisY->min() + priceOffset, axisY->max() + priceOffset);

    // Check if we need more bars
    checkForMissingBars(newMinTime, newMaxTime);

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
    
    // Reset the axes ranges to default
    QDateTime now = QDateTime::currentDateTime();
    axisX->setRange(now.addSecs(-30 * 60), now.addSecs(5 * 60));
    axisY->setRange(0, 100);
    
    // Flag that we need to focus on price when first bars arrive
    needsInitialPriceFocus = true;
}
