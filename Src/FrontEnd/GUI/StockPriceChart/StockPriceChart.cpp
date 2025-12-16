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
#include "Logging.h"

#define LOGGING_CATEGORY ChartLog
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
    m_forwardCandlestickSeries = new QCandlestickSeries();
    m_forwardCandlestickSeries->setPen(QPen(QColor(Qt::black)));
    m_forwardCandlestickSeries->setIncreasingColor(QColor(Qt::green));
    m_forwardCandlestickSeries->setDecreasingColor(QColor(Qt::red));
    m_forwardCandlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH);

    m_backwardCandlestickSeries = new QCandlestickSeries();
    m_backwardCandlestickSeries->setPen(QPen(QColor(Qt::black)));
    m_backwardCandlestickSeries->setIncreasingColor(QColor(Qt::green));
    m_backwardCandlestickSeries->setDecreasingColor(QColor(Qt::red));
    m_backwardCandlestickSeries->setBodyWidth(CANDLESTICK_BODY_WIDTH);

    // Void bar series setup
    voidBarSeries = new QScatterSeries();
    voidBarSeries->setName("Void Bars");
    voidBarSeries->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    voidBarSeries->setMarkerSize(10.0);  // Size of the X marker
    voidBarSeries->setPen(QPen(QColor(Qt::red), 2));  // Red X with thicker lines
    voidBarSeries->setBrush(Qt::NoBrush);  // No fill, just the X outline

    // Adding all series to the chart
    chart = new QChart();
    chart->addSeries(m_forwardCandlestickSeries);
    chart->addSeries(m_backwardCandlestickSeries);
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
    m_forwardCandlestickSeries->attachAxis(axisX);
    m_backwardCandlestickSeries->attachAxis(axisX);
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
    m_forwardCandlestickSeries->attachAxis(axisY);
    m_backwardCandlestickSeries->attachAxis(axisY);
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
    connect(axisX, &QValueAxis::rangeChanged, this, &StockPriceChart::onOneOfTheAxesRangeChanged);
    connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::onOneOfTheAxesRangeChanged);
}


void StockPriceChart::onOneOfTheAxesRangeChanged()
{
    updateAfterHoursBackground();
    redrawLastPriceLine();
    updateAxisLabelsDensity();
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
    m_symbol = symbol;
    m_forwardCandlestickSeries->setName(symbol + " (Bars)");
    m_backwardCandlestickSeries->setName(symbol + " (Bars)");
    chart->setTitle(symbol);

    DEBUG << "Set chart symbol to" << symbol;
}

/**
 * @brief Adds a new bar to the chart.
 * 
 * Processes the bar based on its status using incremental updates.
 * 
 * @param bar The Bar object containing price data and timestamp.
 */
