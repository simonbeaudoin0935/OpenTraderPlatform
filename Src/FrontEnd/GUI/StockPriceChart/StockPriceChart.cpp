#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "MarketHours.h"
#include "Misc/Settings.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY ChartLog
#define CANDLESTICK_BODY_WIDTH 0.9 // 90% of available space

Q_LOGGING_CATEGORY(ChartLog, "Chart");

/**
 * @brief Constructs a StockPriceChart widget using qcustomplot.
 */
StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent)
{
    // Create the custom plot widget
    m_customPlot = new QCustomPlot(this);
    Q_CHECK_PTR(m_customPlot);
    
    m_customPlot->installEventFilter(this);
    
    // Create candlestick chart
    m_candlesticks = new QCPFinancial(m_customPlot->xAxis, m_customPlot->yAxis);
    Q_CHECK_PTR(m_candlesticks);
    
    m_candlesticks->setName("Candlestick");
    m_candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    m_candlesticks->setWidth(CANDLESTICK_BODY_WIDTH);
    m_candlesticks->setTwoColored(true);
    m_candlesticks->setBrushPositive(QColor(0, 180, 0));      // Green for up
    m_candlesticks->setBrushNegative(QColor(200, 0, 0));      // Red for down
    m_candlesticks->setPenPositive(QPen(QColor(0, 180, 0)));
    m_candlesticks->setPenNegative(QPen(QColor(200, 0, 0)));

    // Setup axes
    m_customPlot->xAxis->setLabel("");
    m_customPlot->yAxis->setLabel("");
    
    // Apply dark theme
    m_customPlot->setBackground(QBrush(QColor(75, 75, 80)));
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
    m_volumeAxisRect->setBackground(QBrush(QColor(75, 75, 80)));
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

    // Interconnect x axis ranges of main and bottom axis rects (bidirectional for horizontal zoom)
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
    m_customPlot->setInteractions(QCP::iRangeDrag);
    m_customPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_customPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    
    // Enable horizontal zoom and drag on volume chart independently
    m_volumeAxisRect->setRangeDrag(Qt::Horizontal);
    m_volumeAxisRect->setRangeZoom(Qt::Horizontal);
    
    // Set initial ranges
    m_customPlot->xAxis->setRange(-3, 3);
    m_customPlot->yAxis->setRange(0, 100);

    // Create layout and add widgets
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create and add the timeframe selector at the top
    chartToolbar = new ChartToolbar(this);
    Q_CHECK_PTR(chartToolbar);
    layout->addWidget(chartToolbar, 0);  // 0 stretch - keep minimal size
    
    layout->addWidget(m_customPlot, 1);  // 1 stretch - expand to fill space
    setLayout(layout);

    // Initialize replay functionality
    replayTimeRangeWatcher = new QFutureWatcher<std::tuple<QDateTime, QDateTime, int>>(this);
    connect(replayTimeRangeWatcher, &QFutureWatcher<std::tuple<QDateTime, QDateTime, int>>::finished,
            this, &StockPriceChart::onReplayTimeRangeQueryFinished);

    // Connect timeframe selector signals
    connect(chartToolbar, &ChartToolbar::volumeChartVisibilityChanged,
            this, &StockPriceChart::onVolumeChartVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::volumeAutoRescaleChanged,
            this, &StockPriceChart::onVolumeAutoRescaleChanged);
    connect(chartToolbar, &ChartToolbar::replayDayChanged,
            this, &StockPriceChart::onReplayDayChanged);
    connect(chartToolbar, &ChartToolbar::wheelRatioChanged, this,
            [this](qreal ratio)
            { 
                this->wheelZoomRatio = ratio; 
                // Save to settings
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Chart/WheelZoomRatio", ratio);
                appStateSettings->sync();
            });

    // Load wheel zoom ratio from settings
    Q_CHECK_PTR(appStateSettings);
    qreal savedRatio = appStateSettings->value("Chart/WheelZoomRatio", 1.0).toReal();
    chartToolbar->setWheelRatio(savedRatio);
    wheelZoomRatio = savedRatio;

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
    // Removed stock ticker label to save space
    
    // Populate available replay days when symbol changes
    populateAvailableReplayDays();
    
    DEBUG << "Set chart symbol to" << symbol;
}

