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
#include "MarketHours.h"

#define CANDLESTICK_BODY_WIDTH 0.9 // 90% of available space

Q_LOGGING_CATEGORY(ChartLog, "Chart");

/**
 * @brief Constructs a StockPriceChart widget.
 * 
 * Initializes the chart with candlestick series, last price line, void bar series,
 * axes, and sets up the dark theme. Connects axis range change signals to update
 * background and price line positioning.
 * 
 * @param parent The parent widget, defaults to nullptr.
 */
StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent) {

    // Last price line setup
    lastPriceLine = new QLineSeries();
    lastPriceLine->setPen(QPen(Qt::green, 1, Qt::DashLine)); // Start with green, dashed line

    // Candlestick series setup
    candlestickSeries = new QCandlestickSeries();
    candlestickSeries->setPen(QPen(QColor(Qt::black)));
    candlestickSeries->setIncreasingColor(QColor(Qt::green));
    candlestickSeries->setDecreasingColor(QColor(Qt::red));
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH);

    // Void bar series setup
    voidBarSeries = new QScatterSeries();
    voidBarSeries->setName("Void Bars");
    voidBarSeries->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    voidBarSeries->setMarkerSize(10.0);  // Size of the X marker
    voidBarSeries->setPen(QPen(QColor(Qt::red), 2));  // Red X with thicker lines
    voidBarSeries->setBrush(Qt::NoBrush);  // No fill, just the X outline

    // Adding all series to the chart
    chart = new QChart();
    chart->addSeries(candlestickSeries);
    chart->addSeries(lastPriceLine);
    chart->addSeries(voidBarSeries);

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
    chart->legend()->setVisible(false);

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
    //axisX->setTitleText("Time");
    axisX->setGridLineVisible(true);
    axisX->setMinorGridLineVisible(false);
    axisX->setLabelsAngle(-45); // Angle the time labels for better readability
    axisX->setGridLineColor(QColor(70, 70, 70));
    axisX->setLabelsColor(QColor(220, 220, 220));
    axisX->setTitleBrush(QBrush(QColor(220, 220, 220)));
    axisX->setLabelFormat("");  // We'll use custom labels

    axisX->setRange(-3, 3);
    axisX->setTickCount(7);  // Default tick count
    axisX->setLabelFormat("%d");
    axisX->setTickType(QValueAxis::TicksDynamic);
    axisX->setTickAnchor(0);
    axisX->setTickInterval(1);

    chart->addAxis(axisX, Qt::AlignBottom);
    candlestickSeries->attachAxis(axisX);
    lastPriceLine->attachAxis(axisX);
    voidBarSeries->attachAxis(axisX);  // Attach void bar series to X axis

    axisY = new QValueAxis();
    axisY->setLabelFormat("%.2f");
    //axisY->setTitleText("Price");
    axisY->setGridLineColor(QColor(70, 70, 70));
    axisY->setLabelsColor(QColor(220, 220, 220));
    axisY->setTitleBrush(QBrush(QColor(220, 220, 220)));
    axisY->setTickType(QValueAxis::TicksDynamic);
    axisY->setTickAnchor(0);
    axisY->setTickInterval(0.05);  // 5 cent increments for price ticks
    
    chart->addAxis(axisY, Qt::AlignLeft);
    candlestickSeries->attachAxis(axisY);
    lastPriceLine->attachAxis(axisY);
    voidBarSeries->attachAxis(axisY);  // Attach void bar series to Y axis

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);  // Remove widget margins

    // Create and add the timeframe selector at the top
    timeframeSelector = new TimeFrameSelector(this);
    layout->addWidget(timeframeSelector);

    layout->addWidget(chartView);
    setLayout(layout);

    // Connect to the axis range changed signals directly
    // Background rendering is now optimized with binary search (O(log n) per hour)
    // instead of linear search (O(n) per hour), making it fast enough for real-time updates
    connect(axisX, &QValueAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
    connect(axisX, &QValueAxis::rangeChanged, this, &StockPriceChart::updateLastPriceLineIfNeeded);
    connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
    connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::updateLastPriceLineIfNeeded);
}

/**
 * @brief Destroys the StockPriceChart widget.
 * 
 * Qt's parent-child hierarchy handles cleanup of chart, series, and other Qt objects.
 */
StockPriceChart::~StockPriceChart() {
    // No need to delete chart, series, etc.—handled by Qt parent hierarchy
}

/**
 * @brief Sets the stock symbol for the chart.
 * 
 * Updates the chart title and candlestick series name to display the new symbol.
 * 
 * @param symbol The stock symbol to display (e.g., "AAPL", "GOOGL").
 */
void StockPriceChart::setSymbol(const QString& symbol) {
    this->symbol = symbol;
    candlestickSeries->setName(symbol + " (Bars)");
    chart->setTitle(symbol);
}

/**
 * @brief Adds a new bar to the chart.
 * 
 * Processes the bar based on its status using incremental updates.
 * No longer calls updateChart() for performance - updates are done incrementally.
 * 
 * @param bar The Bar object containing price data and timestamp.
 */
void StockPriceChart::addBar(const Bar& bar) {
    Q_ASSERT(bar.isValid());

    if (bar.getBarStatus() == Bar::BarStatus::Closed) {
        handleClosedBar(bar);
    } else if (bar.getBarStatus() == Bar::BarStatus::Open) {
        handleOpenBar(bar);
    } else {
        qCritical("StockPriceChart: Received bar with unknown status");
    }

    // Note: updateChart() is NOT called here anymore for performance.
    // Individual handlers now do incremental updates instead of full rebuilds.
}

