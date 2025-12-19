#include "StockPriceChart.h"
#include "MarketHours.h"
#include "Logging.h"

#define LOGGING_CATEGORY ChartLog
#define CANDLESTICK_BODY_WIDTH 0.9 // 90% of available space

Q_LOGGING_CATEGORY(ChartLog, "Chart");

/**
 * @brief Constructs a StockPriceChart widget using qcustomplot.
 */
StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent) {

    // Create the custom plot widget
    m_customPlot = new QCustomPlot(this);
    Q_CHECK_PTR(m_customPlot);
    
    // Create candlestick chart
    m_candlesticks = new QCPFinancial(m_customPlot->xAxis, m_customPlot->yAxis);
    Q_CHECK_PTR(m_candlesticks);
    
    m_candlesticks->setName("Candlestick");
    m_candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    m_candlesticks->setWidth(CANDLESTICK_BODY_WIDTH);
    m_candlesticks->setTwoColored(true);
    m_candlesticks->setBrushPositive(QColor(0, 180, 0));      // Green for up
    m_candlesticks->setBrushNegative(QColor(200, 0, 0));      // Red for down
    m_candlesticks->setPenPositive(QPen(QColor(0, 0, 0)));
    m_candlesticks->setPenNegative(QPen(QColor(0, 0, 0)));

    // Setup axes
    m_customPlot->xAxis->setLabel("Time");
    m_customPlot->yAxis->setLabel("Price");
    
    // Apply dark theme
    m_customPlot->setBackground(QBrush(QColor(45, 45, 50)));
    m_customPlot->xAxis->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->yAxis->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->setLabelColor(QColor(220, 220, 220));
    m_customPlot->yAxis->setLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_customPlot->yAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    
    // Create bottom axis rect for volume bar chart
    m_volumeAxisRect = new QCPAxisRect(m_customPlot);
    Q_CHECK_PTR(m_volumeAxisRect);
    m_customPlot->plotLayout()->addElement(1, 0, m_volumeAxisRect);
    m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    
    // Bring bottom and main axis rect closer together
    m_customPlot->plotLayout()->setRowSpacing(0);
    m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
    m_volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
    
    // Apply dark theme to volume axis rect
    m_volumeAxisRect->setBackground(QBrush(QColor(45, 45, 50)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atLeft)->setBasePen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atLeft)->setTickPen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220));
    m_volumeAxisRect->axis(QCPAxis::atLeft)->setTickLabelColor(QColor(220, 220, 220));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_volumeAxisRect->axis(QCPAxis::atLeft)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    
    // Create two bar plottables for positive (green) and negative (red) volume bars
    m_customPlot->setAutoAddPlottableToLegend(false);
    m_volumePos = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
    Q_CHECK_PTR(m_volumePos);
    m_volumeNeg = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
    Q_CHECK_PTR(m_volumeNeg);
    
    m_volumePos->setWidth(CANDLESTICK_BODY_WIDTH);
    m_volumePos->setPen(Qt::NoPen);
    m_volumePos->setBrush(QColor(100, 180, 110));
    m_volumeNeg->setWidth(CANDLESTICK_BODY_WIDTH);
    m_volumeNeg->setPen(Qt::NoPen);
    m_volumeNeg->setBrush(QColor(180, 90, 90));
    
    // Interconnect x axis ranges of main and bottom axis rects
    connect(m_customPlot->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_volumeAxisRect->axis(QCPAxis::atBottom), QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_volumeAxisRect->axis(QCPAxis::atBottom), QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    
    // Configure axes of both main and bottom axis rect
    QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
    dateTimeTicker->setDateTimeSpec(Qt::UTC);
    dateTimeTicker->setDateTimeFormat("hh:mm");
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_customPlot->xAxis->setBasePen(Qt::NoPen);
    m_customPlot->xAxis->setTickLabels(false);
    m_customPlot->xAxis->setTicks(false); // Only want vertical grid in main axis rect
    
    // Make axis rects' left side line up
    QCPMarginGroup *group = new QCPMarginGroup(m_customPlot);
    m_customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_volumeAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    
    // Create last price line
    m_lastPriceLine = new QCPItemLine(m_customPlot);
    Q_CHECK_PTR(m_lastPriceLine);
    m_lastPriceLine->setPen(QPen(Qt::green, 1, Qt::DashLine));
    
    // Create price label
    m_priceLabel = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_priceLabel);
    m_priceLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_priceLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_priceLabel->setFont(QFont(font().family(), 10, QFont::Bold));
    m_priceLabel->setColor(Qt::green);
    m_priceLabel->setPadding(QMargins(5, 2, 5, 2));
    m_priceLabel->setBrush(QBrush(QColor(0, 0, 0, 150)));
    m_priceLabel->setVisible(false);

    // Enable mouse interactions
    m_customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_customPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_customPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    
    // Set initial ranges
    m_customPlot->xAxis->setRange(-3, 3);
    m_customPlot->yAxis->setRange(0, 100);

    // Create layout and add widgets
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create and add the timeframe selector at the top
    timeframeSelector = new TimeFrameSelector(this);
    Q_CHECK_PTR(timeframeSelector);
    layout->addWidget(timeframeSelector, 0);  // 0 stretch - keep minimal size
    
    layout->addWidget(m_customPlot, 1);  // 1 stretch - expand to fill space
    setLayout(layout);

    // Connect axis range change signals
    connect(m_customPlot->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this, &StockPriceChart::onAxisRangeChanged);
    connect(m_customPlot->yAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this, &StockPriceChart::onAxisRangeChanged);

    setSymbol("");
}