/**
 * @brief Populates the replay day dropdown with available dates from cache.
 */
void StockPriceChart::populateAvailableReplayDays() {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString barsDir = QString("%1/RecordedLiveData/Bars").arg(cacheDir);
    
    QDir dir(barsDir);
    if (!dir.exists()) {
        qCWarning(ChartLog) << "Bars directory does not exist:" << barsDir;
        return;
    }
    
    // Get all .db files in the directory
    QStringList filters;
    filters << "RecordedLiveBars_*.db";
    QStringList dbFiles = dir.entryList(filters, QDir::Files, QDir::Name);
    
    QList<QDate> availableDates;
    for (const QString& dbFile : dbFiles) {
        // Extract date from filename (format: RecordedLiveBars_YYYY-MM-DD.db)
        QString dateStr = dbFile.mid(18, 10); // Skip "RecordedLiveBars_" (18 chars) and take 10 chars for date
        QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
        if (date.isValid()) {
            availableDates.append(date);
        }
    }
    
    // Sort dates in descending order (most recent first)
    std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());
    
    chartToolbar->setAvailableReplayDays(availableDates);
    
    qCInfo(ChartLog) << "Found" << availableDates.size() << "available replay dates";
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
            << "isEndOfHistory:" << bar.getIsEndOfHistory()
            << "O:" << bar.getOpen()
            << "H:" << bar.getHigh()
            << "L:" << bar.getLow()
            << "C:" << bar.getClose();
    
    if (startedReceivingRealtimeBars == true) {
        Q_ASSERT(bar.getIsRealtime());
    } else {
        if (bar.getIsEndOfHistory()) {
            Q_ASSERT(!bar.getIsRealtime());
            startedReceivingRealtimeBars = true;

        }
    }

    // Is this the first bar ever received for this chart
    if (indexToBar.size() == 0) [[unlikely]] {

        // Sanity check: semaphore should be available (count == 1) for the first bar
        OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 1);

        // Important that this be aquired here to block further requests until we finish processing this first bar.
        // the ->setRange() calls below trigger a checkForMissingBars() immediately due to the direct connection
        // of the signal/slot
        bool acquired = m_missingBarsRequestSemaphore.tryAcquire();
        Q_ASSERT(acquired); // Should always succeed for first bar

        DEBUG << "Received first bar ";
        
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

        // Draw background rectangles for the session
        drawBackgroundsForReceivedBars({});

        // Here we will fetch the bars from the beginning of the day up to this bar to fill in history
        QDateTime first = QDateTime(bar.getTimeStamp().date(), QTime(TRADING_START_HOUR, 1, 0), QTimeZone("America/New_York"));
        QDateTime last = bar.getTimeStamp();

        DEBUG << "Requesting whole day bars from"
                << first.toString(Qt::ISODate)
                << "to"
                << last.toString(Qt::ISODate);

        emit requestMissingBars(first, last);
        return;
    }


    switch(bar.getBarStatus())
    {
    case Bar::BarStatus::Null:
        // Tradestation doesnt send 'null' bars, it is a construct that we created in this program
        Q_ASSERT(false);
        break;

    case Bar::BarStatus::Closed:
        {
            if (startedReceivingRealtimeBars) {
//                Q_ASSERT(m_latestBar.getBarStatus() == Bar::BarStatus::Open);
                // TradeStation sends a 'closed' bar to close the current candle. Therefore, its timestamp is the one
                // from the current candle
//                Q_ASSERT(bar.getTimeStamp() == m_latestBar.getTimeStamp());
            } else {
                Q_ASSERT(m_latestBar.getBarStatus() == Bar::BarStatus::Closed);
                Q_ASSERT(bar.getTimeStamp() > m_latestBar.getTimeStamp());
            }



            const int newIndex = indexToBar.lastKey() + 1;

            timestampToIndex[bar.getTimeStamp()] = newIndex;
            indexToBar[newIndex] = bar;
            m_latestBar = bar;
            m_latestBarIndex = newIndex;
        }
        break;

    case Bar::BarStatus::Open:
        if (m_latestBar.getBarStatus() == Bar::BarStatus::Closed){
            // New bar after previous one was closed

            Q_ASSERT(bar.getTimeStamp() > m_latestBar.getTimeStamp());
            break;
        } else {
            // Updating existing open bar
            Q_ASSERT(bar.getTimeStamp() == m_latestBar.getTimeStamp());

            indexToBar[m_latestBarIndex] = bar;
            m_latestBar = bar;

            break;
        }
        break;
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
        const Bar::BarStatus status = bar.getBarStatus();
        
        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed) {
            QCPFinancialData data;
            data.key = index;  // Use index as the x-axis value
            data.open = bar.getOpen();
            data.high = bar.getHigh();
            data.low = bar.getLow();
            data.close = bar.getClose();
            financialData.append(std::move(data));
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
        const Bar::BarStatus status = bar.getBarStatus();
        
        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed) {
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
    
    // Rescale volume axis to fit visible data
    rescaleVolumeAxisToVisibleRange();
}