/**
 * @brief Handles the response to a missing bars request.
 * 
 * Processes the received bars, inserting them into the appropriate data structures
 * (completed bars or void bars). Updates the chart display and last price line.
 * Resets the request flag to allow future requests.
 * 
 * @param bars Vector of Bar objects received from the data source.
 */
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

    // thats just to handle the initial pass in this function
    if (lastValidClosePrice == 0.0) {
        lastValidClosePrice = completedBars.first().getOpen();
    }

    // Store current view state to preserve it
    qreal currentMinIndex = axisX->min();
    qreal currentMaxIndex = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = (currentMaxIndex - currentMinIndex) > 0;

    // First, add all historical bars to the index mapping using negative indices.
    // This is O(m) where m = number of new bars, instead of O(n) rebuild.
    // Note: This only creates timestamp → index mappings; bars don't need to be
    // in completedBars/voidBars yet since we only read their timestamps.
    addHistoricalBarsToIndexMapping(bars);
    
    qCDebug(ChartLog) << "After addHistoricalBarsToIndexMapping, index range:" 
                      << (indexToTimestamp.isEmpty() ? "empty" : QString("%1 to %2").arg(indexToTimestamp.firstKey()).arg(indexToTimestamp.lastKey()));

    // Insert all received bars into the completedBars/voidBars maps
    for (const Bar& bar : bars) {
        if (bar.getBarStatus() != Bar::BarStatus::Null) {
            completedBars.insert(bar.getTimeStamp(), bar);
            // Update the last valid close price
            lastValidClosePrice = bar.getClose();
        } else {
            // Store void bar with the timestamp and price
            voidBars.insert(bar.getTimeStamp(), lastValidClosePrice);
        }
    }
    
    // Rebuild candlestick series in sorted index order to ensure correct positioning.
    // QCandlestickSeries positions bars by insertion order, not by timestamp value,
    // so we need to clear and rebuild the series sorted by index.
    candlestickSeries->clear();
    voidBarSeries->clear();
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH);
    
    qCDebug(ChartLog) << "Rebuilding candlestick series with" << indexToTimestamp.size() << "total bars";
    
    // Add all bars in sorted index order
    int addedCount = 0;
    for (auto it = indexToTimestamp.constBegin(); it != indexToTimestamp.constEnd(); ++it) {
        int index = it.key();
        const QDateTime& timestamp = it.value();
        
        // Check if it's a void bar
        if (voidBars.contains(timestamp)) {
            double price = voidBars[timestamp];
            voidBarSeries->append(index, price);
            if (addedCount < 5 || index < 0) {
                qCDebug(ChartLog) << "  Adding void bar at index" << index << "time" << timestamp.toString("hh:mm:ss");
            }
            addedCount++;
        }
        // Check if it's a completed bar
        else if (completedBars.contains(timestamp)) {
            const Bar& bar = completedBars[timestamp];
            auto set = new QCandlestickSet();
            Q_CHECK_PTR(set);
            set->setTimestamp(index);
            set->setOpen(bar.getOpen());
            set->setHigh(bar.getHigh());
            set->setLow(bar.getLow());
            set->setClose(bar.getClose());
            candlestickSeries->append(set);
            if (addedCount < 5 || index < 0) {
                qCDebug(ChartLog) << "  Adding candlestick at index" << index << "time" << timestamp.toString("hh:mm:ss") << "O/H/L/C:" << bar.getOpen() << bar.getHigh() << bar.getLow() << bar.getClose();
            }
            addedCount++;
        }
        // Check if it's the current open bar
        else if (hasOpenBar && timestamp == currentOpenBar.getTimeStamp()) {
            auto set = new QCandlestickSet();
            Q_CHECK_PTR(set);
            set->setTimestamp(index);
            set->setOpen(currentOpenBar.getOpen());
            set->setHigh(currentOpenBar.getHigh());
            set->setLow(currentOpenBar.getLow());
            set->setClose(currentOpenBar.getClose());
            candlestickSeries->append(set);
            qCDebug(ChartLog) << "  Adding open bar at index" << index << "time" << timestamp.toString("hh:mm:ss");
            addedCount++;
        }
    }

    // Maintain the bar limit
    maintainBarLimit();

    // Restore the view state if it was initialized
    if (hadInitialView) {
        axisX->setRange(currentMinIndex, currentMaxIndex);
        axisY->setRange(currentYMin, currentYMax);
        updateAxisLabels();
    }

    // Update the last price line if needed
    if (!bars.isEmpty()) {
        const Bar& lastBar = bars.last();
        updateLastPriceLine(lastBar.getClose(), lastBar.getClose() >= lastBar.getOpen());
    }

    // Update the after-hours background
    updateAfterHoursBackground();
}

/**
 * @brief Processes a closed bar (completed trading period).
 * 
 * Stores the closed bar in the completed bars map. Uses incremental updates
 * instead of full chart rebuild for better performance.
 * 
 * @param bar The closed Bar object to process.
 */