StockPriceChart::~StockPriceChart() {
    // Qt parent-child hierarchy handles cleanup
}

/**
 * @brief Sets the stock symbol for the chart.
 */
void StockPriceChart::setSymbol(const QString& symbol) {
    m_symbol = symbol;
    m_candlesticks->setName(symbol + " (Bars)");
    m_customPlot->plotLayout()->insertRow(0);
    m_customPlot->plotLayout()->addElement(0, 0, new QCPTextElement(m_customPlot, symbol, QFont("sans", 12, QFont::Bold)));
    
    DEBUG << "Set chart symbol to" << symbol;
}

/**
 * @brief Adds a new bar to the chart.
 */
void StockPriceChart::addLiveBar(const QString& symbol, const Bar& bar)
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
    if (indexToBar.size() == 0) [[unlikely]] {
        const int index = 0;

        Q_ASSERT(timestampToIndex.size() == 0);

        timestampToIndex[bar.getTimeStamp()] = index;
        indexToBar[index] = bar;
        m_latestBar = bar;
        m_latestBarIndex = index;

        // Update candlestick data
        updateCandlestickData();
        updateVolumeData();

        m_customPlot->xAxis->setRange(index - 30, index + 1);

        double newPrice = bar.getClose();
        double padding = newPrice * 0.0002;
        double minRange = newPrice * 0.0005;
        m_customPlot->yAxis->setRange(qMax(0.0, newPrice - minRange/2 - padding), 
                        newPrice + minRange/2 + padding);

        m_customPlot->replot();
        return;
    }

    Q_ASSERT(bar.getBarStatus() != Bar::BarStatus::Null);

    if (bar.getBarStatus() == Bar::BarStatus::Closed 
       || (bar.getBarStatus() == Bar::BarStatus::Open && m_latestBar.getBarStatus() == Bar::BarStatus::Closed)) {
        // New bar starting
        Q_ASSERT(bar.getTimeStamp() > m_latestBar.getTimeStamp());

        const int newIndex = indexToBar.lastKey() + 1;

        timestampToIndex[bar.getTimeStamp()] = newIndex;
        indexToBar[newIndex] = bar;
        m_latestBar = bar;
        m_latestBarIndex = newIndex;
        
        maintainBarLimit();
    } else {
        // Update existing bar
        Q_ASSERT(bar.getTimeStamp() == m_latestBar.getTimeStamp());
        indexToBar[m_latestBarIndex] = bar;
        m_latestBar = bar;
    }

    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    redrawLastPriceLine();
    m_customPlot->replot();
}