/**
 * @brief Rescales the volume Y-axis based on the second highest volume of visible bars.
 * 
 * Uses the second highest volume bar to determine the Y-axis range, which helps avoid
 * erratic single high-volume bars from skewing the scale. If there's only one bar with
 * volume, the highest is used instead.
 * 
 * Only performs rescaling if m_volumeAutoRescaleEnabled is true.
 */
void StockPriceChart::rescaleVolumeAxisToVisibleRange()
{
    if (!m_volumeAutoRescaleEnabled) {
        return;
    }

    if (indexToBar.isEmpty()) {
        return;
    }

    // Get the visible X range
    const QCPRange xRange = m_customPlot->xAxis->range();
    const int visibleStart = qMax(static_cast<int>(qFloor(xRange.lower)), indexToBar.firstKey());
    const int visibleEnd = qMin(static_cast<int>(qCeil(xRange.upper)), indexToBar.lastKey());

    // Track the two highest volumes
    qint64 highestVolume = 0;
    qint64 secondHighestVolume = 0;
    
    for (int i = visibleStart; i <= visibleEnd; ++i) {
        if (indexToBar.contains(i)) {
            const Bar& bar = indexToBar[i];
            const Bar::BarStatus status = bar.getBarStatus();
            if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed) {
                const qint64 volume = static_cast<qint64>(bar.getTotalVolume());
                if (volume > highestVolume) {
                    secondHighestVolume = highestVolume;
                    highestVolume = volume;
                } else if (volume > secondHighestVolume) {
                    secondHighestVolume = volume;
                }
            }
        }
    }

    // Use second highest if available, otherwise fall back to highest
    const qint64 scaleVolume = (secondHighestVolume > 0) ? secondHighestVolume : highestVolume;

    if (scaleVolume > 0) {
        // Add small padding (5%) at the top for visual clarity
        const double upperBound = scaleVolume * 1.05;
        m_volumeAxisRect->axis(QCPAxis::atLeft)->setRange(0.0, upperBound);
    }
}

/**
 * @brief Updates the background colors for different market sessions.
 * 
 * Draws colored background rectangles to indicate:
 * - Pre-market hours (brownish/orange)
 * - After-hours (blueish/violet)
 * 
 * Only draws backgrounds for the currently visible time range.
 */
/**
 * @brief Draws background rectangles for trading sessions based on received bars.
 * 
 * This is called once when bars are received, creating fixed rectangles that
 * QCustomPlot will automatically clip to the visible range.
 * 
 * @param bars The bars that were just received (typically a complete day's worth)
 */