void StockPriceChart::addBar(const QString& symbol, const Bar& bar)
{
    // Its a bug if we receive a new bar for a different symbol than current
    Q_ASSERT(symbol == m_symbol);

    Q_ASSERT(bar.isValid());

    WARNING << "Received bar for" << symbol 
            << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
            << "Status:" << Bar::barStatusToString(bar.getBarStatus())
            << "O:" << bar.getOpen()
            << "H:" << bar.getHigh()
            << "L:" << bar.getLow()
            << "C:" << bar.getClose();

    // Is this the first bar ever received for this chart
    if (m_latestCandlestick == nullptr ) [[unlikely]] {

        // Populate m_latestCandlestick
        addNewCandlestick(bar);
        m_latestBar = bar;

        // Set initial view
        int index = 0; // First bar gets index 0
        axisX->setRange(index - 30, index + 1); // Show last 30 bars plus one extra for room

        // First open bar - add to index mapping and create candlestick
        updateIndexMappingForward(bar.getTimeStamp());
        
        double newPrice = bar.getClose();
               
        // Set Y axis range with padding
        double padding = newPrice * 0.0002;
        double minRange = newPrice * 0.0005;
        axisY->setRange(qMax(0.0, newPrice - minRange/2 - padding), 
                        newPrice + minRange/2 + padding);

        return;
    }


    if (bar.getBarStatus() == Bar::BarStatus::Closed) {
        // We get a closed bar after a open bar
        Q_ASSERT(m_latestBar.getBarStatus() == Bar::BarStatus::Open);

        // The closed bar will be the same of the previous current open bar
        Q_ASSERT(bar.getTimeStamp() == m_latestBar.getTimeStamp());

        // Same timestamp - the open bar became closed, update the existing candlestick
        completedBars.insert(bar.getTimeStamp(), bar);

        Q_CHECK_PTR(m_latestCandlestick);

        m_latestCandlestick->setOpen(bar.getOpen());
        m_latestCandlestick->setHigh(bar.getHigh());
        m_latestCandlestick->setLow(bar.getLow());
        m_latestCandlestick->setClose(bar.getClose());
    
        m_latestBar = bar;
        
        maintainBarLimit();

    } else if (bar.getBarStatus() == Bar::BarStatus::Open) {

        if (m_latestBar.getBarStatus() == Bar::BarStatus::Closed) {
            // New open bar - create a new candlestick
            Q_ASSERT(false);
            // tackle this
            //updateIndexMappingIncremental(bar.getTimeStamp());
            addNewCandlestick(bar);
        } else {
            // Updating the existing open bar
            Q_ASSERT(bar.getTimeStamp() == m_latestBar.getTimeStamp());
        }
    
        m_latestBar = bar;
    
        redrawLastPriceLine();
    } else {
        Q_ASSERT(false); // Unknown bar status, or perhaps have to deal with void bars
    }
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

    Q_ASSERT(!bars.isEmpty());

    // thats just to handle the initial pass in this function
    // FIXME this is wrong
    if (lastValidClosePrice == 0.0) {
        lastValidClosePrice = completedBars.last().getOpen();
    }

    // First, add all historical bars to the index mapping using negative indices.
    // This is O(m) where m = number of new bars, instead of O(n) rebuild.
    // Note: This only creates timestamp → index mappings; bars don't need to be
    // in completedBars/voidBars yet since we only read their timestamps.
    addHistoricalBarsToIndexMapping(bars);
    
    DEBUG << "After addHistoricalBarsToIndexMapping, index range:" 
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
    
    DEBUG << "Prepending to candlestick series with" << indexToTimestamp.size() << "total bars";
    
    for(auto it = bars.rbegin(); it != bars.rend(); ++it) {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();


        if (bar.getBarStatus() == Bar::BarStatus::Null) {
            DEBUG << "  Prepending void candlestick for time" << timestamp.toString("hh:mm:ss");
            //voidBarSeries->prepend(it->getTimeStamp(), voidBars[it->getTimeStamp()]);
        } else {
            DEBUG << "  Prepending bar for time" << timestamp.toString("hh:mm:ss");
            auto set = new QCandlestickSet();
            Q_CHECK_PTR(set);
            DEBUG << "  Prepending candlestick for time" << timestamp.toString("hh:mm:ss"); 

            int index = timestampToIndex[timestamp];
            Q_ASSERT(index < 0); // Historical bars should have negative indices

            set->setTimestamp(index);
            set->setOpen(bar.getOpen());
            set->setHigh(bar.getHigh());
            set->setLow(bar.getLow());
            set->setClose(bar.getClose());

            index*=-1; // Make positive for prepending
            DEBUG << "Prepending candlestick at index" << index << "time" << timestamp.toString("hh:mm:ss") 
                  << "O/H/L/C:" << bar.getOpen() << bar.getHigh() << bar.getLow() << bar.getClose();
            m_backwardCandlestickSeries->insert(index, set);
        }
    } 

    // Maintain the bar limit
    maintainBarLimit();

    // Update the after-hours background
    updateAfterHoursBackground();
}

/**
 * @brief Maintains the maximum number of bars limit.
 * 
 * Removes the oldest bars (both completed and void bars) when the total
 * exceeds MAX_BARS to prevent memory issues and maintain performance.
 * Also cleans up the index mappings for removed bars.
 */