/**
 * @brief Updates the candlestick data from indexToBar map.
 */
void StockPriceChart::updateCandlestickData()
{
    // Convert our bar data to QCPFinancialData format
    QVector<QCPFinancialData> financialData;
    
    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it) {
        const int index = it.key();
        const Bar& bar = it.value();
        
        if (bar.getBarStatus() != Bar::BarStatus::Null) {
            QCPFinancialData data;
            data.key = index;  // Use index as the x-axis value
            data.open = bar.getOpen();
            data.high = bar.getHigh();
            data.low = bar.getLow();
            data.close = bar.getClose();
            financialData.append(data);
        }
    }
    
    m_candlesticks->data()->set(financialData);
}

/**
 * @brief Updates the volume bar data from indexToBar map.
 */
void StockPriceChart::updateVolumeData()
{
    // Clear existing volume data
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    
    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it) {
        const int index = it.key();
        const Bar& bar = it.value();
        
        if (bar.getBarStatus() != Bar::BarStatus::Null) {
            // Determine if bar is up or down based on close vs open
            bool isUp = bar.getClose() >= bar.getOpen();
            qint64 volume = bar.getTotalVolume();
            
            // Add to appropriate bar series
            if (isUp) {
                m_volumePos->addData(index, volume);
            } else {
                m_volumeNeg->addData(index, volume);
            }
        }
    }
    
    // Rescale volume axis to fit data
    m_volumeAxisRect->axis(QCPAxis::atLeft)->rescale();
}

/**
 * @brief Updates the background colors for different market sessions.
 * 
 * Draws colored background rectangles to indicate:
 * - Pre-market hours (brownish/orange)
 * - After-hours (blueish/violet)
 * - Closed market periods including weekends (dark gray)
 * 
 * Only draws backgrounds for the currently visible time range.
 */
void StockPriceChart::updateSessionBackgrounds() {
    // Clear existing rectangles
    clearBackgroundRects();
    
    if(indexToBar.isEmpty()) {
        DEBUG << "No bars available for background drawing";
        return;
    }
      
    // Get the timestamps for visible range
    int minIndex = static_cast<int>(m_customPlot->xAxis->range().lower);
    int maxIndex = static_cast<int>(m_customPlot->xAxis->range().upper);
    
    DEBUG << "Updating session backgrounds for index range:" << minIndex << "to" << maxIndex;
    
    // Find the bar at min and max indices (or closest)
    auto minIt = indexToBar.lowerBound(minIndex);
    auto maxIt = indexToBar.lowerBound(maxIndex);
    
    if (minIt == indexToBar.end() || maxIt == indexToBar.end()) {
        DEBUG << "Could not find bars at min/max indices";
        return;
    }
    
    QDateTime startTime = minIt.value().getTimeStamp();
    QDateTime endTime = maxIt.value().getTimeStamp();
    
    DEBUG << "Time range:" << startTime.toString() << "to" << endTime.toString();
    
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
                                      QColor(40, 40, 50, 120), m_closedMarketRects);
            currentDate = currentDate.addDays(1);
            continue;
        }

        // Consolidate consecutive hours of the same session type into single rectangles
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
            
            // Draw one rectangle for the entire session
            if (isPreMarket) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(90, 60, 30, 100), m_preMarketRects);
            }
            else if (isAfterHours) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(50, 50, 80, 100), m_afterHoursRects);
            }
            else if (!isRegularHours) {
                drawBackgroundForTimeRange(localSessionStart, localSessionEnd,
                                          QColor(40, 40, 50, 120), m_closedMarketRects);
            }
            
            // Move to the next session
            hour = sessionEndHour;
        }
        
        currentDate = currentDate.addDays(1);
    }
    
    DEBUG << "Created" << m_preMarketRects.size() << "pre-market rects,"
          << m_afterHoursRects.size() << "after-hours rects,"
          << m_closedMarketRects.size() << "closed market rects";
}

/**
 * @brief Clears all background rectangles from the chart.
 */