void StockPriceChart::drawBackgroundsForReceivedBars(const QVector<Bar>& bars) {
    if (bars.isEmpty()) {
        return;
    }
    
    // Get all dates that have bars
    QTimeZone nyZone("America/New_York");
    QSet<QDate> datesWithBars;
    
    for (const Bar& bar : indexToBar) {
        QDateTime barTime = bar.getTimeStamp();
        QDateTime nyTime = barTime.toTimeZone(nyZone);
        QDate date = nyTime.date();
        
        // Skip weekends
        if (date.dayOfWeek() > 5) {
            continue;
        }
        
        datesWithBars.insert(date);
    }
    
    // Draw full session rectangles only for dates that don't already have backgrounds
    for (const QDate& date : datesWithBars) {
        // Skip if we already have backgrounds for this date
        if (m_datesWithBackgrounds.contains(date)) {
            continue;
        }
        
        // Create pre-market and after-hours rectangles for this date
        // Note: Chart data only covers 6:01 AM to 8:00 PM, so we clamp the visual ranges
        // Pre-market: 6:01 AM to 9:30 AM (data starts at 6:01, regular trading at 9:30)
        // After-hours: 4:00 PM to 8:00 PM (regular trading ends at 4:00, data ends at 8:00)
        QDateTime preMarketStartNY = QDateTime(date, QTime(TRADING_START_HOUR, 1), nyZone);  // 6:01 AM
        QDateTime preMarketEndNY = QDateTime(date, QTime(9, 30), nyZone);
        QDateTime afterHoursStartNY = QDateTime(date, QTime(16, 0), nyZone);
        QDateTime afterHoursEndNY = QDateTime(date, QTime(TRADING_END_HOUR, 0), nyZone);  // 8:00 PM
        
        QDateTime preMarketStart = preMarketStartNY.toTimeZone(QTimeZone::utc());
        QDateTime preMarketEnd = preMarketEndNY.toTimeZone(QTimeZone::utc());
        QDateTime afterHoursStart = afterHoursStartNY.toTimeZone(QTimeZone::utc());
        QDateTime afterHoursEnd = afterHoursEndNY.toTimeZone(QTimeZone::utc());
        
        // Draw pre-market rectangle (6:01am - 9:30am ET)
        drawFixedBackgroundRect(preMarketStart, preMarketEnd, 
                              QColor(255, 165, 0, 180), m_preMarketRects);
        
        // Draw after-hours rectangle (4pm - 8pm ET)
        drawFixedBackgroundRect(afterHoursStart, afterHoursEnd, 
                              QColor(138, 43, 226, 180), m_afterHoursRects);
        
        // Mark this date as having backgrounds
        m_datesWithBackgrounds.insert(date);
        
        DEBUG << "Created background rectangles for date" << date.toString();
    }
}

/**
 * @brief Draws background rectangles for trading sessions based on all loaded bars.
 * 
 * Only creates backgrounds for dates that don't already have them.
 * QCustomPlot handles clipping to the visible range automatically.
 */