void StockPriceChart::handleClosedBar(const Bar& bar) {
    // Store current view state (using indices)
    qreal currentMinIndex = axisX->min();
    qreal currentMaxIndex = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = (currentMaxIndex - currentMinIndex) > 0;
    bool isFirstBar = !hasOpenBar && completedBars.isEmpty();

    if (!hasOpenBar) {
        // No open bar - just add the closed bar
        completedBars.insert(bar.getTimeStamp(), bar);
        updateIndexMappingIncremental(bar.getTimeStamp());
        addNewCandlestick(bar.getTimeStamp(), bar);
        maintainBarLimit();
        
        // If this is the very first bar, set up initial view
        if (isFirstBar) {
            int index = getIndexForTimestamp(bar.getTimeStamp());
            axisX->setRange(index - 30, index + 1);  // Allow negative indices
            
            // Set Y axis range with padding
            double price = bar.getClose();
            double padding = price * 0.0002;
            double minRange = price * 0.0005;
            axisY->setRange(qMax(0.0, price - minRange/2 - padding), 
                           price + minRange/2 + padding);
            updateAxisLabels();
        }
        return;
    }

    QDateTime openBarTime = currentOpenBar.getTimeStamp();
    QDateTime closedBarTime = bar.getTimeStamp();

    if (openBarTime != closedBarTime) {
        // Different timestamps - move open bar to completed, then add closed bar
        completedBars.insert(openBarTime, currentOpenBar);
        completedBars.insert(closedBarTime, bar);
        updateIndexMappingIncremental(closedBarTime);
        addNewCandlestick(closedBarTime, bar);
    } else {
        // Same timestamp - the open bar became closed, update the existing candlestick
        completedBars.insert(closedBarTime, bar);
        QCandlestickSet* existingSet = findCandlestickSetByTimestamp(closedBarTime);
        if (existingSet) {
            existingSet->setOpen(bar.getOpen());
            existingSet->setHigh(bar.getHigh());
            existingSet->setLow(bar.getLow());
            existingSet->setClose(bar.getClose());
        } else {
            // Shouldn't happen, but add it if missing
            addNewCandlestick(closedBarTime, bar);
        }
    }
    
    hasOpenBar = false;
    maintainBarLimit();

    // Restore view state if it was initialized
    if (hadInitialView) {
        axisX->setRange(currentMinIndex, currentMaxIndex);
        axisY->setRange(currentYMin, currentYMax);
        updateAxisLabels();
    }
}

/**
 * @brief Maintains the maximum number of bars limit.
 * 
 * Removes the oldest bars (both completed and void bars) when the total
 * exceeds MAX_BARS to prevent memory issues and maintain performance.
 * Also cleans up the index mappings for removed bars.
 */
void StockPriceChart::maintainBarLimit() {
    // Count total bars (completed + void)
    int totalBars = completedBars.size() + voidBars.size();
    
    while (totalBars > MAX_BARS) {
        // Find the oldest timestamp across both maps
        QDateTime oldestCompletedTime = completedBars.isEmpty() ? QDateTime() : completedBars.firstKey();
        QDateTime oldestVoidTime = voidBars.isEmpty() ? QDateTime() : voidBars.firstKey();
        
        QDateTime oldestTime;
        
        // Remove the older one
        if (oldestCompletedTime.isValid() && 
            (!oldestVoidTime.isValid() || oldestCompletedTime < oldestVoidTime)) {
            oldestTime = oldestCompletedTime;
            completedBars.erase(completedBars.begin());
        } else if (oldestVoidTime.isValid()) {
            oldestTime = oldestVoidTime;
            voidBars.erase(voidBars.begin());
        }
        
        // Clean up the index mapping for the removed bar
        if (oldestTime.isValid() && timestampToIndex.contains(oldestTime)) {
            int removedIndex = timestampToIndex[oldestTime];
            timestampToIndex.remove(oldestTime);
            indexToTimestamp.remove(removedIndex);
        }
        
        totalBars--;
    }
}

/**
 * @brief Processes an open bar (currently active trading period).
 * 
 * Updates the current open bar data and manages the transition between
 * different open bars. Uses incremental updates instead of full chart rebuild.
 * 
 * @param bar The open Bar object to process.
 */
void StockPriceChart::handleOpenBar(const Bar& bar) {
    QDateTime newBarTime = bar.getTimeStamp();
    double newPrice = bar.getClose();
    bool isFirstBar = !hasOpenBar && completedBars.isEmpty();

    if (!hasOpenBar) {
        // First open bar - add to index mapping and create candlestick
        currentOpenBar = bar;
        hasOpenBar = true;
        updateIndexMappingIncremental(newBarTime);
        addNewCandlestick(newBarTime, bar);
        updateLastPriceLine(newPrice, newPrice >= lastPrice);
        
        // If this is the very first bar, set up initial view
        if (isFirstBar) {
            int index = getIndexForTimestamp(newBarTime);
            axisX->setRange(index - 30, index + 1);  // Allow negative indices
            
            // Set Y axis range with padding
            double padding = newPrice * 0.0002;
            double minRange = newPrice * 0.0005;
            axisY->setRange(qMax(0.0, newPrice - minRange/2 - padding), 
                           newPrice + minRange/2 + padding);
            updateAxisLabels();
        }
        return;
    }

    QDateTime currentBarTime = currentOpenBar.getTimeStamp();
    if (newBarTime != currentBarTime) {
        // New bar started - move old open bar to completed
        completedBars.insert(currentBarTime, currentOpenBar);
        currentOpenBar = bar;
        updateIndexMappingIncremental(newBarTime);
        addNewCandlestick(newBarTime, bar);
    } else {
        // Same bar updated - just update the candlestick
        currentOpenBar = bar;
        updateOpenBarCandlestick();
    }

    updateLastPriceLine(newPrice, newPrice >= currentOpenBar.getOpen());
}

/**
 * @brief Updates the entire chart display.
 * 
 * Rebuilds the index mapping, clears and repopulates all series (candlesticks,
 * void bars), calculates appropriate axis ranges based on visible data,
 * and preserves the current view state when possible.
 */