void StockPriceChart::clearBackgroundRects() {
    // Delete and clear pre-market rectangles
    for (auto rect : m_preMarketRects) {
        m_customPlot->removeItem(rect);
    }
    m_preMarketRects.clear();

    // Delete and clear after-hours rectangles
    for (auto rect : m_afterHoursRects) {
        m_customPlot->removeItem(rect);
    }
    m_afterHoursRects.clear();

    // Delete and clear closed market rectangles
    for (auto rect : m_closedMarketRects) {
        m_customPlot->removeItem(rect);
    }
    m_closedMarketRects.clear();
}

/**
 * @brief Draws a background rectangle for a specific time range.
 * 
 * Creates a colored background rectangle covering the bars that fall within
 * the specified time range, clipped to the currently visible area.
 * 
 * @param rangeStart The start time of the range to highlight.
 * @param rangeEnd The end time of the range to highlight.
 * @param color The color for the background rectangle.
 * @param rectList The list to add the created rectangle to.
 */
void StockPriceChart::drawBackgroundForTimeRange(const QDateTime& rangeStart, const QDateTime& rangeEnd,
                                                   const QColor& color, QList<QCPItemRect*>& rectList) {
    if (indexToBar.isEmpty()) {
        return;
    }
    
    // Use efficient binary search to find indices
    auto startIt = timestampToIndex.lowerBound(rangeStart);
    auto endIt = timestampToIndex.upperBound(rangeEnd);
    
    // Check if we found any bars in this range
    if (startIt == timestampToIndex.end() || startIt.key() > rangeEnd) {
        DEBUG << "No bars found in range" << rangeStart.toString() << "to" << rangeEnd.toString();
        return; // No bars in this range
    }
    
    int startIndex = startIt.value();
    int endIndex = -1;
    
    // Get the last valid index in range
    if (endIt != timestampToIndex.begin()) {
        --endIt;
        endIndex = endIt.value();
    } else {
        DEBUG << "No valid end index found";
        return; // No bars in range
    }
    
    // Verify we have a valid range
    if (startIndex == -1 || endIndex == -1 || endIndex < startIndex) {
        DEBUG << "Invalid index range:" << startIndex << "to" << endIndex;
        return;
    }
    
    qreal visibleMinIndex = m_customPlot->xAxis->range().lower;
    qreal visibleMaxIndex = m_customPlot->xAxis->range().upper;
    
    // Clip to visible range
    qreal clippedStart = qMax(static_cast<qreal>(startIndex), visibleMinIndex);
    qreal clippedEnd = qMin(static_cast<qreal>(endIndex + 1), visibleMaxIndex);
    
    if (clippedStart < clippedEnd) {
        QCPItemRect* rect = new QCPItemRect(m_customPlot);
        Q_CHECK_PTR(rect);
        
        // Set the rectangle coordinates
        rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
        rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
        rect->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->yAxis);
        rect->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->yAxis);
        rect->topLeft->setCoords(clippedStart, m_customPlot->yAxis->range().upper);
        rect->bottomRight->setCoords(clippedEnd, m_customPlot->yAxis->range().lower);
        
        rect->setPen(Qt::NoPen);
        rect->setBrush(QBrush(color));
        
        // Set layer to ensure rectangles are behind the data
        rect->setLayer("background");
        
        DEBUG << "Created background rect from index" << clippedStart << "to" << clippedEnd
              << "with color" << color.name();
        
        rectList.append(rect);
    } else {
        DEBUG << "Clipped range invalid:" << clippedStart << ">=" << clippedEnd;
    }
}

/**
 * @brief Handles the response to a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsReceived(const QVector<Bar>& bars) {
    Q_ASSERT(currentGetBarsRequestInProcess == true);
    currentGetBarsRequestInProcess = false;
    Q_ASSERT(!bars.isEmpty());

    addHistoricalBarsToIndexMapping(bars);
    
    DEBUG << "After addHistoricalBarsToIndexMapping, index range:" 
          << QString("%1 to %2").arg(indexToBar.firstKey()).arg(indexToBar.lastKey());

    maintainBarLimit();
    
    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();
    
    m_customPlot->replot();
}

/**
 * @brief Maintains the maximum number of bars limit.
 */