void StockPriceChart::drawBackgroundsForVisibleRange() {
    if (indexToBar.isEmpty()) {
        return;
    }
    
    // Get all dates that have bars
    QTimeZone nyZone("America/New_York");
    QSet<QDate> datesWithBars;
    
    for (const Bar& bar : indexToBar) {
        QDateTime barTime = bar.getTimeStamp();
        QDateTime nyTime = barTime.toTimeZone(nyZone);
        QDate date = nyTime.date();
        
        // Skip weekends
        if (date.dayOfWeek() > 5) {
            continue;
        }
        
        datesWithBars.insert(date);
    }
    
    // Draw backgrounds only for dates that don't already have them
    for (const QDate& date : datesWithBars) {
        // Skip if we already have backgrounds for this date
        if (m_datesWithBackgrounds.contains(date)) {
            continue;
        }
        
        // Create pre-market and after-hours rectangles for this date
        // Note: Chart data only covers 6:01 AM to 8:00 PM, so we clamp the visual ranges
        // Pre-market: 6:01 AM to 9:30 AM (data starts at 6:01, regular trading at 9:30)
        // After-hours: 4:00 PM to 8:00 PM (regular trading ends at 4:00, data ends at 8:00)
        QDateTime preMarketStartNY = QDateTime(date, QTime(TRADING_START_HOUR, 1), nyZone);  // 6:01 AM
        QDateTime preMarketEndNY = QDateTime(date, QTime(9, 30), nyZone);
        QDateTime afterHoursStartNY = QDateTime(date, QTime(16, 0), nyZone);
        QDateTime afterHoursEndNY = QDateTime(date, QTime(TRADING_END_HOUR, 0), nyZone);  // 8:00 PM
        
        QDateTime preMarketStart = preMarketStartNY.toTimeZone(QTimeZone::utc());
        QDateTime preMarketEnd = preMarketEndNY.toTimeZone(QTimeZone::utc());
        QDateTime afterHoursStart = afterHoursStartNY.toTimeZone(QTimeZone::utc());
        QDateTime afterHoursEnd = afterHoursEndNY.toTimeZone(QTimeZone::utc());
        
        // Draw pre-market rectangle (6:01am - 9:30am ET)
        drawFixedBackgroundRect(preMarketStart, preMarketEnd, 
                              QColor(255, 165, 0, 180), m_preMarketRects);
        
        // Draw after-hours rectangle (4pm - 8pm ET)
        drawFixedBackgroundRect(afterHoursStart, afterHoursEnd, 
                              QColor(138, 43, 226, 180), m_afterHoursRects);
        
        // Mark this date as having backgrounds
        m_datesWithBackgrounds.insert(date);
        
        DEBUG << "Created background rectangles for date" << date.toString();
    }
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
    
    // Clear the tracking set so backgrounds can be redrawn
    m_datesWithBackgrounds.clear();
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
/**
 * @brief Draws a fixed background rectangle for a specific time range.
 * 
 * Creates a rectangle with fixed coordinates that spans the entire Y-axis.
 * QCustomPlot will automatically handle clipping to the visible range.
 * 
 * @param rangeStart The start time of the range to highlight.
 * @param rangeEnd The end time of the range to highlight.
 * @param color The color for the background rectangle.
 * @param rectList The list to add the created rectangle to.
 */
void StockPriceChart::drawFixedBackgroundRect(const QDateTime& rangeStart, const QDateTime& rangeEnd,
                                               const QColor& color, QList<QCPItemRect*>& rectList) {
    if (indexToBar.isEmpty()) {
        return;
    }
    
    // Calculate indices for the time boundaries
    qreal sessionStartIndex = static_cast<qreal>(getIndexForTimestamp(rangeStart));
    qreal sessionEndIndex = static_cast<qreal>(getIndexForTimestamp(rangeEnd));
    
    // Create rectangle with fixed coordinates
    QCPItemRect* rect = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(rect);
    
    // Set the rectangle to use plot coordinates
    rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->yAxis);
    rect->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->yAxis);
    
    // Set fixed X coordinates (index range) and full Y range
    // Y coordinates will automatically adapt to axis range changes
    rect->topLeft->setCoords(sessionStartIndex, 0);
    rect->bottomRight->setCoords(sessionEndIndex, 0);
    
    // Use axis rect ratio for Y to span full height
    rect->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->topLeft->setCoords(sessionStartIndex, 0);  // 0 = top of axis rect
    rect->bottomRight->setCoords(sessionEndIndex, 1);  // 1 = bottom of axis rect
    
    rect->setPen(Qt::NoPen);
    rect->setBrush(QBrush(color));
    
    // Set layer to ensure rectangles are behind the data
    rect->setLayer("background");
    
    DEBUG << "Created fixed background rect from index" << sessionStartIndex << "to" << sessionEndIndex
          << "for time" << rangeStart.toString("hh:mm") << "-" << rangeEnd.toString("hh:mm")
          << "with color" << color.name();
    
    rectList.append(rect);
}