void StockPriceChart::updateChart() {
    // Store current view state (now using indices)
    qreal currentMinIndex = axisX->min();
    qreal currentMaxIndex = axisX->max();
    qreal currentYMin = axisY->min();
    qreal currentYMax = axisY->max();
    bool hadInitialView = (currentMaxIndex - currentMinIndex) > 0;

    // Rebuild the index mapping
    rebuildIndexMapping();

    // Clear existing candlesticks and void bars
    candlestickSeries->clear();
    candlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH); // Reset body width after clearing
    voidBarSeries->clear();

    // Calculate marker size based on candlestick width
    qreal markerSize = CANDLESTICK_BODY_WIDTH * 0.8; // Make it slightly smaller than candlestick width
    voidBarSeries->setMarkerSize(markerSize);

    // Add all bars (completed and void) using their indices
    for (auto it = indexToTimestamp.constBegin(); it != indexToTimestamp.constEnd(); ++it) {
        int index = it.key();
        const QDateTime& timestamp = it.value();
        
        // Check if it's a void bar
        if (voidBars.contains(timestamp)) {
            // Add void bar to scatter series
            double price = voidBars[timestamp];
            voidBarSeries->append(index, price);
        }
        // Check if it's a completed bar
        else if (completedBars.contains(timestamp)) {
            const Bar& bar = completedBars[timestamp];
            auto set = new QCandlestickSet();
            set->setTimestamp(index);  // Use index instead of timestamp
            set->setOpen(bar.getOpen());
            set->setHigh(bar.getHigh());
            set->setLow(bar.getLow());
            set->setClose(bar.getClose());
            candlestickSeries->append(set);
        }
        // Check if it's the current open bar
        else if (hasOpenBar && timestamp == currentOpenBar.getTimeStamp()) {
            auto set = new QCandlestickSet();
            set->setTimestamp(index);  // Use index instead of timestamp
            set->setOpen(currentOpenBar.getOpen());
            set->setHigh(currentOpenBar.getHigh());
            set->setLow(currentOpenBar.getLow());
            set->setClose(currentOpenBar.getClose());
            candlestickSeries->append(set);
        }
    }

    // Only update time axis range if this is the initial setup
    if (!hadInitialView && !indexToTimestamp.isEmpty()) {
        int lastIndex = indexToTimestamp.lastKey();
        int startIndex = lastIndex - 30;  // Show last 30 bars (can be negative)
        int endIndex = lastIndex + 1;  // Small buffer
        
        axisX->setRange(startIndex, endIndex);   
    } else {
        // Restore the previous view
        axisX->setRange(currentMinIndex, currentMaxIndex);
        axisY->setRange(currentYMin, currentYMax);
    }

    updateAxisLabels();

    // Calculate current visible price range efficiently
    // Use binary search to find only the visible bars instead of iterating all bars
    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();
    double currentPrice = 0.0;

    // Get visible range in indices
    int visibleStartIndex = static_cast<int>(axisX->min());
    int visibleEndIndex = static_cast<int>(axisX->max());
    
    // Use lowerBound to find first visible bar efficiently - O(log n) instead of O(n)
    auto startIt = indexToTimestamp.lowerBound(visibleStartIndex);
    auto endIt = indexToTimestamp.upperBound(visibleEndIndex);
    
    // Iterate only through visible bars - O(m) where m = visible bars
    for (auto it = startIt; it != endIt; ++it) {
        const QDateTime& timestamp = it.value();
        
        // Check completed bars
        if (completedBars.contains(timestamp)) {
            const Bar& bar = completedBars[timestamp];
            minPrice = qMin(minPrice, bar.getLow());
            maxPrice = qMax(maxPrice, bar.getHigh());
            currentPrice = bar.getClose();
        }
        // Check open bar
        else if (hasOpenBar && timestamp == currentOpenBar.getTimeStamp()) {
            minPrice = qMin(minPrice, currentOpenBar.getLow());
            maxPrice = qMax(maxPrice, currentOpenBar.getHigh());
            currentPrice = currentOpenBar.getClose();
        }
        // Check void bars
        else if (voidBars.contains(timestamp)) {
            double price = voidBars[timestamp];
            minPrice = qMin(minPrice, price);
            maxPrice = qMax(maxPrice, price);
            currentPrice = price;
        }
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
        axisY->setRange(qMax(0.0, minPrice - padding), maxPrice + padding);
    }
}

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
}

/**
 * @brief Updates the last price line if there are candlesticks available.
 * 
 * Uses the last candlestick's close and open prices to determine the line color
 * and position. Called when axis ranges change.
 */
void StockPriceChart::updateLastPriceLineIfNeeded() {
    // Update the price line regardless of whether there's an open bar
    if (candlestickSeries->count() > 0) {
        auto lastSet = candlestickSeries->sets().last();
        double closePrice = lastSet->close();
        double openPrice = lastSet->open();
        updateLastPriceLine(closePrice, closePrice >= openPrice);
    }
}

/**
 * @brief Updates the position of the price label on the chart.
 * 
 * Positions the price label in the right margin of the chart, aligned with
 * the last price line. Keeps the label within the plot area bounds.
 */
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

/**
 * @brief Updates the horizontal last price line across the visible chart area.
 * 
 * Creates a horizontal dashed line at the specified price level spanning the
 * current visible X-axis range. Sets the line color based on price movement
 * (green for uptick, red for downtick) and updates the price label.
 * 
 * @param price The price level for the line.
 * @param isUpTick True if price is moving up (green line), false for down (red line).
 */
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

/**
 * @brief Checks if the given time is after market hours.
 * 
 * Converts the local time to New York timezone and checks if it's after 4 PM ET.
 * 
 * @param localTime The time to check in local timezone.
 * @return True if the time is after 4 PM ET, false otherwise.
 */
bool StockPriceChart::isAfterMarketHours(const QDateTime& localTime) {
    // Convert local time to New York time
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = localTime.toTimeZone(nyZone);
    
    // Check if it's after 4 PM (16:00) NY time
    return nyTime.time().hour() >= 16;
}