void StockPriceChart::maintainBarLimit()
{
    int totalBars = indexToBar.size();
    
    if (totalBars > MAX_BARS) {
        CRITICAL << "TODO: deal with this scenario";
    }
}

/**
 * @brief Updates the horizontal last price line.
 */
void StockPriceChart::redrawLastPriceLine() {
    if (m_latestBarIndex == -1) {
        m_priceLabel->setVisible(false);
        return;
    }

    double lastPrice = m_latestBar.getClose();
    QColor lineColor = (m_latestBar.getClose() >= m_latestBar.getOpen()) ? Qt::green : Qt::red;

    // Update line
    m_lastPriceLine->start->setCoords(m_customPlot->xAxis->range().lower, lastPrice);
    m_lastPriceLine->end->setCoords(m_customPlot->xAxis->range().upper, lastPrice);
    m_lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update label
    m_priceLabel->setText(QString::number(lastPrice, 'f', 2));
    m_priceLabel->setColor(lineColor);
    m_priceLabel->position->setCoords(1.0, lastPrice);
    m_priceLabel->setVisible(true);
}

/**
 * @brief Checks if the current view requires missing bars to be loaded.
 */
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime) {
    Q_UNUSED(viewEndTime);

    Q_ASSERT(!indexToBar.isEmpty());

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to the nearest minute
    viewStartTimeRounded = viewStartTimeRounded.addSecs(-viewStartTimeRounded.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());
    
    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);
    
    QDateTime firstBarTime = timestampToIndex.firstKey();
            
    if (viewStartTimeRounded >= firstBarTime) {
        // View is within available bars
        return;
    }

    DEBUG << "Chart view extends beyond available bars:";
    DEBUG << "  Last :" << firstBarTime;
    DEBUG << "  First:" << viewStartTimeRounded;
    
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
 */
void StockPriceChart::clearSymbol() {
    m_candlesticks->data()->clear();
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    clearBackgroundRects();

    indexToBar.clear();
    timestampToIndex.clear();
    
    m_latestBarIndex = -1;
    m_latestBar = Bar();

    m_priceLabel->setVisible(false);
    
    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->yAxis->setRange(0, 100);
    
    m_customPlot->replot();
}

/**
 * @brief Gets the previous valid trading minute.
 */
QDateTime StockPriceChart::getPreviousTradingMinute(const QDateTime& timestamp) const {
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = timestamp.toTimeZone(nyZone);
    QDateTime previousMinute = nyTime.addSecs(-60);
    
    QTime time = previousMinute.time();
    int dayOfWeek = previousMinute.date().dayOfWeek();
    
    if (dayOfWeek >= MONDAY && dayOfWeek <= FRIDAY) {
        if (time < QTime(TRADING_START_HOUR, 0, 0)) {
            QDateTime result = QDateTime(previousMinute.date().addDays(-1), 
                                        QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                                        nyZone);
            if (result.date().dayOfWeek() > FRIDAY) {
                QDate friday = getPreviousFriday(result.date());
                result = QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), nyZone);
            }
            return result.toTimeZone(timestamp.timeZone());
        } else if (time >= QTime(TRADING_END_HOUR, 0, 0)) {
            DEBUG << "Unexpected: getPreviousTradingMinute called with time after 8PM:" << nyTime;
            return QDateTime(previousMinute.date(), 
                           QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                           nyZone).toTimeZone(timestamp.timeZone());
        }
    } else {
        QDate friday = getPreviousFriday(previousMinute.date());
        return QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    return previousMinute.toTimeZone(timestamp.timeZone());
}

/**
 * @brief Adjusts a timestamp to the nearest valid trading time.
 */