/**
 * @brief Handles the response to a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsReceived(const std::shared_ptr<QVector<Bar>> barsPtr) {
    
    DEBUG << "Received missing bars response with" << barsPtr->size() << "bars";
    
    // Sanity check: semaphore should be acquired (count == 0) when we receive the response
    OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 0);
    m_missingBarsRequestSemaphore.release();

    Q_ASSERT(!barsPtr->isEmpty());

    addHistoricalBarsToIndexMapping(*barsPtr);
    
    DEBUG << "After addHistoricalBarsToIndexMapping, index range:" 
          << QString("%1 to %2").arg(indexToBar.firstKey()).arg(indexToBar.lastKey());
    
    // Draw background rectangles for the visible range
    drawBackgroundsForVisibleRange();
    
    //#warning TODO: optimize redraws
    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();
    
    m_customPlot->replot();
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

    //WARNING << "checkForMissingBars";

    // Try to acquire the semaphore - if it fails, a request is already in progress
    if (!m_missingBarsRequestSemaphore.tryAcquire())
    {
        DEBUG << "Missing bars request already in progress, skipping";
        return;
    }

    Q_ASSERT(!indexToBar.isEmpty());

    DEBUG << "Check for missing bars for view range:"
          << viewStartTime.toString(Qt::ISODate)
          << "to"
          << viewEndTime.toString(Qt::ISODate);

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
    
    // Determine the appropriate time range to request
    QTimeZone nyZone("America/New_York");
    QDateTime firstBarInNY = firstBarTime.toTimeZone(nyZone);
    QDateTime viewStartInNY = viewStartTimeRounded.toTimeZone(nyZone);

    DEBUG << "firstBarInNY:" << firstBarInNY << "viewStartInNY:" << viewStartInNY;
    DEBUG << "firstBarInNY.date():" << firstBarInNY.date() << "viewStartInNY.date():" << viewStartInNY.date();
    DEBUG << "Date comparison:" << (viewStartInNY.date() < firstBarInNY.date());

    QDateTime requestStartTime;
    QDateTime requestEndTime;

    if (viewStartInNY.date() < firstBarInNY.date()) {
        // View extends to previous day(s) - request the complete previous day only
        // (we'll request additional days in subsequent calls if needed)
        QDateTime viewDayStart = QDateTime(viewStartInNY.date(), QTime(TRADING_START_HOUR, 1, 0), nyZone);
        QDateTime viewDayEnd = QDateTime(viewStartInNY.date(), QTime(TRADING_END_HOUR, 0, 0), nyZone);
        requestStartTime = viewDayStart.toTimeZone(firstBarTime.timeZone());
        requestEndTime = viewDayEnd.toTimeZone(firstBarTime.timeZone());
        DEBUG << "Requesting previous day from" << viewDayStart << "to" << viewDayEnd;
    } else {
        // View extends within the same day - request from start of day to one minute before firstBarTime
        QDateTime dayStart = QDateTime(firstBarInNY.date(), QTime(TRADING_START_HOUR, 1, 0), nyZone);
        requestStartTime = dayStart.toTimeZone(firstBarTime.timeZone());
        requestEndTime = firstBarTime.addSecs(-60);
        DEBUG << "Requesting same day, dayStart:" << dayStart << "requestStartTime:" << requestStartTime;
    }

    // Ensure we don't request invalid ranges
    if (requestStartTime >= requestEndTime) {
        DEBUG << "Invalid request range, skipping";
        return;
    }

    DEBUG << "Requesting missing bars from"
            << requestStartTime.toString(Qt::ISODate)
            << "to"
            << requestEndTime.toString(Qt::ISODate);

    emit requestMissingBars(requestStartTime, requestEndTime);
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
        
        DEBUG << "Received historical bar" 
            << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
            << "Status:" << Bar::barStatusToString(bar.getBarStatus())
            << "isEndOfHistory:" << bar.getIsEndOfHistory()
            << "O:" << bar.getOpen()
            << "H:" << bar.getHigh()
            << "L:" << bar.getLow()
            << "C:" << bar.getClose();

        //OBJ_ASSUME_TRUE(timestampToIndex.contains(timestamp));
        
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
 * @brief Gets the index corresponding to a timestamp.
 */