/**
 * @brief Updates the background rectangles for different market sessions.
 * 
 * Draws colored background rectangles to indicate:
 * - Pre-market hours (brown)
 * - After-hours (blue)
 * - Closed market periods including weekends (dark gray)
 * 
 * Only draws backgrounds for the currently visible time range.
 */
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

        // Consolidate consecutive hours of the same session type into single rectangles
        // This eliminates visual artifacts from overlapping hourly rectangles
        int hour = 0;
        while (hour < 24) {
            QDateTime hourStart = QDateTime(currentDate, QTime(hour, 0), nyZone);
            
            // Determine the session type for this hour
            bool isPreMarket = MarketHours::isPreMarket(hourStart);
            bool isAfterHours = MarketHours::isAfterHours(hourStart);
            bool isRegularHours = MarketHours::isRegularHours(hourStart);
            
            // Find the end of this session by scanning forward
            int sessionEndHour = hour;
            while (sessionEndHour < 24) {
                QDateTime testTime = QDateTime(currentDate, QTime(sessionEndHour, 0), nyZone);
                bool sameSession = (isPreMarket && MarketHours::isPreMarket(testTime)) ||
                                  (isAfterHours && MarketHours::isAfterHours(testTime)) ||
                                  (isRegularHours && MarketHours::isRegularHours(testTime)) ||
                                  (!isPreMarket && !isAfterHours && !isRegularHours && 
                                   !MarketHours::isPreMarket(testTime) && 
                                   !MarketHours::isAfterHours(testTime) && 
                                   !MarketHours::isRegularHours(testTime));
                
                if (!sameSession) {
                    break;
                }
                sessionEndHour++;
            }
            
            // Create single rectangle for the entire session
            QDateTime sessionStart = QDateTime(currentDate, QTime(hour, 0), nyZone);
            QDateTime sessionEnd = sessionEndHour == 24 ? 
                QDateTime(currentDate, QTime(23, 59, 59), nyZone) :
                QDateTime(currentDate, QTime(sessionEndHour, 0), nyZone);
            
            QDateTime localSessionStart = sessionStart.toLocalTime();
            QDateTime localSessionEnd = sessionEnd.toLocalTime();
            
            // Draw one rectangle for the entire session instead of one per hour
            if (isPreMarket) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(90, 60, 30, 100), -1, preMarketRects);
            }
            else if (isAfterHours) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(50, 50, 80, 100), -1, afterHoursRects);
            }
            else if (!isRegularHours) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(40, 40, 50, 120), -2, closedMarketRects);
            }
            
            // Move to the next session
            hour = sessionEndHour;
        }
        
        currentDate = currentDate.addDays(1);
    }
}

/**
 * @brief Creates a background rectangle with specified color and Z-value.
 * 
 * @param color The fill color for the rectangle.
 * @param zValue The stacking order (higher values appear on top).
 * @return Pointer to the created QGraphicsRectItem.
 */
QGraphicsRectItem* StockPriceChart::createBackgroundRect(const QColor& color, int zValue) {
    QGraphicsRectItem* rect = new QGraphicsRectItem(chart);
    rect->setBrush(color);
    rect->setPen(Qt::NoPen);
    rect->setZValue(zValue);
    return rect;
}

/**
 * @brief Clears all background rectangles from the chart.
 * 
 * Removes and deletes all after-hours, pre-market, and closed market background rectangles.
 */
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

/**
 * @brief Checks if the current view requires missing bars to be loaded.
 * 
 * When the view extends beyond available data, emits a signal to request
 * missing bars from the data source. Only one request can be active at a time.
 * Adjusts request times to valid trading hours (6AM-8PM weekdays).
 * 
 * @param viewStartTime The start time of the current view.
 * @param viewEndTime The end time of the current view (unused).
 */
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime) {
    Q_UNUSED(viewEndTime);

    QDateTime viewStartTimeRounded = viewStartTime.addSecs(-viewStartTime.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTime.time().msec());
    
    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);
    
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


        qCDebug(ChartLog) << "Requesting missing bars from"
                          << viewStartTimeRounded.toString(Qt::ISODate)
                          << "to"
                          << firstBarTime.toString(Qt::ISODate);

        Q_ASSERT(viewStartTimeRounded < firstBarTime);

        emit requestMissingBars(viewStartTimeRounded, firstBarTime);
    }
}

/**
 * @brief Clears all data and resets the chart for a new symbol.
 * 
 * Removes all bars, series data, background rectangles, and resets axes
 * to default ranges. Prepares the chart for displaying a new stock symbol.
 */
void StockPriceChart::clearSymbol() {
    // Clear the candlestick series
    candlestickSeries->clear();
    
    // Clear the completed bars
    completedBars.clear();
    
    // Clear void bars
    voidBars.clear();
    
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
    chart->setTitle(symbol);
    
    // Clear index mappings
    indexToTimestamp.clear();
    timestampToIndex.clear();
    
    // Reset the axes ranges to default
    axisX->setRange(0, 30);
    axisY->setRange(0, 100);
}

/**
 * @brief Rebuilds the index-to-timestamp mappings for continuous display.
 * 
 * Creates indices for all timestamps (completed bars, void bars, and open bar) 
 * to enable gapless chart display across time periods. When existing indices 
 * are present, preserves the existing index origin (index 0) position by 
 * rebuilding around it. Otherwise, starts from index 0.
 * 
 * Maintains bidirectional mapping between indices and timestamps.
 */