QDateTime StockPriceChart::adjustToValidTradingTime(const QDateTime& timestamp) const {
    QTimeZone nyZone("America/New_York");
    QDateTime nyTime = timestamp.toTimeZone(nyZone);
    QTime time = nyTime.time();
    int dayOfWeek = nyTime.date().dayOfWeek();
    
    if (dayOfWeek > FRIDAY) {
        QDate friday = getPreviousFriday(nyTime.date());
        return QDateTime(friday, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    if (time < QTime(TRADING_START_HOUR, 0, 0)) {
        QDate previousDay = nyTime.date().addDays(-1);
        if (previousDay.dayOfWeek() > FRIDAY) {
            previousDay = getPreviousFriday(previousDay);
        }
        return QDateTime(previousDay, QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    } else if (time >= QTime(TRADING_END_HOUR, 0, 0)) {
        return QDateTime(nyTime.date(), QTime(TRADING_END_HOUR - 1, LAST_TRADING_MINUTE, 0), 
                        nyZone).toTimeZone(timestamp.timeZone());
    }
    
    return timestamp;
}

/**
 * @brief Gets the previous Friday given a date.
 */
QDate StockPriceChart::getPreviousFriday(const QDate& date) const {
    int dayOfWeek = date.dayOfWeek();
    if (dayOfWeek == FRIDAY) {
        return date;
    } else if (dayOfWeek > FRIDAY) {
        return date.addDays(-(dayOfWeek - FRIDAY));
    } else {
        return date.addDays(-(dayOfWeek + 2));
    }
}

/**
 * @brief Updates axis tick intervals dynamically based on screen density.
 */
void StockPriceChart::updateAxisLabelsDensity() {
    // Simple implementation - qcustomplot handles most of this automatically
    // Can be enhanced later if needed
    m_customPlot->xAxis->setNumberFormat("g");
    m_customPlot->yAxis->setNumberFormat("f");
    m_customPlot->yAxis->setNumberPrecision(2);
}

/**
 * @brief Adds historical bars to the index mapping using negative indices.
 */
void StockPriceChart::addHistoricalBarsToIndexMapping(const QVector<Bar>& bars) {
    Q_ASSERT(!bars.isEmpty());
    Q_ASSERT(!indexToBar.isEmpty());

    int minIndex = indexToBar.firstKey();
    
    DEBUG << "addHistoricalBarsToIndexMapping: adding" << bars.size() << "bars, starting minIndex:" << minIndex;
    
    for (auto it = bars.rbegin(); it != bars.rend(); ++it) {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();
        
        if (timestampToIndex.contains(timestamp)) {
            DEBUG << "  Skipping duplicate timestamp" << timestamp.toString("hh:mm:ss");
            continue;
        }
        
        --minIndex;
        indexToBar[minIndex] = bar;
        timestampToIndex[timestamp] = minIndex;
        
        if (it - bars.rbegin() >= bars.size() - 3 || minIndex >= -3) {
            DEBUG << "  Assigned index" << minIndex << "to timestamp" << timestamp.toString("hh:mm:ss");
        }
    }
    
    DEBUG << "addHistoricalBarsToIndexMapping: completed, new minIndex:" << minIndex;
}

/**
 * @brief Gets the timestamp corresponding to an index.
 */
QDateTime StockPriceChart::getTimestampForIndex(int index) const {
    auto it = indexToBar.find(index);
    if (it != indexToBar.end()) {
        return it.value().getTimeStamp();
    }
    
    Q_ASSERT(!indexToBar.isEmpty());
    
    if (index < 0) {
        int firstIndex = indexToBar.firstKey();
        QDateTime currentTime = indexToBar.first().getTimeStamp();
        int deltaIndex = firstIndex - index;
        
        for (int i = 0; i < deltaIndex; ++i) {
            currentTime = getPreviousTradingMinute(currentTime);
        }
        
        return currentTime;
    }
    
    int lastIndex = indexToBar.lastKey();
    if (index > lastIndex) {
        QDateTime lastTime = indexToBar.last().getTimeStamp();
        int deltaIndex = index - lastIndex;
        return lastTime.addSecs(deltaIndex * 60);
    }
    
    return QDateTime::currentDateTime();
}

/**
 * @brief Slot called when axis ranges change.
 */
void StockPriceChart::onAxisRangeChanged()
{
    updateAxisLabelsDensity();
    redrawLastPriceLine();
    updateSessionBackgrounds();  // Update background colors when view changes
    
    // Check for missing bars when view extends beyond available data
    if (!indexToBar.isEmpty()) {
        double minIndex = m_customPlot->xAxis->range().lower;
        if (minIndex < indexToBar.firstKey()) {
            QDateTime requestTime = getTimestampForIndex(static_cast<int>(minIndex));
            checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
        }
    }
}

/**
 * @brief Handles widget resize events.
 */
void StockPriceChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    onAxisRangeChanged();
}

/**
 * @brief Handles mouse wheel events for zooming.
 */
void StockPriceChart::wheelEvent(QWheelEvent* event) 
{    
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

    event->accept();
}

void StockPriceChart::handleVerticalPanning(QWheelEvent* event) {
    DEBUG << "Vertical panning with wheel";
    
    QCPRange range = m_customPlot->yAxis->range();
    qreal priceRange = range.size();

    qreal shiftAmount = priceRange * 0.03;
    if (event->angleDelta().y() < 0) {
        shiftAmount = -shiftAmount;
    }

    m_customPlot->yAxis->setRange(qMax(0.0, range.lower + shiftAmount), range.upper + shiftAmount);
    m_customPlot->replot();
}

void StockPriceChart::handleHorizontalPanning(QWheelEvent* event) {
    DEBUG << "Horizontal panning with wheel";

    QCPRange range = m_customPlot->xAxis->range();
    qreal indexRange = range.size();
    qreal shiftAmount = indexRange * 0.05;
    shiftAmount = -shiftAmount * (event->angleDelta().x() > 0 ? 1 : -1);

    qreal newMin = range.lower + shiftAmount;
    qreal newMax = range.upper + shiftAmount;
    
    if (!indexToBar.isEmpty() && newMin < indexToBar.firstKey()) {
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
    }
    
    m_customPlot->xAxis->setRange(newMin, newMax);
    m_customPlot->replot();
}

void StockPriceChart::handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    QCPRange range = m_customPlot->xAxis->range();
    qreal centerIndex = range.center();

    qreal newSize = range.size() * zoomFactor;
    qreal newMin = centerIndex - (newSize / 2);
    qreal newMax = centerIndex + (newSize / 2);
    
    if (!indexToBar.isEmpty() && newMin < indexToBar.firstKey()) {
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMin));
        checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
    }

    m_customPlot->xAxis->setRange(newMin, newMax);
    m_customPlot->replot();
}