void StockPriceChart::maintainBarLimit() {

    #warning deal with this
    return;
    
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
 * @brief Updates the horizontal last price line across the visible chart area.
 * 
 * Creates a horizontal dashed line at the specified price level spanning the
 * current visible X-axis range. Sets the line color based on price movement
 * (green for uptick, red for downtick) and updates the price label.
 * 
 * @param price The price level for the line.
 * @param isUpTick True if price is moving up (green line), false for down (red line).
 */
void StockPriceChart::redrawLastPriceLine() {

    if (m_latestCandlestick == nullptr) {
        // This is the init state where no bars exist yet when we havent received any data
        // yet. Simply ignore.
        return;
    }

    double lastPrice = m_latestBar.getClose();
    QColor lineColor = (m_latestBar.getClose() >= m_latestBar.getOpen()) ? Qt::green : Qt::red;

    lastPriceLine->clear();

    // Create two points for the horizontal line spanning the visible range
    lastPriceLine->append(axisX->min(), lastPrice);
    lastPriceLine->append(axisX->max(), lastPrice);

    // Update the line color based on price movement
    lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update price label
    priceLabel->setPlainText(QString::number(lastPrice, 'f', 2));
    priceLabel->setDefaultTextColor(lineColor);

    // Update price label position
    {
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

    Q_ASSERT(!completedBars.isEmpty() || !voidBars.isEmpty());

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to the nearest minute
    viewStartTimeRounded = viewStartTimeRounded.addSecs(-viewStartTimeRounded.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());
    
    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);
    
    QDateTime firstBarTime = completedBars.firstKey();
            
    if (viewStartTimeRounded >= firstBarTime) {
        // View is within available bars
        return;
    }

    DEBUG << "Chart view extends beyond available bars:";
    DEBUG << "  Last :" << firstBarTime;
    DEBUG << "  First:" << viewStartTimeRounded;
    
    // If there is already a getBars request to the bar cache, suck it up and wait to receive the data
    if (currentGetBarsRequestInProcess) {
        DEBUG << "current get bars request already in progress";
        return;
    }
    
    currentGetBarsRequestInProcess = true;


    DEBUG << "Requesting missing bars from"
          << viewStartTimeRounded.toString(Qt::ISODate)
          << "to"
          << firstBarTime.toString(Qt::ISODate);

    emit requestMissingBars(viewStartTimeRounded, firstBarTime);
}

/**
 * @brief Clears all data and resets the chart for a new symbol.
 * 
 * Removes all bars, series data, background rectangles, and resets axes
 * to default ranges. Prepares the chart for displaying a new stock symbol.
 */
void StockPriceChart::clearSymbol() {
    // Clear the candlestick series
    m_forwardCandlestickSeries->clear();
    m_backwardCandlestickSeries->clear();

    // Clear the completed bars
    completedBars.clear();
    
    // Clear void bars
    voidBars.clear();
    
    // Reset the current open bar
    m_latestCandlestick = nullptr;
    m_latestBar = Bar();

    // Clear the last price line
    lastPriceLine->clear();
    
    // Clear background rectangles
    clearBackgroundRects();
    
    // Reset the price label
    priceLabel->setPlainText("");
    
    // Reset the chart title
    chart->setTitle("");
    
    // Clear index mappings
    indexToTimestamp.clear();
    timestampToIndex.clear();
    
    // Reset the axes ranges to default
    axisX->setRange(0, 30);
    axisY->setRange(0, 100);
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
QDateTime StockPriceChart::getTimestampForIndex(int index) const
{
    Q_ASSERT(!indexToTimestamp.isEmpty());

    auto it = indexToTimestamp.find(index);
    if (it != indexToTimestamp.end()) {
        return it.value();
    }
    
    // If index not found, interpolate or extrapolate

    
    if (index < 0) {
        // Extrapolate backwards - skip over non-trading hours
        int firstIndex = indexToTimestamp.firstKey();

        Q_ASSERT(index < firstIndex);

        QDateTime currentTime = indexToTimestamp.first();
        int deltaIndex = firstIndex - index;
        
        // Walk backwards minute by minute, skipping non-trading hours
        for (int i = 0; i < deltaIndex; ++i) {
            currentTime = getPreviousTradingMinute(currentTime);
        }
        
        return currentTime;
    }
    
    int lastIndex = indexToTimestamp.lastKey();

    Q_ASSERT(index > lastIndex);

    // Extrapolate forwards - assume 1-minute bars
    QDateTime lastTime = indexToTimestamp.last();
    int deltaIndex = index - lastIndex;
    return lastTime.addSecs(deltaIndex * 60);
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
            DEBUG << "Unexpected: getPreviousTradingMinute called with time after 8PM:" << nyTime;
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
 * @brief Updates axis tick intervals dynamically based on screen density.
 * 
 * This function implements a density-based tick interval adjustment system that
 * automatically adjusts the spacing of tick marks on both X and Y axes based on
 * the current zoom level and chart dimensions. This ensures axis labels remain
 * readable and appropriately spaced regardless of zoom level.
 * 
 * **X-Axis (Index-based):**
 * - Calculates pixels per index unit
 * - Selects from intervals: 1, 2, 5, 10, 20, 50, 100, etc.
 * - Maintains minimum spacing of MIN_PIXELS_PER_TICK_X pixels between ticks
 * 
 * **Y-Axis (Price-based):**
 * - Calculates pixels per dollar
 * - Selects from intervals: $0.05, $0.10, $0.25, $1.00, $5.00, $25.00, $100.00
 * - Maintains minimum spacing of MIN_PIXELS_PER_TICK_Y pixels between ticks
 * 
 * The function is called automatically whenever the view is zoomed or panned
 * to ensure tick density remains optimal for the current view.
 * 
 * @see MIN_PIXELS_PER_TICK_X, MIN_PIXELS_PER_TICK_Y for configurable thresholds
 */
void StockPriceChart::updateAxisLabelsDensity() {
    
    // Get visible range in indices
    qreal minIndex = axisX->min();
    qreal maxIndex = axisX->max();
    qreal range = maxIndex - minIndex;
    
    Q_ASSERT(range > 0);

    if (m_latestCandlestick == nullptr) {
        // No bars yet, cannot adjust axis
        INFO << "No candlesticks yet, skipping axis label update.";
        return;
    }
    
    // ========== X-AXIS TICK INTERVAL ADJUSTMENT ==========
    // Calculate density-based tick interval for X-axis
    int chartWidth = chartView->width();
    
    Q_ASSERT(chartWidth > 0);

  
    // Calculate current pixels per unit on the X-axis
    qreal pixelsPerUnit = static_cast<qreal>(chartWidth) / range;
        
    // Determine the appropriate tick interval based on density
    // We want at least MIN_PIXELS_PER_TICK_X pixels between ticks
    qreal minTickInterval = MIN_PIXELS_PER_TICK_X / pixelsPerUnit;
        
    // Track previous interval for level jump detection
    static qreal previousXInterval = 1.0;
        
    //DEBUG << "========== X-AXIS INTERVAL SELECTION ==========";
    //DEBUG << "Chart width:" << chartWidth << "px, Range:" << range 
    //                      << "Pixels per unit:" << pixelsPerUnit;
    //DEBUG << "MIN_PIXELS_PER_TICK_X threshold:" << MIN_PIXELS_PER_TICK_X << "px";
    //DEBUG << "Minimum tick interval needed:" << minTickInterval;
        
    // Round up to a sensible tick interval (powers/multiples of common numbers)
    qreal tickInterval = 1.0;
    if (minTickInterval <= 1.0) {
        tickInterval = 1.0;
        //DEBUG << "  Selected interval: 1 (minTickInterval <= 1.0)";
    } else if (minTickInterval <= 2.0) {
        tickInterval = 2.0;
        //DEBUG << "  Selected interval: 2 (minTickInterval <= 2.0)";
    } else if (minTickInterval <= 5.0) {
        tickInterval = 5.0;
        //DEBUG << "  Selected interval: 5 (minTickInterval <= 5.0)";
    } else if (minTickInterval <= 10.0) {
        tickInterval = 10.0;
        //DEBUG << "  Selected interval: 10 (minTickInterval <= 10.0)";
    } else if (minTickInterval <= 20.0) {
        tickInterval = 20.0;
       //DEBUG << "  Selected interval: 20 (minTickInterval <= 20.0)";
    } else if (minTickInterval <= 50.0) {
        tickInterval = 50.0;
        //DEBUG << "  Selected interval: 50 (minTickInterval <= 50.0)";
    } else if (minTickInterval <= 100.0) {
        tickInterval = 100.0;
        //DEBUG << "  Selected interval: 100 (minTickInterval <= 100.0)";
    } else {
        // For very large zoom-outs, use larger intervals
        tickInterval = qCeil(minTickInterval / 100.0) * 100.0;
        //DEBUG << "  Selected interval:" << tickInterval << "(very large zoom-out)";
    }
        
    // Detect and log level jumps
    if (tickInterval != previousXInterval) {
        //INFO << "***** X-AXIS LEVEL JUMP:" << previousXInterval 
        //                 << "→" << tickInterval << "*****";
        previousXInterval = tickInterval;
    }
        
    axisX->setTickInterval(tickInterval);
        
    // Set tick count based on the range and interval
    int estimatedTicks = qMax(2, static_cast<int>(range / tickInterval) + 1);
    int finalTickCount = qMin(estimatedTicks, 20); // Cap at 20 ticks maximum
    axisX->setTickCount(finalTickCount);
        
    // Force the chart to update with new tick settings
    chart->update();
        
    //DEBUG << "→ SELECTED:" << tickInterval 
    //                  << "| Actual pixels per tick:" << (pixelsPerUnit * tickInterval)
    //                  << "| Est. ticks:" << estimatedTicks 
    //                  << "| Final tick count:" << finalTickCount;
    //DEBUG << "================================================";

    // ========== Y-AXIS TICK INTERVAL ADJUSTMENT ==========
    // Calculate density-based tick interval for Y-axis
    qreal minPrice = axisY->min();
    qreal maxPrice = axisY->max();
    qreal priceRange = maxPrice - minPrice;
    
    int chartHeight = chartView->height();
    
    //INFO << "About to check Y-axis conditions: priceRange=" << priceRange 
    //                  << "chartHeight=" << chartHeight;
    
    Q_ASSERT(priceRange > 0);
    Q_ASSERT(chartHeight > 0);

    // Calculate current pixels per dollar on the Y-axis
    qreal pixelsPerDollar = static_cast<qreal>(chartHeight) / priceRange;
        
    // Available tick intervals in dollars: 0.05, 0.10, 0.25, 1.00, 5.00, 25.00, 100.00
    static const QVector<qreal> availableIntervals = {0.05, 0.10, 0.25, 1.00, 5.00, 25.00, 100.00};
        
    // Track previous interval for level jump detection
    static qreal previousInterval = 0.05;
        
    // Select the smallest interval that maintains desired pixel density
    qreal selectedInterval = availableIntervals.last(); // Default to largest
        
    //DEBUG << "========== Y-AXIS INTERVAL SELECTION ==========";
    //DEBUG << "Chart height:" << chartHeight << "px, Price range:" << priceRange 
    //                  << "Pixels per dollar:" << pixelsPerDollar;
    //DEBUG << "MIN_PIXELS_PER_TICK_Y threshold:" << MIN_PIXELS_PER_TICK_Y << "px";
    //DEBUG << "Current tick interval on axis:" << axisY->tickInterval();
        
    for (qreal interval : availableIntervals) {
        qreal pixelsPerTick = pixelsPerDollar * interval;
        bool meetsThreshold = pixelsPerTick >= MIN_PIXELS_PER_TICK_Y;
          
        //DEBUG << "  Testing interval $" << interval 
        //                  << "→ pixelsPerTick:" << pixelsPerTick << "px"
        //                  << (meetsThreshold ? "✓ ACCEPTABLE" : "✗ too dense");
            
        // If this interval provides enough spacing, use it
        if (meetsThreshold) {
            selectedInterval = interval;
            break;
        }
    }
        
    // Detect and log level jumps
    if (selectedInterval != previousInterval) {
        //INFO << "***** Y-AXIS LEVEL JUMP: $" << previousInterval 
        //                         << "→ $" << selectedInterval << "*****";
        previousInterval = selectedInterval;
    }
        
    axisY->setTickInterval(selectedInterval);
       
    // Set tick count based on the range and interval
    int estimatedTicksY = qMax(2, static_cast<int>(priceRange / selectedInterval) + 1);
    int finalTickCountY = qMin(estimatedTicksY, 20); // Cap at 20 ticks maximum
    axisY->setTickCount(finalTickCountY);
        
    // Force the chart to update with new tick settings
    chart->update();
        
    //DEBUG << "→ SELECTED: $" << selectedInterval 
    //      << "| Est. ticks:" << estimatedTicksY 
    //      << "| Final tick count:" << finalTickCountY;
    //DEBUG << "Axis tick interval after setting:" << axisY->tickInterval();
    //DEBUG << "================================================";
    
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
    
    DEBUG << "addHistoricalBarsToIndexMapping: adding" << bars.size() << "bars, starting minIndex:" << minIndex;
    
    // Process bars in reverse chronological order (newest to oldest)
    // so we can assign negative indices going backwards.
    // We decrement the index only when we actually add a bar to avoid gaps.
    for (auto it = bars.rbegin(); it != bars.rend(); ++it) {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();
        
        // Skip if already in mapping (shouldn't happen in normal flow,
        // but protects against duplicate insertions)
        if (timestampToIndex.contains(timestamp)) {
            DEBUG << "  Skipping duplicate timestamp" << timestamp.toString("hh:mm:ss");
            continue;
        }
        
        // Assign the next negative index for all bars (including void bars)
        // Decrement BEFORE assignment so the first bar gets minIndex-1
        --minIndex;
        indexToTimestamp[minIndex] = timestamp;
        timestampToIndex[timestamp] = minIndex;
        
        if (it - bars.rbegin() >= bars.size() - 3 || minIndex >= -3) {
            DEBUG << "  Assigned index" << minIndex << "to timestamp" << timestamp.toString("hh:mm:ss");
        }
    }
    
    DEBUG << "addHistoricalBarsToIndexMapping: completed, new minIndex:" << minIndex;
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
void StockPriceChart::addNewCandlestick(const Bar& bar) {
    int index = getIndexForTimestamp(bar.getTimeStamp());
    
    m_latestCandlestick = new QCandlestickSet();
    Q_CHECK_PTR(m_latestCandlestick);

    m_latestCandlestick->setTimestamp(index);
    m_latestCandlestick->setOpen(bar.getOpen());
    m_latestCandlestick->setHigh(bar.getHigh());
    m_latestCandlestick->setLow(bar.getLow());
    m_latestCandlestick->setClose(bar.getClose());
    m_forwardCandlestickSeries->append(m_latestCandlestick);
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
void StockPriceChart::updateIndexMappingForward(const QDateTime& timestamp)
{
    if (indexToTimestamp.isEmpty()) {
        // First entry
        indexToTimestamp[0] = timestamp;
        timestampToIndex[timestamp] = 0;
        return;
    }

    // Its a logic error if we call this function while the timestamp already exists
    Q_ASSERT(!timestampToIndex.contains(timestamp));
    
    // Logic error if we are trying to add a timestamp earlier than the last one
    Q_ASSERT(timestamp > indexToTimestamp.last());
    
    int newIndex = indexToTimestamp.lastKey() + 1;
    
    indexToTimestamp[newIndex] = timestamp;
    timestampToIndex[timestamp] = newIndex;
}