void StockPriceChart::rebuildIndexMapping() {
    // Save the current origin timestamp (what was at index 0) if it exists
    QDateTime originTimestamp;
    bool hadOrigin = false;
    if (indexToTimestamp.contains(0)) {
        originTimestamp = indexToTimestamp[0];
        hadOrigin = true;
    }
    
    indexToTimestamp.clear();
    timestampToIndex.clear();
    
    // Create a combined sorted list of all timestamps (completed bars + void bars)
    QMap<QDateTime, bool> allTimestamps;  // timestamp -> isVoid
    
    for (auto it = completedBars.constBegin(); it != completedBars.constEnd(); ++it) {
        allTimestamps[it.key()] = false;
    }
    
    for (auto it = voidBars.constBegin(); it != voidBars.constEnd(); ++it) {
        allTimestamps[it.key()] = true;
    }
    
    // Add open bar if it exists
    if (hasOpenBar) {
        allTimestamps[currentOpenBar.getTimeStamp()] = false;
    }
    
    if (allTimestamps.isEmpty()) {
        return;  // Nothing to map
    }
    
    // Find where to start indexing based on origin preservation
    int startIndex = 0;
    
    if (hadOrigin && allTimestamps.contains(originTimestamp)) {
        // Count how many timestamps come before the origin (they'll get negative indices)
        // Iterate and count instead of using std::distance for O(n) but more Qt-idiomatic
        int timestampsBeforeOrigin = 0;
        for (auto it = allTimestamps.constBegin(); it != allTimestamps.constEnd(); ++it) {
            if (it.key() == originTimestamp) {
                break;
            }
            ++timestampsBeforeOrigin;
        }
        startIndex = -timestampsBeforeOrigin;
    }
    
    // Build index mapping starting from startIndex
    int index = startIndex;
    for (auto it = allTimestamps.constBegin(); it != allTimestamps.constEnd(); ++it) {
        const QDateTime& timestamp = it.key();
        indexToTimestamp[index] = timestamp;
        timestampToIndex[timestamp] = index;
        index++;
    }
}

/**
 * @brief Gets the index corresponding to a timestamp.
 * 
 * Returns the exact index if the timestamp exists in the mapping, otherwise
 * returns the index of the closest earlier bar. If the timestamp is before
 * all available bars, returns the first available index (which could be negative),
 * serving as a boundary for extrapolation by getTimestampForIndex().
 * 
 * @param timestamp The timestamp to find the index for.
 * @return The index for the timestamp, or the closest available index if exact match not found.
 */
int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const {
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end()) {
        return it.value();
    }
    
    // If exact timestamp not found, find the closest index
    if (timestampToIndex.isEmpty()) {
        return 0;  // Default when no bars exist yet
    }
    
    // Find the first timestamp greater than or equal to the given timestamp
    auto upper = timestampToIndex.upperBound(timestamp);
    if (upper == timestampToIndex.begin()) {
        // Before all bars - return the first available index (could be negative)
        return indexToTimestamp.firstKey();
    }
    
    // Return the index of the previous bar
    --upper;
    return upper.value();
}

/**
 * @brief Gets the timestamp corresponding to an index.
 * 
 * For indices within the mapped range, returns the exact timestamp.
 * For indices outside the range, extrapolates assuming 1-minute bars,
 * skipping over non-trading hours (weekends and 8PM-6AM gaps).
 * 
 * @param index The index to find the timestamp for.
 * @return The timestamp for the index.
 */
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
        // Extrapolate backwards - skip over non-trading hours
        int firstIndex = indexToTimestamp.firstKey();
        QDateTime currentTime = indexToTimestamp.first();
        int deltaIndex = firstIndex - index;
        
        // Walk backwards minute by minute, skipping non-trading hours
        for (int i = 0; i < deltaIndex; ++i) {
            currentTime = getPreviousTradingMinute(currentTime);
        }
        
        return currentTime;
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

/**
 * @brief Gets the previous valid trading minute, skipping non-trading hours.
 * 
 * Given a timestamp, returns the timestamp of the previous 1-minute bar,
 * skipping over closed market periods (8PM-6AM overnight gaps and weekends).
 * Uses America/New_York timezone for market hours calculations.
 * 
 * @param timestamp The current timestamp.
 * @return The previous trading minute timestamp.
 */
QDateTime StockPriceChart::getPreviousTradingMinute(const QDateTime& timestamp) const {
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = timestamp.toTimeZone(nyZone);
    QDateTime previousMinute = nyTime.addSecs(-60);
    
    // Check if we crossed into a closed period
    QTime time = previousMinute.time();
    int dayOfWeek = previousMinute.date().dayOfWeek();
    
    // If we're in the closed period (before 6AM or after 8PM on a weekday, or weekend)
    if (dayOfWeek >= MONDAY && dayOfWeek <= FRIDAY) {
        // Weekday
        if (time < QTime(TRADING_START_HOUR, 0, 0)) {
            // Before 6AM - jump to 7:59PM previous day
            QDateTime result = QDateTime(previousMinute.date().addDays(-1), 
                                        QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                                        nyZone);
            // If that previous day is a weekend, jump to Friday 7:59PM
            if (result.date().dayOfWeek() > FRIDAY) {
                QDate friday = getPreviousFriday(result.date());
                result = QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), nyZone);
            }
            return result.toTimeZone(timestamp.timeZone());
        } else if (time >= QTime(TRADING_END_HOUR, 0, 0)) {
            // After 8PM - move to 7:59PM same day
            // This case shouldn't occur with valid input, but we handle it defensively
            qCDebug(ChartLog) << "Unexpected: getPreviousTradingMinute called with time after 8PM:" << nyTime;
            return QDateTime(previousMinute.date(), 
                           QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                           nyZone).toTimeZone(timestamp.timeZone());
        }
    } else {
        // Weekend - jump to Friday 7:59PM
        QDate friday = getPreviousFriday(previousMinute.date());
        return QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    // Normal case - previous minute is within trading hours
    return previousMinute.toTimeZone(timestamp.timeZone());
}

/**
 * @brief Adjusts a timestamp to the nearest valid trading time.
 * 
 * If the timestamp falls in a closed market period (8PM-6AM or weekend),
 * adjusts it to the nearest valid trading time. For times before 6AM,
 * moves to the previous day's 7:59PM. For weekends, moves to Friday 7:59PM.
 * 
 * @param timestamp The timestamp to adjust.
 * @return The adjusted timestamp within valid trading hours (6AM-8PM weekdays).
 */