void StockPriceChart::handleVerticalZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    QCPRange range = m_customPlot->yAxis->range();
    qreal center = range.center();

    qreal newSize = range.size() * zoomFactor;
    qreal newMin = center - (newSize / 2);
    qreal newMax = center + (newSize / 2);

    m_customPlot->yAxis->setRange(qMax(0.0, newMin), newMax);
    m_customPlot->replot();
}

void StockPriceChart::handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor) {
    Q_UNUSED(event);

    // X-axis zoom
    QCPRange xRange = m_customPlot->xAxis->range();
    qreal centerIndex = xRange.center();
    qreal newXSize = xRange.size() * zoomFactor;
    qreal newMinX = centerIndex - (newXSize / 2);
    qreal newMaxX = centerIndex + (newXSize / 2);
    
    if (!indexToBar.isEmpty() && newMinX < indexToBar.firstKey()) {
        QDateTime requestTime = getTimestampForIndex(static_cast<int>(newMinX));
        checkForMissingBars(requestTime, indexToBar.first().getTimeStamp());
    }

    // Y-axis zoom
    QCPRange yRange = m_customPlot->yAxis->range();
    qreal centerPrice = yRange.center();
    qreal newYSize = yRange.size() * zoomFactor;
    qreal newMinY = centerPrice - (newYSize / 2);
    qreal newMaxY = centerPrice + (newYSize / 2);

    m_customPlot->xAxis->setRange(newMinX, newMaxX);
    m_customPlot->yAxis->setRange(qMax(0.0, newMinY), newMaxY);
    m_customPlot->replot();
}

/**
 * @brief Converts an index to a time string for axis labels.
 */
QString StockPriceChart::indexToTimeString(double index) const {
    QDateTime timestamp = getTimestampForIndex(static_cast<int>(index));
    return timestamp.toString("hh:mm");
}