int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const {
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end()) {
        return it.value();
    }
    
    Q_ASSERT(!indexToBar.isEmpty());
    
    QDateTime firstTime = indexToBar.first().getTimeStamp();
    int firstIndex = indexToBar.firstKey();
    
    if (timestamp < firstTime) {
        // Calculate minutes before first bar
        qint64 minutesDiff = firstTime.toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
        Q_ASSERT(minutesDiff % 60 == 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return firstIndex - indexDiff;
    }
    
    QDateTime lastTime = indexToBar.last().getTimeStamp();
    int lastIndex = indexToBar.lastKey();
    
    if (timestamp > lastTime) {
        // Calculate minutes after last bar
        qint64 minutesDiff = timestamp.toSecsSinceEpoch() - lastTime.toSecsSinceEpoch();
        Q_ASSERT(minutesDiff % 60 == 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return lastIndex + indexDiff;
    }
    
    // Timestamp is between existing bars - interpolate
    auto lowerIt = timestampToIndex.lowerBound(timestamp);
    if (lowerIt == timestampToIndex.begin()) {
        // Should not happen since we checked < firstTime
        return firstIndex;
    } else if (lowerIt == timestampToIndex.end()) {
        // Should not happen since we checked > lastTime
        return lastIndex;
    } else {
        // Interpolate between prev and next
        auto prevIt = std::prev(lowerIt);
        QDateTime prevTime = prevIt.key();
        QDateTime nextTime = lowerIt.key();
        int prevIndex = prevIt.value();
        int nextIndex = lowerIt.value();
        
        qint64 totalSeconds = prevTime.secsTo(nextTime);
        qint64 secondsFromPrev = prevTime.secsTo(timestamp);
        
        if (totalSeconds == 0) return prevIndex;
        
        double fraction = static_cast<double>(secondsFromPrev) / totalSeconds;
        return prevIndex + static_cast<int>((nextIndex - prevIndex) * fraction);
    }
}




/**
 * @brief Converts an index to a time string for axis labels.
 */
QString StockPriceChart::indexToTimeString(double index) const {
    QDateTime timestamp = getTimestampForIndex(static_cast<int>(index));
    return timestamp.toString("hh:mm");
}

/**
 * @brief Handles replay day selection changes.
 */
void StockPriceChart::onReplayDayChanged(const QDate& date) {
    if (m_symbol.isEmpty()) {
        qCWarning(ChartLog) << "No symbol selected for replay day query";
        return;
    }

    // Cancel any ongoing query
    if (replayTimeRangeWatcher->isRunning()) {
        Q_ASSERT(false); // TO_DELETE
        replayTimeRangeWatcher->cancel();
    }

    // Start asynchronous query for time range
    QFuture<std::tuple<QDateTime, QDateTime, int>> future = QtConcurrent::run([this, date]() {
        return queryStockTimeRangeForDate(m_symbol, date);
    });
    replayTimeRangeWatcher->setFuture(future);
}

/**
 * @brief Handles completion of replay time range query.
 */
void StockPriceChart::onReplayTimeRangeQueryFinished() {
    if (replayTimeRangeWatcher->isCanceled()) {
        return;
    }

    std::tuple<QDateTime, QDateTime, int> timeRangeResult = replayTimeRangeWatcher->result();
    
    if (std::get<0>(timeRangeResult).isValid() && std::get<1>(timeRangeResult).isValid()) {
        qCInfo(ChartLog) << "Found replay time range for" << m_symbol 
                        << "from" << std::get<0>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") 
                        << "to" << std::get<1>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") 
                        << "(NY timezone) -" << std::get<2>(timeRangeResult) << "bars available";
        
        // Update the toolbar with the time range and bar count info
        QTime startTime = std::get<0>(timeRangeResult).time();
        QTime endTime = std::get<1>(timeRangeResult).time();
        int barCount = std::get<2>(timeRangeResult);
        chartToolbar->updateReplayInfo(startTime, endTime, barCount);
        
        // Pre-fill the time input widget with the earliest available time
        chartToolbar->setReplayStartTime(startTime);
    } else {
        qCWarning(ChartLog) << "No data found for" << m_symbol << "on selected date";
        // Clear the info label when no data is found
        chartToolbar->updateReplayInfo(QTime(), QTime(), 0);
    }
}

/**
 * @brief Queries the database for the first and last timestamps of a stock on a specific date.
 * @param symbol The stock symbol to query
 * @param date The date to query (in NY timezone)
 * @return A tuple of QDateTime objects representing the first and last timestamps (in NY timezone) and the bar count
 */
std::tuple<QDateTime, QDateTime, int> StockPriceChart::queryStockTimeRangeForDate(const QString& symbol, const QDate& date) {
    std::tuple<QDateTime, QDateTime, int> result;
    
    // Build database path: ~/.cache/L2Trader/RecordedLiveData/Bars/RecordedLiveBars_YYYY-MM-DD.db
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString dbPath = QString("%1/RecordedLiveData/Bars/RecordedLiveBars_%2.db")
                    .arg(cacheDir, date.toString("yyyy-MM-dd"));
    
    if (!QFile::exists(dbPath)) {
        qCWarning(ChartLog) << "Database file does not exist:" << dbPath;
        return result;
    }
    
    // Open database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "replay_query");
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        qCWarning(ChartLog) << "Failed to open database:" << db.lastError().text();
        return result;
    }
    
    // Calculate epoch range for the date (start of day to end of day in NY timezone)
    QTimeZone nyZone("America/New_York");
    QDateTime dayStart(date, QTime(0, 0, 0), nyZone);
    QDateTime dayEnd(date, QTime(23, 59, 59, 999), nyZone);
    qint64 startEpochMs = dayStart.toMSecsSinceEpoch();
    qint64 endEpochMs = dayEnd.toMSecsSinceEpoch();
    
    // Query for min, max timestamps and count for the symbol
    QSqlQuery query(db);
    query.prepare("SELECT MIN(epochMs), MAX(epochMs), COUNT(*) FROM bars "
                  "WHERE stockTicker = ? AND epochMs >= ? AND epochMs <= ?");
    query.addBindValue(symbol);
    query.addBindValue(startEpochMs);
    query.addBindValue(endEpochMs);
    
    if (query.exec() && query.next()) {
        qint64 minEpochMs = query.value(0).toLongLong();
        qint64 maxEpochMs = query.value(1).toLongLong();
        int barCount = query.value(2).toInt();
        
        if (minEpochMs > 0 && maxEpochMs > 0) {
            // Convert from UTC to New York timezone
            std::get<0>(result) = QDateTime::fromMSecsSinceEpoch(minEpochMs, Qt::UTC).toTimeZone(nyZone);
            std::get<1>(result) = QDateTime::fromMSecsSinceEpoch(maxEpochMs, Qt::UTC).toTimeZone(nyZone);
            std::get<2>(result) = barCount;
            
            qCInfo(ChartLog) << "Database query result for" << symbol << "on" << date.toString("yyyy-MM-dd") 
                            << ":" << barCount << "bars found";
        } else {
            qCInfo(ChartLog) << "No bars found for" << symbol << "on" << date.toString("yyyy-MM-dd");
        }
    } else {
        qCWarning(ChartLog) << "Query failed:" << query.lastError().text();
    }
    
    // Clean up database connection
    db.close();
    QSqlDatabase::removeDatabase("replay_query");
    
    return result;
}