QDateTime StockPriceChart::adjustToValidTradingTime(const QDateTime& timestamp) const {
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = timestamp.toTimeZone(nyZone);
    QTime time = nyTime.time();
    int dayOfWeek = nyTime.date().dayOfWeek();
    
    // Handle weekends
    if (dayOfWeek > FRIDAY) {
        // Weekend - move to Friday 7:59PM
        QDate friday = getPreviousFriday(nyTime.date());
        return QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    // Handle weekday times outside trading hours
    if (time < QTime(TRADING_START_HOUR, 0, 0)) {
        // Before 6AM - move to previous day's 7:59PM
        QDate previousDay = nyTime.date().addDays(-1);
        // If previous day is weekend, move to Friday
        if (previousDay.dayOfWeek() > FRIDAY) {
            previousDay = getPreviousFriday(previousDay);
        }
        return QDateTime(previousDay, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    } else if (time >= QTime(TRADING_END_HOUR, 0, 0)) {
        // After 8PM - move to 7:59PM same day
        return QDateTime(nyTime.date(), QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    // Already in valid trading hours
    return timestamp;
}

/**
 * @brief Gets the previous Friday given a date.
 * 
 * Calculates the most recent Friday before or equal to the given date.
 * Used for adjusting weekend dates to the last trading day.
 * 
 * @param date The date to start from.
 * @return The date of the previous Friday.
 */
QDate StockPriceChart::getPreviousFriday(const QDate& date) const {
    int dayOfWeek = date.dayOfWeek();
    if (dayOfWeek == FRIDAY) {
        return date;  // Already Friday
    } else if (dayOfWeek > FRIDAY) {
        // Saturday or Sunday - go back to Friday
        return date.addDays(-(dayOfWeek - FRIDAY));
    } else {
        // Monday-Thursday - go back to previous Friday
        return date.addDays(-(dayOfWeek + 2));
    }
}

/**
 * @brief Updates the X-axis tick count based on the visible range.
 * 
 * Adjusts the number of tick marks on the X-axis depending on the zoom level
 * to provide appropriate labeling density. Currently uses automatic labeling
 * showing index numbers.
 */
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
    
    qCDebug(ChartLog) << "Setting X-axis tick count to" << numLabels << "for range" << range;
    axisX->setTickCount(numLabels);

    // FIXME this is a hack, continue modifying the a variable
    // Additionally, adjust tick interval based on chart width and range
    {
        qreal range = axisX->max() - axisX->min();
        int chartWidth = chartView->width();
        qreal rangeRatio = chartWidth / range;

        int a = 0;

        if (rangeRatio <= 2){

        }
        else if (rangeRatio <= 5){
            a = 6;
        }
        else if (rangeRatio <= 10){
            a = 3;
        }
        else if (rangeRatio <= 20){
            a = 2;
        }
        else if (rangeRatio <= 30) {
            a = 1;
        }
        axisX->setTickInterval(a + 1);
    }

    // Update Y-axis labels similarly
    qreal minPrice = axisY->min();
    qreal maxPrice = axisY->max();
    qreal priceRange = maxPrice - minPrice;
    
    if (priceRange > 0) {
        int numLabelsY = 5;  // Default
        if (priceRange < 1) {
            numLabelsY = 3;
        } else if (priceRange < 5) {
            numLabelsY = 5;
        } else if (priceRange < 10) {
            numLabelsY = 7;
        } else {
            numLabelsY = 10;
        }
        
        qCDebug(ChartLog) << "Setting Y-axis tick count to" << numLabelsY << "for price range" << priceRange;
        axisY->setTickCount(numLabelsY);
        
        // Set tick interval to 5 cents
        axisY->setTickInterval(0.05);
    }

    // Qt doesn't provide easy custom labels for QValueAxis, so we'll rely on
    // the automatic labeling showing indices. For a production version, you
    // could use QCategoryAxis or custom drawing, but that's beyond minimal changes.
    // The axis will show index numbers which is acceptable for now.
}

/**
 * @brief Draws a background rectangle for a specific time range.
 * 
 * Creates a colored background rectangle covering the bars that fall within
 * the specified time range, clipped to the currently visible area.
 * Uses efficient binary search (lowerBound/upperBound) instead of linear iteration.
 * 
 * @param rangeStart The start time of the range to highlight.
 * @param rangeEnd The end time of the range to highlight.
 * @param color The color for the background rectangle.
 * @param zValue The Z-order for layering (higher values appear on top).
 * @param rectList The list to add the created rectangle to.
 */
void StockPriceChart::drawBackgroundForTimeRange(const QDateTime& rangeStart, const QDateTime& rangeEnd,
                                                   const QColor& color, int zValue,
                                                   QList<QGraphicsRectItem*>& rectList) {
    if (indexToTimestamp.isEmpty()) {
        return;
    }
    
    // Use efficient binary search to find indices instead of linear iteration
    // Find first timestamp >= rangeStart
    auto startIt = timestampToIndex.lowerBound(rangeStart);
    
    // Find first timestamp > rangeEnd
    auto endIt = timestampToIndex.upperBound(rangeEnd);
    
    // Check if we found any bars in this range
    if (startIt == timestampToIndex.end() || startIt.key() > rangeEnd) {
        return; // No bars in this range
    }
    
    int startIndex = startIt.value();
    int endIndex = -1;
    
    // Get the last valid index in range
    if (endIt != timestampToIndex.begin()) {
        --endIt;
        endIndex = endIt.value();
    } else {
        return; // No bars in range
    }
    
    // Verify we have a valid range
    if (startIndex == -1 || endIndex == -1 || endIndex < startIndex) {
        return;
    }
    
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

/**
 * @brief Updates index mapping incrementally for a single timestamp.
 * 
 * Instead of rebuilding the entire mapping, this adds a new timestamp
 * to the existing mappings. This is O(log n) instead of O(n log n).
 * Only works when adding bars in chronological order.
 * 
 * @param timestamp The timestamp to add to the mapping.
 */
void StockPriceChart::updateIndexMappingIncremental(const QDateTime& timestamp) {
    // Check if timestamp already exists
    if (timestampToIndex.contains(timestamp)) {
        return;  // Already in mapping
    }
    
    // Add to the end of the mapping (assumes chronological order)
    int newIndex = indexToTimestamp.isEmpty() ? 0 : indexToTimestamp.lastKey() + 1;
    
    // However, we need to handle the case where the timestamp is not at the end
    // For now, if the timestamp is older than the last one, we need to rebuild
    if (!indexToTimestamp.isEmpty() && timestamp < indexToTimestamp.last()) {
        // Timestamp is out of order, need full rebuild
        rebuildIndexMapping();
        return;
    }
    
    indexToTimestamp[newIndex] = timestamp;
    timestampToIndex[timestamp] = newIndex;
}

/**
 * @brief Adds historical bars to the index mapping using negative indices.
 * 
 * This method implements the bidirectional index system where historical bars
 * (earlier than the first existing bar) are assigned negative indices going
 * backwards: -1, -2, -3, etc. This avoids the O(n) cost of shifting all
 * existing positive indices when adding historical data.
 * 
 * All bars (including void bars) are added to the index mapping to maintain
 * proper chronological ordering in the index space.
 * 
 * The bars vector is expected to be sorted chronologically (oldest to newest),
 * but they are processed in reverse order (newest to oldest) to assign negative
 * indices correctly going backwards from the current minimum index.
 * 
 * @param bars Vector of historical bars to add (should be sorted by timestamp oldest to newest).
 */
void StockPriceChart::addHistoricalBarsToIndexMapping(const QVector<Bar>& bars) {
    if (bars.isEmpty()) {
        return;
    }
    
    // Get the current minimum index (could be negative or 0)
    int minIndex = indexToTimestamp.isEmpty() ? 0 : indexToTimestamp.firstKey();
    
    qCDebug(ChartLog) << "addHistoricalBarsToIndexMapping: adding" << bars.size() << "bars, starting minIndex:" << minIndex;
    
    // Process bars in reverse chronological order (newest to oldest)
    // so we can assign negative indices going backwards.
    // We decrement the index only when we actually add a bar to avoid gaps.
    for (auto it = bars.rbegin(); it != bars.rend(); ++it) {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();
        
        // Skip if already in mapping (shouldn't happen in normal flow,
        // but protects against duplicate insertions)
        if (timestampToIndex.contains(timestamp)) {
            qCDebug(ChartLog) << "  Skipping duplicate timestamp" << timestamp.toString("hh:mm:ss");
            continue;
        }
        
        // Assign the next negative index for all bars (including void bars)
        // Decrement BEFORE assignment so the first bar gets minIndex-1
        --minIndex;
        indexToTimestamp[minIndex] = timestamp;
        timestampToIndex[timestamp] = minIndex;
        
        if (it - bars.rbegin() >= bars.size() - 3 || minIndex >= -3) {
            qCDebug(ChartLog) << "  Assigned index" << minIndex << "to timestamp" << timestamp.toString("hh:mm:ss");
        }
    }
    
    qCDebug(ChartLog) << "addHistoricalBarsToIndexMapping: completed, new minIndex:" << minIndex;
}

/**
 * @brief Updates the candlestick for the current open bar.
 * 
 * Finds and updates the existing candlestick set for the open bar
 * instead of rebuilding the entire series. This is O(1) instead of O(n).
 */
void StockPriceChart::updateOpenBarCandlestick() {
    if (!hasOpenBar) {
        return;
    }
    
    // Find the candlestick set for the open bar
    QCandlestickSet* openSet = findCandlestickSetByTimestamp(currentOpenBar.getTimeStamp());
    
    if (openSet) {
        // Update existing set
        openSet->setOpen(currentOpenBar.getOpen());
        openSet->setHigh(currentOpenBar.getHigh());
        openSet->setLow(currentOpenBar.getLow());
        openSet->setClose(currentOpenBar.getClose());
    } else {
        // Set doesn't exist yet, add it
        int index = getIndexForTimestamp(currentOpenBar.getTimeStamp());
        auto set = new QCandlestickSet();
        set->setTimestamp(index);
        set->setOpen(currentOpenBar.getOpen());
        set->setHigh(currentOpenBar.getHigh());
        set->setLow(currentOpenBar.getLow());
        set->setClose(currentOpenBar.getClose());
        candlestickSeries->append(set);
    }
}

/**
 * @brief Adds a new candlestick to the series.
 * 
 * Appends a single candlestick set to the series instead of rebuilding everything.
 * This is O(1) instead of O(n).
 * 
 * @param timestamp The timestamp of the bar.
 * @param bar The bar data to add.
 */
void StockPriceChart::addNewCandlestick(const QDateTime& timestamp, const Bar& bar) {
    int index = getIndexForTimestamp(timestamp);
    
    auto set = new QCandlestickSet();
    set->setTimestamp(index);
    set->setOpen(bar.getOpen());
    set->setHigh(bar.getHigh());
    set->setLow(bar.getLow());
    set->setClose(bar.getClose());
    candlestickSeries->append(set);
}

/**
 * @brief Finds a candlestick set by its timestamp.
 * 
 * Searches through the candlestick series to find the set with the given timestamp.
 * Returns nullptr if not found.
 * 
 * @param timestamp The timestamp to search for.
 * @return Pointer to the candlestick set, or nullptr if not found.
 */
QCandlestickSet* StockPriceChart::findCandlestickSetByTimestamp(const QDateTime& timestamp) const {
    int targetIndex = getIndexForTimestamp(timestamp);
    
    // Search through the series sets
    for (QCandlestickSet* set : candlestickSeries->sets()) {
        if (set && static_cast<int>(set->timestamp()) == targetIndex) {
            return set;
        }
    }
    
    return nullptr;
}
