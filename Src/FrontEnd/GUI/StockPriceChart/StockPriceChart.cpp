#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "IndexToTimeTicker.h"
#include "Misc/Settings.h"
#include "Logging.h"
#include "Assume.h"
#include "SQL/StockPriceChartQueries.h"
#include "BarCache.h"
#include "MainApp.h"
#include "Order.h"
#include "Position.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"

#define LOGGING_CATEGORY ChartLog


Q_LOGGING_CATEGORY(ChartLog, "Chart");

/**
 * @brief Constructs a StockPriceChart widget using qcustomplot.
 */
StockPriceChart::StockPriceChart(QWidget* parent) : QWidget(parent)
{
    // Create the custom plot widget
    m_customPlot = new QCustomPlot(this);
    Q_CHECK_PTR(m_customPlot);

    m_customPlot->installEventFilter(this);

    // Create candlestick chart attached to right axis
    m_candlesticks = new QCPFinancial(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    Q_CHECK_PTR(m_candlesticks);

    m_candlesticks->setName("Candlestick");
    m_candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    m_candlesticks->setWidth(ChartConstants::CANDLESTICK_BODY_WIDTH);
    m_candlesticks->setTwoColored(true);
    m_candlesticks->setBrushPositive(QColor(0, 180, 0)); // Green for up
    m_candlesticks->setBrushNegative(QColor(200, 0, 0)); // Red for down
    m_candlesticks->setPenPositive(QPen(Qt::black));     // Black contour and wicks
    m_candlesticks->setPenNegative(QPen(Qt::black));     // Black contour and wicks

    // Setup axes - hide left axis and show right axis
    m_customPlot->xAxis->setLabel("");
    m_customPlot->yAxis->setVisible(false);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setVisible(true);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setLabel("");

    // Apply dark theme
    m_customPlot->setBackground(QBrush(QColor(75, 75, 80)));
    m_customPlot->xAxis->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->setLabelColor(QColor(220, 220, 220));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setVisible(true);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin

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

    // Apply dark theme to volume axis rect - hide left axis, show right axis
    m_volumeAxisRect->setBackground(QBrush(QColor(75, 75, 80)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
    m_volumeAxisRect->axis(QCPAxis::atRight)->setVisible(true);
    m_volumeAxisRect->axis(QCPAxis::atRight)->setBasePen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atRight)->setTickPen(QPen(QColor(220, 220, 220)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220));
    m_volumeAxisRect->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setVisible(true);
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin

    // Create two bar plottables for positive (green) and negative (red) volume bars
    m_customPlot->setAutoAddPlottableToLegend(false);
    m_volumePos = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atRight));
    Q_CHECK_PTR(m_volumePos);
    m_volumeNeg = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atRight));
    Q_CHECK_PTR(m_volumeNeg);

    m_volumePos->setWidth(ChartConstants::CANDLESTICK_BODY_WIDTH);
    m_volumePos->setPen(Qt::NoPen);
    m_volumePos->setBrush(QColor(100, 180, 110));
    m_volumeNeg->setWidth(ChartConstants::CANDLESTICK_BODY_WIDTH);
    m_volumeNeg->setPen(Qt::NoPen);
    m_volumeNeg->setBrush(QColor(180, 90, 90));

    // Interconnect x axis ranges of main and bottom axis rects (bidirectional for horizontal zoom)
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_volumeAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_volumeAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));

    // Configure axes of both main and bottom axis rect
    // Note: Custom time ticker is set up later when first bar is received,
    // because it needs indexToBar to be populated to convert indices to timestamps.
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false); // Hide until we have data
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_customPlot->xAxis->setBasePen(Qt::NoPen);
    m_customPlot->xAxis->setTickLabels(false);
    m_customPlot->xAxis->setTicks(false); // Only want vertical grid in main axis rect

    // Make axis rects' left side line up
    QCPMarginGroup* group = new QCPMarginGroup(m_customPlot);
    m_customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_volumeAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);

    // Create last price line
    m_lastPriceLine = new QCPItemLine(m_customPlot);
    Q_CHECK_PTR(m_lastPriceLine);
    m_lastPriceLine->setPen(QPen(Qt::green, 1, Qt::DashLine));
    m_lastPriceLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_lastPriceLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));

    // Create price label
    m_priceLabel = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_priceLabel);
    m_priceLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_priceLabel->position->setType(QCPItemPosition::ptPlotCoords);
    m_priceLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_priceLabel->setFont(QFont(font().family(), 10, QFont::Bold));
    m_priceLabel->setColor(Qt::green);
    m_priceLabel->setPadding(QMargins(5, 2, 5, 2));
    m_priceLabel->setBrush(QBrush(QColor(0, 0, 0, 150)));
    m_priceLabel->setVisible(false);

    // Create symbol watermark (center-top, behind candlesticks)
    m_symbolWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_symbolWatermark);
    m_symbolWatermark->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_symbolWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_symbolWatermark->position->setCoords(0.5, 0.05); // Center-top of chart
    m_symbolWatermark->setText("");
    m_symbolWatermark->setFont(QFont(font().family(), 48, QFont::Bold));
    m_symbolWatermark->setColor(QColor(255, 255, 255, 30)); // Pale transparent white
    m_symbolWatermark->setLayer("background");              // Draw behind candlesticks

    // Create current time vertical line
    m_currentTimeLine = new QCPItemLine(m_customPlot);
    Q_CHECK_PTR(m_currentTimeLine);
    m_currentTimeLine->setPen(QPen(Qt::white, 2, Qt::SolidLine));
    m_currentTimeLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_currentTimeLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_currentTimeLine->setVisible(false); // Initially hidden until first bar is received

    // Create timer for updating the current time line position
    m_timeLineTimer = new QTimer(this);
    Q_CHECK_PTR(m_timeLineTimer);
    m_timeLineTimer->setInterval(1000); // Update every second
    connect(m_timeLineTimer, &QTimer::timeout, this, &StockPriceChart::updateCurrentTimeLine);

    // Enable mouse interactions
    m_customPlot->setInteractions(QCP::iRangeDrag);
    m_customPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_customPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    // Set which axes to use for dragging and zooming
    m_customPlot->axisRect()->setRangeDragAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_customPlot->axisRect()->setRangeZoomAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));

    // Enable horizontal zoom and drag on volume chart independently
    m_volumeAxisRect->setRangeDrag(Qt::Horizontal);
    m_volumeAxisRect->setRangeZoom(Qt::Horizontal);

    // Set initial ranges
    m_customPlot->xAxis->setRange(-3, 3);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    // Create layout and add widgets
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create and add the timeframe selector at the top
    chartToolbar = new ChartToolbar(this);
    Q_CHECK_PTR(chartToolbar);
    layout->addWidget(chartToolbar,
                      0); // 0 stretch - keep minimal size

    layout->addWidget(m_customPlot,
                      1); // 1 stretch - expand to fill space
    setLayout(layout);

    // Initialize replay functionality
    replayTimeRangeWatcher = new QFutureWatcher<std::tuple<QDateTime, QDateTime, int>>(this);
    connect(replayTimeRangeWatcher,
            &QFutureWatcher<std::tuple<QDateTime, QDateTime, int>>::finished,
            this,
            &StockPriceChart::onReplayTimeRangeQueryFinished);

    // Connect timeframe selector signals
    connect(chartToolbar,
            &ChartToolbar::volumeChartVisibilityChanged,
            this,
            &StockPriceChart::onVolumeChartVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::volumeAutoRescaleChanged, this, &StockPriceChart::onVolumeAutoRescaleChanged);
    connect(chartToolbar,
            &ChartToolbar::orderVisualizationsVisibilityChanged,
            this,
            &StockPriceChart::onOrderVisualizationsVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::replayDayChanged, this, &StockPriceChart::onReplayDayChanged);
    connect(chartToolbar, &ChartToolbar::replayStartTimeChanged, this, &StockPriceChart::onReplayTimeChanged);
    connect(chartToolbar,
            &ChartToolbar::wheelRatioChanged,
            this,
            [this](qreal ratio)
            {
                this->wheelZoomRatio = ratio;
                // Save to settings
                Q_CHECK_PTR(appStateSettings);
                appStateSettings->setValue("Chart/WheelZoomRatio", ratio);
                appStateSettings->sync();
            });

    // Connect replay play/pause button
    connect(chartToolbar,
            &ChartToolbar::replayPlayPauseToggled,
            this,
            [this](bool playing)
            {
                if (playing)
                {
                    QDate date = chartToolbar->getSelectedReplayDay();
                    QTime startTime = chartToolbar->getReplayStartTime();
                    ReplayEngine::PlaybackSpeed speed = chartToolbar->getReplaySpeed();

                    if (MainApp::getInstance()->isReplayPaused())
                    {
                        // Resume from pause
                        MainApp::getInstance()->resumeReplayPlayback();
                    }
                    else if (date.isValid())
                    {
                        // Start new playback
                        MainApp::getInstance()->startReplayPlayback(date, startTime, speed);
                    }
                    else
                    {
                        qWarning() << "Cannot start replay: no valid date selected";
                        chartToolbar->setReplayPlaying(false);
                    }
                }
                else
                {
                    // Pause playback (stay in replay mode)
                    MainApp::getInstance()->pauseReplayPlayback();
                }
            });

    // Connect replay speed change
    connect(chartToolbar,
            &ChartToolbar::replaySpeedChanged,
            this,
            [](ReplayEngine::PlaybackSpeed speed) { MainApp::getInstance()->setReplaySpeed(speed); });

    // Load wheel zoom ratio from settings
    Q_CHECK_PTR(appStateSettings);
    qreal savedRatio = appStateSettings->value("Chart/WheelZoomRatio", 1.0).toReal();
    chartToolbar->setWheelRatio(savedRatio);
    wheelZoomRatio = savedRatio;

    // Connect axis range change signals
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this,
            &StockPriceChart::onAxisRangeChanged);
    connect(m_customPlot->axisRect()->axis(QCPAxis::atRight),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this,
            &StockPriceChart::onAxisRangeChanged);

    setSymbol("");
}

StockPriceChart::~StockPriceChart()
{
    // Qt parent-child hierarchy handles cleanup
}

/**
 * @brief Sets the stock symbol for the chart.
 */
void StockPriceChart::setSymbol(const QString& symbol)
{
    // Clear order visualizations from previous symbol immediately
    clearOrderVisualizations();

    m_symbol = symbol;
    m_candlesticks->setName(symbol + " (Bars)");

    // Update symbol watermark
    m_symbolWatermark->setText(symbol);
    m_customPlot->replot();

    // Populate available replay days when symbol changes
    populateAvailableReplayDays();

    // Historical orders/positions loaded after first bar is received (need bar data for index calculation)

    DEBUG << "Set chart symbol to" << symbol;
}

/**
 * @brief Populates the replay day dropdown with available dates from cache.
 */
void StockPriceChart::populateAvailableReplayDays()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString barsDir = QString("%1/RecordedLiveData/Bars").arg(cacheDir);

    QDir dir(barsDir);
    if (!dir.exists())
    {
        WARNING << "Bars directory does not exist:" << barsDir;
        return;
    }

    // Get all .db files in the directory (format: YYYY-MM-DD.db)
    QStringList filters;
    filters << "*.db";
    QStringList dbFiles = dir.entryList(filters, QDir::Files, QDir::Name);

    QList<QDate> availableDates;
    for (const QString& dbFile: dbFiles)
    {
        // Extract date from filename (format: YYYY-MM-DD.db)
        QString baseName = dbFile;
        if (baseName.endsWith(".db"))
        {
            baseName.chop(3); // Remove ".db"
        }
        QDate date = QDate::fromString(baseName, "yyyy-MM-dd");
        if (date.isValid())
        {
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
    OBJ_ASSUME_EQUAL(symbol, m_symbol);
    OBJ_ASSUME_TRUE(bar.isValid());

    WARNING << "Received bar for" << symbol << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
            << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "isEndOfHistory:" << bar.getIsEndOfHistory()
            << "O:" << bar.getOpen() << "H:" << bar.getHigh() << "L:" << bar.getLow() << "C:" << bar.getClose();

    // Putting unlikely because only at the start will this condition be true,
    // so optimizing for the cruising case
    if (startedReceivingRealtimeBars == false) [[unlikely]]
    {
        if (bar.getIsEndOfHistory())
        {
            OBJ_ASSUME_FALSE(bar.getIsRealtime());
            startedReceivingRealtimeBars = true;
        }
    }
    else if (bar.getIsRealtime() == false) [[unlikely]]
    {
        WARNING << "We received a double of the last bar of the day for symbol" << m_symbol
                << "at timestamp:" << bar.getTimeStamp()
                << "- Experimentally, this has proven to be possible from the API."
                   " It seems to be a little glitch from their side when the app sits idle after hours.";

        return; // Ignore this bar
    }

    // Is this the first bar ever received for this chart
    if (indexToBar.size() == 0) [[unlikely]]
    {
        // Should be the case becase we call openBarStream() with barsback=1,
        // so we always get at least one historical bar first
        if (MainApp::isInReplayMode() == false)
        {
            OBJ_ASSUME_TRUE(bar.getIsEndOfHistory() == true);
        }

        // Sanity check: semaphore should be available (count == 1) for the first bar
        OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 1);

        // Important that this be aquired here to block further requests until we finish processing this first bar.
        // the ->setRange() calls below trigger a checkForMissingBars() immediately due to the direct connection
        // of the signal/slot
        bool acquired = m_missingBarsRequestSemaphore.tryAcquire();
        OBJ_ASSUME_TRUE(acquired); // Should always succeed for first bar

        DEBUG << "Received first bar " << bar;

        // Now that we have the first bar, we can set up the custom time ticker
        // that converts index values to time labels
        QSharedPointer<IndexToTimeTicker> indexToTimeTicker(new IndexToTimeTicker);
        indexToTimeTicker->setTimeFormat("hh:mm");
        indexToTimeTicker->setIndexToTimestampFunction([this](int index) { return this->getTimestampForIndex(index); });
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(true);

        // Also set the same ticker on main chart's X-axis so grid lines align with nice times
        m_customPlot->xAxis->setTicker(indexToTimeTicker);

        OBJ_ASSUME_EQUAL(timestampToIndex.size(), 0);

        // The indexd of the first bar received when opening the stream is always 0
        // Historical bars fetched will go in the negative indices, and future bars in positive indices
        const int index = 0;

        timestampToIndex[bar.getTimeStamp()] = index;
        indexToBar[index] = bar;
        m_latestBar = bar;
        m_latestBarIndex = index;

        // Update candlestick data
        updateCandlestickData();
        updateVolumeData();

        // Center index 0 with 1 hour (60 bars) on each side
        m_customPlot->xAxis->setRange(-60, 60);

        double newPrice = bar.getClose();
        double padding = newPrice * 0.0002;
        double minRange = newPrice * 0.0005;
        m_customPlot->axisRect()
            ->axis(QCPAxis::atRight)
            ->setRange(qMax(0.0, newPrice - minRange / 2 - padding), newPrice + minRange / 2 + padding);

        m_customPlot->replot();

        // Draw background rectangles for the session
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});

        // Start the timer for updating the current time line
        m_currentTimeLine->setVisible(true);
        m_timeLineTimer->start();
        updateCurrentTimeLine(); // Update immediately

        // Here we will fetch the bars from the beginning of the day up to this bar to fill in history
        QDateTime first = QDateTime(bar.getTimeStamp().date(),
                                    TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                    TradingHours::MARKET_TIMEZONE);

        QDateTime last = bar.getTimeStamp().addSecs(-60);

        // Only fetch history if there are bars before the first streaming bar
        if (last >= first)
        {
            DEBUG << "Requesting whole day bars from" << first.toString(Qt::ISODate) << "to"
                  << last.toString(Qt::ISODate);
            emit requestMissingBars(first, last);
        }
        else
        {
            DEBUG << "First bar is the earliest candle, no history to fetch";
            m_missingBarsRequestSemaphore.release();
        }

        return;
    }

    if (MainApp::isInReplayMode() == false)
    {
        // Any live bar after the first one shall have the isEndOfHistory flag false
        OBJ_ASSUME_TRUE(bar.getIsEndOfHistory() == false);
    }

    switch (bar.getBarStatus())
    {
    case Bar::BarStatus::Uninitialized:
        // Should not receive uninitialized bars
        Q_UNREACHABLE();
        break;

    case Bar::BarStatus::Null:
        // Tradestation doesnt send 'null' bars, it is a construct that we created in this program
        Q_UNREACHABLE();
        break;

    case Bar::BarStatus::Closed:
    {
        if (startedReceivingRealtimeBars)
        {
            //                Q_ASSERT(m_latestBar.getBarStatus() == Bar::BarStatus::Open);
            // TradeStation sends a 'closed' bar to close the current candle. Therefore, its timestamp is the one
            // from the current candle
            //                OBJ_ASSUME_EQUAL(bar.getTimeStamp(), m_latestBar.getTimeStamp());
        }
        else
        {
            OBJ_ASSUME_TRUE(m_latestBar.getBarStatus() == Bar::BarStatus::Closed);
            OBJ_ASSUME_GT(bar.getTimeStamp(), m_latestBar.getTimeStamp());
        }

        // When we receive a closed bar, we shall not increase the latestBarIndex,
        // because the next bar to be received will be an open bar for the next candle
        // Therefore, we just replace the existing latest bar at latestBarIndex
        indexToBar[m_latestBarIndex] = bar;

        // Especially here, we need to store this bar with status = closed, because the
        // next open bar will need to know that the previous bar was closed.
        m_latestBar = bar;
    }
    break;

    case Bar::BarStatus::Open:
        if (m_latestBar.getBarStatus() == Bar::BarStatus::Closed)
        {
#warning I hit this assert. fuck tradestation
            // New bar after previous one was closed
            OBJ_ASSUME_GT(bar.getTimeStamp(), m_latestBar.getTimeStamp());

            const int newIndex = indexToBar.lastKey() + 1;

            timestampToIndex[bar.getTimeStamp()] = newIndex;
            indexToBar[newIndex] = bar;
            m_latestBar = bar;
            m_latestBarIndex = newIndex;
        }
        else
        {
            // Updating existing open bar
            if (bar.getTimeStamp() != m_latestBar.getTimeStamp())
            {
                CRITICAL << "Timestamp mismatch when updating existing open bar: new bar timestamp"
                         << bar.getTimeStamp().toString(Qt::ISODate) << "does not match latest bar timestamp"
                         << m_latestBar.getTimeStamp().toString(Qt::ISODate);

                return;
            }

            indexToBar[m_latestBarIndex] = bar;
            m_latestBar = bar;
        }
        break;
    }

    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    // Draw session backgrounds if this bar belongs to a new date (e.g., next day's pre-market just started)
    if (!m_datesWithBackgrounds.contains(bar.getTimeStamp().date()))
    {
        drawBackgroundsForReceivedBars(QVector<Bar>{bar});
    }

    redrawLastPriceLine();

    // Update open position dynamic line and P&L box with current price
    if (m_currentOpenPosition && !m_currentOpenPosition->isClosed)
    {
        double currentPrice = bar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    m_customPlot->replot();
}

/**
 * @brief Updates the candlestick data from indexToBar map.
 */
void StockPriceChart::updateCandlestickData()
{
    // Convert our bar data to QCPFinancialData format
    QVector<QCPFinancialData> financialData;

    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();

        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            QCPFinancialData barData;
            barData.key = index; // Use index as the x-axis value
            barData.open = bar.getOpen();
            barData.high = bar.getHigh();
            barData.low = bar.getLow();
            barData.close = bar.getClose();
            financialData.append(std::move(barData));
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

    for (auto it = indexToBar.begin(); it != indexToBar.end(); ++it)
    {
        const int index = it.key();
        const Bar& bar = it.value();
        const Bar::BarStatus status = bar.getBarStatus();

        // Only include bars with valid status (Open or Closed) - skip Null and Uninitialized bars
        if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
        {
            // Determine if bar is up or down based on close vs open
            bool isUp = bar.getClose() >= bar.getOpen();
            qint64 volume = bar.getTotalVolume();

            // Add to appropriate bar series
            if (isUp)
            {
                m_volumePos->addData(index, volume);
            }
            else
            {
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
    if (!m_volumeAutoRescaleEnabled)
    {
        return;
    }

    if (indexToBar.isEmpty())
    {
        return;
    }

    // Get the visible X range
    const QCPRange xRange = m_customPlot->xAxis->range();
    const int visibleStart = qMax(static_cast<int>(qFloor(xRange.lower)), indexToBar.firstKey());
    const int visibleEnd = qMin(static_cast<int>(qCeil(xRange.upper)), indexToBar.lastKey());

    // Track the two highest volumes
    qint64 highestVolume = 0;
    qint64 secondHighestVolume = 0;

    for (int i = visibleStart; i <= visibleEnd; ++i)
    {
        if (indexToBar.contains(i))
        {
            const Bar& bar = indexToBar[i];
            const Bar::BarStatus status = bar.getBarStatus();
            if (status == Bar::BarStatus::Open || status == Bar::BarStatus::Closed)
            {
                const qint64 volume = static_cast<qint64>(bar.getTotalVolume());
                if (volume > highestVolume)
                {
                    secondHighestVolume = highestVolume;
                    highestVolume = volume;
                }
                else if (volume > secondHighestVolume)
                {
                    secondHighestVolume = volume;
                }
            }
        }
    }

    // Use second highest if available, otherwise fall back to highest
    const qint64 scaleVolume = (secondHighestVolume > 0) ? secondHighestVolume : highestVolume;

    if (scaleVolume > 0)
    {
        // Add small padding (5%) at the top for visual clarity
        const double upperBound = scaleVolume * 1.05;
        m_volumeAxisRect->axis(QCPAxis::atRight)->setRange(0.0, upperBound);
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
void StockPriceChart::drawBackgroundsForReceivedBars(const QVector<Bar>& bars)
{
    OBJ_ASSUME_FALSE(bars.isEmpty());

    QSet<QDate> datesToDrawRectancles;

    // Go through all received bars to find unique dates
    for (const Bar& bar: bars)
    {
        OBJ_ASSUME_TRUE(bar.getTimeStamp().timeZone() == TradingHours::MARKET_TIMEZONE);

        datesToDrawRectancles.insert(bar.getTimeStamp().date());
    }

    // Draw full session rectangles only for dates that don't already have backgrounds
    for (const QDate& date: datesToDrawRectancles)
    {
        // Skip if we already have backgrounds for this date
        if (m_datesWithBackgrounds.contains(date))
        {
            continue;
        }

        // Draw early pre-market rectangle (4:01am - 6:00am ET) - paler orange
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 90),
                                m_earlyPreMarketRects);

        // Draw pre-market rectangle (6:01am - 9:30am ET)
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION,
                                QColor(255, 165, 0, 180),
                                m_preMarketRects);

        // Draw after-hours rectangle (4pm - 8:00pm ET)
        drawFixedBackgroundRect(date,
                                TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION,
                                TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                QColor(138, 43, 226, 180),
                                m_afterHoursRects);

        // Mark this date as having backgrounds
        m_datesWithBackgrounds.insert(date);

        DEBUG << "Created background rectangles for date" << date.toString();
    }
}

/**
 * @brief Clears all background rectangles from the chart.
 */
void StockPriceChart::clearBackgroundRects()
{
    // Delete and clear early pre-market rectangles
    for (auto rect: m_earlyPreMarketRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_earlyPreMarketRects.clear();

    // Delete and clear pre-market rectangles
    for (auto rect: m_preMarketRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_preMarketRects.clear();

    // Delete and clear after-hours rectangles
    for (auto rect: m_afterHoursRects)
    {
        m_customPlot->removeItem(rect);
    }
    m_afterHoursRects.clear();

    // Clear the tracking set so backgrounds can be redrawn
    m_datesWithBackgrounds.clear();
}

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
void StockPriceChart::drawFixedBackgroundRect(const QDate& date,
                                              const QTime& rangeStart,
                                              const QTime& rangeEnd,
                                              const QColor& color,
                                              QList<QCPItemRect*>& rectList)
{
    OBJ_ASSUME_FALSE(indexToBar.isEmpty());
    OBJ_ASSUME_TRUE(date.dayOfWeek() >= Qt::Monday && date.dayOfWeek() <= Qt::Friday);

    QDateTime rangeStartDT(date, rangeStart, TradingHours::MARKET_TIMEZONE);
    QDateTime rangeEndDT(date, rangeEnd, TradingHours::MARKET_TIMEZONE);

    // Calculate indices for the time boundaries
    qreal sessionStartIndex = static_cast<qreal>(getIndexForTimestamp(rangeStartDT));
    qreal sessionEndIndex = static_cast<qreal>(getIndexForTimestamp(rangeEndDT));

    sessionStartIndex -= 0.5; // Make start index inclusive of the first candle
    sessionEndIndex += 0.5;   // Make end index inclusive of the last candle

    // Create rectangle with fixed coordinates
    QCPItemRect* rect = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(rect);

    // Set the rectangle to use plot coordinates
    rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    rect->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));

    // Set fixed X coordinates (index range) and full Y range
    // Y coordinates will automatically adapt to axis range changes
    rect->topLeft->setCoords(sessionStartIndex, 0);
    rect->bottomRight->setCoords(sessionEndIndex, 0);

    // Use axis rect ratio for Y to span full height
    rect->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    rect->topLeft->setCoords(sessionStartIndex,
                             0); // 0 = top of axis rect
    rect->bottomRight->setCoords(sessionEndIndex,
                                 1); // 1 = bottom of axis rect

    rect->setPen(Qt::NoPen);
    rect->setBrush(QBrush(color));

    // Set layer to ensure rectangles are behind the data
    rect->setLayer("background");

    DEBUG << "Created fixed background rect from index" << sessionStartIndex << "to" << sessionEndIndex << "for time"
          << rangeStart.toString("hh:mm") << "-" << rangeEnd.toString("hh:mm") << "with color" << color.name();

    rectList.append(rect);
}

/**
 * @brief Handles the response to a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsReceived(const std::shared_ptr<QVector<Bar>>& barsPtr)
{

    DEBUG << "Received missing bars response with" << barsPtr->size() << "bars";

    // Sanity check: semaphore should be acquired (count == 0) when we receive the response
    OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 0);
    m_missingBarsRequestSemaphore.release();

    OBJ_ASSUME_FALSE(barsPtr->isEmpty());

    addHistoricalBarsToIndexMapping(barsPtr);

    DEBUG << "After addHistoricalBarsToIndexMapping, index range:"
          << QString("%1 to %2").arg(indexToBar.firstKey()).arg(indexToBar.lastKey());

    // Draw background rectangles for the visible range
    drawBackgroundsForReceivedBars(*barsPtr);

    // Only set initial Y-axis range on the first batch of historical bars
    if (!m_initialYAxisRangeSet)
    {
        // Compute min/max price from the last 60 non-null bars to set initial Y-axis range
        // We skip Null (void) bars since they have 0 prices and would distort the range
        double minPrice = std::numeric_limits<double>::max();
        double maxPrice = std::numeric_limits<double>::lowest();
        int barsAnalyzed = 0;

        for (int i = barsPtr->size() - 1; i >= 0 && barsAnalyzed < 60; --i)
        {
            const Bar& bar = barsPtr->at(i);
            if (bar.getBarStatus() == Bar::BarStatus::Null)
            {
                continue;
            }
            minPrice = qMin(minPrice, bar.getLow());
            maxPrice = qMax(maxPrice, bar.getHigh());
            ++barsAnalyzed;
        }

        if (minPrice < maxPrice)
        {
            double padding = (maxPrice - minPrice) * 0.05; // 5% padding
            m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(minPrice - padding, maxPrice + padding);
        }

        m_initialYAxisRangeSet = true;
    }

    //#warning TODO: optimize redraws
    // Update candlestick data
    updateCandlestickData();
    updateVolumeData();

    m_customPlot->replot();

    // Now that we have bars, load historical orders and positions
    loadHistoricalOrders();
    loadHistoricalPositions();
}

/**
 * @brief Handles the failure of a missing bars request.
 */
void StockPriceChart::onRequestedMissingBarsFailed()
{
    DEBUG << "Missing bars request failed, releasing semaphore";

    // Sanity check: semaphore should be acquired (count == 0) when we receive the failure notification
    OBJ_ASSUME_TRUE(m_missingBarsRequestSemaphore.available() == 0);
    m_missingBarsRequestSemaphore.release();
}

/**
 * @brief Updates the horizontal last price line.
 * The line is "sticky" - when the price is outside the visible Y-axis range,
 * it sticks to the top or bottom edge of the chart to maintain visual awareness.
 */
void StockPriceChart::redrawLastPriceLine()
{
    if (m_latestBarIndex == -1)
    {
        m_priceLabel->setVisible(false);
        return;
    }

    double lastPrice = m_latestBar.getClose();
    QColor lineColor = (m_latestBar.getClose() >= m_latestBar.getOpen()) ? Qt::green : Qt::red;

    // Get the visible Y-axis range
    QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();

    // Clamp the line position to visible range (sticky behavior)
    double displayPrice = lastPrice;
    if (lastPrice < yRange.lower)
    {
        displayPrice = yRange.lower;
    }
    else if (lastPrice > yRange.upper)
    {
        displayPrice = yRange.upper;
    }

    // Update line at clamped position
    m_lastPriceLine->start->setCoords(m_customPlot->xAxis->range().lower, displayPrice);
    m_lastPriceLine->end->setCoords(m_customPlot->xAxis->range().upper, displayPrice);
    m_lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));

    // Update label at clamped position, but show actual price value
    m_priceLabel->setText(QString::number(lastPrice, 'f', 2));
    m_priceLabel->setColor(lineColor);
    m_priceLabel->position->setCoords(m_customPlot->xAxis->range().upper, displayPrice);
    m_priceLabel->setVisible(true);
}

/**
 * @brief Checks if the current view requires missing bars to be loaded.
 */
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime)
{
    Q_UNUSED(viewEndTime);

    // Try to acquire the semaphore - if it fails, a request is already in progress
    if (!m_missingBarsRequestSemaphore.tryAcquire())
    {
        DEBUG << "Missing bars request already in progress, skipping";
        return;
    }

    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    DEBUG << "Check for missing bars for view range:" << viewStartTime.toString(Qt::ISODate) << "to"
          << viewEndTime.toString(Qt::ISODate);

    QDateTime viewStartTimeRounded = viewStartTime;

    // Round down to the nearest minute
    viewStartTimeRounded = viewStartTimeRounded.addSecs(-viewStartTimeRounded.time().second());
    viewStartTimeRounded = viewStartTimeRounded.addMSecs(-viewStartTimeRounded.time().msec());

    // Adjust to valid trading hours
    viewStartTimeRounded = adjustToValidTradingTime(viewStartTimeRounded);

    const QDateTime firstBarTime = timestampToIndex.firstKey();
    OBJ_ASSUME_TRUE(firstBarTime.timeZone() == TradingHours::MARKET_TIMEZONE);

    if (viewStartTimeRounded >= firstBarTime)
    {
        // View is within available bars - release semaphore before returning
        m_missingBarsRequestSemaphore.release();
        return;
    }

    DEBUG << "Chart view extends beyond available bars:";
    DEBUG << "  Last :" << firstBarTime;
    DEBUG << "  First:" << viewStartTimeRounded;
    DEBUG << "firstBarInNY:" << firstBarTime << "viewStartInNY:" << viewStartTimeRounded;
    DEBUG << "firstBarInNY.date():" << firstBarTime.date() << "viewStartInNY.date():" << viewStartTimeRounded.date();
    DEBUG << "Date comparison:" << (viewStartTimeRounded.date() < firstBarTime.date());

    QDateTime requestStartTime;
    QDateTime requestEndTime;

    if (viewStartTimeRounded.date() < firstBarTime.date())
    {
        requestStartTime = viewStartTimeRounded;
        requestStartTime.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);

        requestEndTime = viewStartTimeRounded;
        requestEndTime.setTime(TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

        DEBUG << "Requesting previous day from" << requestStartTime << "to" << requestEndTime;
    }
    else
    {
        requestStartTime = firstBarTime;
        requestStartTime.setTime(TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);

        requestEndTime = firstBarTime;
        requestEndTime = requestEndTime.addSecs(-60);

        DEBUG << "Requesting for same day from" << requestStartTime << "to" << requestEndTime;
    }

    OBJ_ASSUME_LT(requestStartTime, requestEndTime);

    emit requestMissingBars(requestStartTime, requestEndTime);
}

/**
 * @brief Clears all data and resets the chart for a new symbol.
 */
void StockPriceChart::clearSymbol()
{
    m_candlesticks->data()->clear();
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    clearBackgroundRects();

    indexToBar.clear();
    timestampToIndex.clear();

    m_latestBarIndex = -1;
    m_latestBar = Bar();

    m_priceLabel->setVisible(false);
    m_symbolWatermark->setText("");

    // Stop and hide the current time line
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);

    // Reset state flags for new symbol
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;

    // Reset semaphore to available state (1) for new symbol
    // If it was acquired (count == 0), release it; if already available, do nothing
    if (m_missingBarsRequestSemaphore.available() == 0)
    {
        DEBUG << "Releasing semaphore during clearSymbol - previous request was in-flight";
        m_missingBarsRequestSemaphore.release();
    }

    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_customPlot->replot();
}

void StockPriceChart::clearChart()
{
    INFO << "Clearing chart data for replay mode";

    // Clear order visualizations immediately
    clearOrderVisualizations();

    // Clear all candlestick and volume data
    m_candlesticks->data()->clear();
    m_volumePos->data()->clear();
    m_volumeNeg->data()->clear();
    clearBackgroundRects();

    // Clear index mappings
    indexToBar.clear();
    timestampToIndex.clear();

    // Reset bar tracking
    m_latestBarIndex = -1;
    m_latestBar = Bar();

    // Hide price label but keep symbol watermark (same symbol in replay)
    m_priceLabel->setVisible(false);

    // Stop current time line (replay has its own time)
    m_timeLineTimer->stop();
    m_currentTimeLine->setVisible(false);

    // Reset state flags
    startedReceivingRealtimeBars = false;
    m_initialYAxisRangeSet = false;

    // Reset semaphore to available state
    if (m_missingBarsRequestSemaphore.available() == 0)
    {
        DEBUG << "Releasing semaphore during clearChart - previous request was in-flight";
        m_missingBarsRequestSemaphore.release();
    }

    // Reset view range
    m_customPlot->xAxis->setRange(0, 30);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_customPlot->replot();

    DEBUG << "Chart cleared for replay mode";
}

/**
 * @brief Gets the previous valid trading minute.
 *
 * This function calculates the trading minute immediately preceding the given timestamp.
 * It handles timezone conversion to New York time, adjusts for weekends by rolling back
 * to the previous Friday, and ensures the result falls within valid trading hours
 * (4:00 AM to 8:00 PM ET on weekdays).
 *
 * @param timestamp The reference timestamp (in any timezone)
 * @return The previous trading minute as a QDateTime in the same timezone as the input
 */
QDateTime StockPriceChart::getPreviousTradingMinute(const QDateTime& timestamp) const
{
    OBJ_ASSUME_TRUE(timestamp.timeZone() == TradingHours::MARKET_TIMEZONE);

    QDateTime previousMinute = timestamp.addSecs(-60);

    QTime time = previousMinute.time();
    int dayOfWeek = previousMinute.date().dayOfWeek();

    if (dayOfWeek >= TradingHours::MONDAY && dayOfWeek <= TradingHours::FRIDAY)
    {
        if (time < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
        {
            QDateTime result = QDateTime(previousMinute.date().addDays(-1),
                                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                         TradingHours::MARKET_TIMEZONE);
            if (result.date().dayOfWeek() > TradingHours::FRIDAY)
            {
                QDate friday = getPreviousFriday(result.date());
                result = QDateTime(friday,
                                   TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                   TradingHours::MARKET_TIMEZONE);
            }
            return result.toTimeZone(timestamp.timeZone());
        }
        else if (time > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
        {
            DEBUG << "Unexpected: getPreviousTradingMinute called with time after 8PM:" << time;
            return QDateTime(previousMinute.date(),
                             TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                             TradingHours::MARKET_TIMEZONE);
        }
    }
    else
    {
        QDate friday = getPreviousFriday(previousMinute.date());
        return QDateTime(friday, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
    }

    OBJ_ASSUME_GTE(previousMinute.time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(previousMinute.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    return previousMinute;
}

/**
 * @brief Adjusts a timestamp to the nearest valid trading time.
 */
QDateTime StockPriceChart::adjustToValidTradingTime(const QDateTime& timestamp) const
{
    OBJ_ASSUME_TRUE(timestamp.timeZone() == TradingHours::MARKET_TIMEZONE);

    QTime time = timestamp.time();
    int dayOfWeek = timestamp.date().dayOfWeek();

    if (dayOfWeek > TradingHours::FRIDAY)
    {
        QDate friday = getPreviousFriday(timestamp.date());
        return QDateTime(friday, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
    }

    if (time < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        QDate previousDay = timestamp.date().addDays(-1);
        if (previousDay.dayOfWeek() > TradingHours::FRIDAY)
        {
            previousDay = getPreviousFriday(previousDay);
        }
        return QDateTime(previousDay,
                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                         TradingHours::MARKET_TIMEZONE);
    }
    else if (time >= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return QDateTime(timestamp.date(),
                         TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                         TradingHours::MARKET_TIMEZONE);
    }

    return timestamp;
}

/**
 * @brief Gets the previous Friday given a date.
 */
QDate StockPriceChart::getPreviousFriday(const QDate& date) const
{
    int dayOfWeek = date.dayOfWeek();
    if (dayOfWeek == TradingHours::FRIDAY)
    {
        return date;
    }
    else if (dayOfWeek > TradingHours::FRIDAY)
    {
        return date.addDays(-(dayOfWeek - TradingHours::FRIDAY));
    }
    else
    {
        return date.addDays(-(dayOfWeek + 2));
    }
}

/**
 * @brief Updates axis tick intervals dynamically based on screen density.
 */
void StockPriceChart::updateAxisLabelsDensity()
{
    // Simple implementation - qcustomplot handles most of this automatically
    // Can be enhanced later if needed
    m_customPlot->xAxis->setNumberFormat("g");
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setNumberFormat("f");
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setNumberPrecision(2);
}

/**
 * @brief Adds historical bars to the index mapping using negative indices.
 */
void StockPriceChart::addHistoricalBarsToIndexMapping(const std::shared_ptr<QVector<Bar>>& bars)
{
    OBJ_ASSUME_FALSE(bars->isEmpty());
    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    int minIndex = indexToBar.firstKey();

    DEBUG << "addHistoricalBarsToIndexMapping: adding" << bars->size() << "bars, starting minIndex:" << minIndex;

    for (auto it = bars->rbegin(); it != bars->rend(); ++it)
    {
        const Bar& bar = *it;
        const QDateTime& timestamp = bar.getTimeStamp();

        //DEBUG << "Received historical bar" << "at" << bar.getTimeStamp().toString("yyyy-MM-dd hh:mm:ss")
        //      << "Status:" << Bar::barStatusToString(bar.getBarStatus()) << "isEndOfHistory:" << bar.getIsEndOfHistory()
        //      << "O:" << bar.getOpen() << "H:" << bar.getHigh() << "L:" << bar.getLow() << "C:" << bar.getClose();

        //OBJ_ASSUME_TRUE(timestampToIndex.contains(timestamp));

        --minIndex;
        indexToBar[minIndex] = bar;
        timestampToIndex[timestamp] = minIndex;

        if (it - bars->rbegin() >= bars->size() - 3 || minIndex >= -3)
        {
            DEBUG << "  Assigned index" << minIndex << "to timestamp" << timestamp.toString("hh:mm:ss");
        }
    }

    DEBUG << "addHistoricalBarsToIndexMapping: completed, new minIndex:" << minIndex;
}

/**
 * @brief Gets the timestamp corresponding to an index.
 */
QDateTime StockPriceChart::getTimestampForIndex(int index) const
{
    auto it = indexToBar.find(index);
    if (it != indexToBar.end())
    {
        return it.value().getTimeStamp();
    }

    // If indexToBar is empty (e.g., after clearSymbol()), return invalid QDateTime
    // The IndexToTimeTicker will handle this gracefully by displaying the index as a number
    if (indexToBar.isEmpty())
    {
        return QDateTime();
    }

    if (index < 0)
    {
        int firstIndex = indexToBar.firstKey();
        QDateTime currentTime = indexToBar.first().getTimeStamp();
        int deltaIndex = firstIndex - index;

        for (int i = 0; i < deltaIndex; ++i)
        {
            currentTime = getPreviousTradingMinute(currentTime);
        }

        return currentTime;
    }

    int lastIndex = indexToBar.lastKey();
    if (index > lastIndex)
    {
        QDateTime lastTime = indexToBar.last().getTimeStamp();
        int deltaIndex = index - lastIndex;
        return lastTime.addSecs(deltaIndex * 60);
    }

    return QDateTime::currentDateTime();
}

/**
 * @brief Gets the index corresponding to a timestamp.
 */
int StockPriceChart::getIndexForTimestamp(const QDateTime& timestamp) const
{
    auto it = timestampToIndex.find(timestamp);
    if (it != timestampToIndex.end())
    {
        return it.value();
    }

    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    QDateTime firstTime = indexToBar.first().getTimeStamp();
    int firstIndex = indexToBar.firstKey();

    if (timestamp < firstTime)
    {
        // Calculate minutes before first bar
        qint64 minutesDiff = firstTime.toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
        OBJ_ASSUME_EQUAL(minutesDiff % 60, 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return firstIndex - indexDiff;
    }

    QDateTime lastTime = indexToBar.last().getTimeStamp();
    int lastIndex = indexToBar.lastKey();

    if (timestamp > lastTime)
    {
        // Calculate minutes after last bar
        qint64 minutesDiff = timestamp.toSecsSinceEpoch() - lastTime.toSecsSinceEpoch();
        OBJ_ASSUME_EQUAL(minutesDiff % 60, 0); // Should be exact minutes
        int indexDiff = minutesDiff / 60;
        return lastIndex + indexDiff;
    }

    // Timestamp is between existing bars - interpolate
    auto lowerIt = timestampToIndex.lowerBound(timestamp);
    if (lowerIt == timestampToIndex.begin())
    {
        // Should not happen since we checked < firstTime
        return firstIndex;
    }
    else if (lowerIt == timestampToIndex.end())
    {
        // Should not happen since we checked > lastTime
        return lastIndex;
    }
    else
    {
        // Interpolate between prev and next
        auto prevIt = std::prev(lowerIt);
        QDateTime prevTime = prevIt.key();
        QDateTime nextTime = lowerIt.key();
        int prevIndex = prevIt.value();
        int nextIndex = lowerIt.value();

        qint64 totalSeconds = prevTime.secsTo(nextTime);
        qint64 secondsFromPrev = prevTime.secsTo(timestamp);

        if (totalSeconds == 0)
            return prevIndex;

        double fraction = static_cast<double>(secondsFromPrev) / totalSeconds;
        return prevIndex + static_cast<int>((nextIndex - prevIndex) * fraction);
    }
}


/**
 * @brief Converts an index to a time string for axis labels.
 */
QString StockPriceChart::indexToTimeString(double index) const
{
    QDateTime timestamp = getTimestampForIndex(static_cast<int>(index));
    return timestamp.toString("hh:mm");
}

/**
 * @brief Handles replay day selection changes.
 */
void StockPriceChart::onReplayDayChanged(const QDate& date)
{
    if (m_symbol.isEmpty())
    {
        WARNING << "No symbol selected for replay day query";
        return;
    }

    // Cancel any ongoing query
    if (replayTimeRangeWatcher->isRunning())
    {
        Q_UNREACHABLE(); // TO_DELETE
        replayTimeRangeWatcher->cancel();
    }

    // If in replay mode, trigger preload with the new day and current time selection
    if (MainApp::isInReplayMode())
    {
        QTime currentTime = chartToolbar->getReplayStartTime();
        ReplayEngine::PlaybackSpeed currentSpeed = chartToolbar->getReplaySpeed();

        qCInfo(ChartLog) << "Preloading chart for new replay day:" << date.toString(Qt::ISODate) << "at"
                         << currentTime.toString("hh:mm");

        MainApp::getInstance()->preloadChartForReplay(date, currentTime, currentSpeed);
    }

    // Start asynchronous query for time range (for display info in toolbar)
    QFuture<std::tuple<QDateTime, QDateTime, int>> future =
        QtConcurrent::run([this, date]() { return queryStockTimeRangeForDate(m_symbol, date); });
    replayTimeRangeWatcher->setFuture(future);
}

/**
 * @brief Handles replay start time changes.
 *
 * When user changes the start time in paused state, triggers chart preload
 * with the new time but keeping the current day selected.
 */
void StockPriceChart::onReplayTimeChanged(const QTime& time)
{
    if (m_symbol.isEmpty())
    {
        WARNING << "No symbol selected for replay time change";
        return;
    }

    // If in replay mode, trigger preload with the current day and new time
    if (MainApp::isInReplayMode())
    {
        QDate currentDate = chartToolbar->getSelectedReplayDay();
        ReplayEngine::PlaybackSpeed currentSpeed = chartToolbar->getReplaySpeed();

        qCInfo(ChartLog) << "Preloading chart for new replay time:" << currentDate.toString(Qt::ISODate) << "at"
                         << time.toString("hh:mm");

        MainApp::getInstance()->preloadChartForReplay(currentDate, time, currentSpeed);
    }
}

/**
 * @brief Handles completion of replay time range query.
 */
void StockPriceChart::onReplayTimeRangeQueryFinished()
{
    if (replayTimeRangeWatcher->isCanceled())
    {
        return;
    }

    std::tuple<QDateTime, QDateTime, int> timeRangeResult = replayTimeRangeWatcher->result();

    if (std::get<0>(timeRangeResult).isValid() && std::get<1>(timeRangeResult).isValid())
    {
        qCInfo(ChartLog) << "Found replay time range for" << m_symbol << "from"
                         << std::get<0>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") << "to"
                         << std::get<1>(timeRangeResult).toString("yyyy-MM-dd hh:mm:ss t") << "(NY timezone) -"
                         << std::get<2>(timeRangeResult) << "bars available";

        // Update the toolbar with the time range and bar count info
        QTime startTime = std::get<0>(timeRangeResult).time();
        QTime endTime = std::get<1>(timeRangeResult).time();
        int barCount = std::get<2>(timeRangeResult);
        chartToolbar->updateReplayInfo(startTime, endTime, barCount);

        // Pre-fill the time input widget with the earliest available time
        chartToolbar->setReplayStartTime(startTime);
    }
    else
    {
        WARNING << "No data found for" << m_symbol << "on selected date";
        // Clear the info label when no data is found
        chartToolbar->updateReplayInfo(QTime(), QTime(), 0);
    }
}

/**
 * @brief Handles replay data loading failure (database not found, corrupted, etc.)
 *
 * Shows error message to user and reverts to previous valid state.
 */
void StockPriceChart::onReplayDataLoadFailed(const QString& errorMessage)
{
    WARNING << "Replay data load failed:" << errorMessage;

    // Update toolbar info display to show failure
    chartToolbar->updateReplayInfo(QTime(), QTime(), 0);

    // Log detailed error
    qCCritical(ChartLog) << "Failed to preload replay data:" << errorMessage;
}

/**
 * @brief Queries the database for the first and last timestamps of a stock on a specific date.
 * @param symbol The stock symbol to query
 * @param date The date to query (in NY timezone)
 * @return A tuple of QDateTime objects representing the first and last timestamps (in NY timezone) and the bar count
 */
std::tuple<QDateTime, QDateTime, int> StockPriceChart::queryStockTimeRangeForDate(const QString& symbol,
                                                                                  const QDate& date)
{
    std::tuple<QDateTime, QDateTime, int> result;

    // Build database path: ~/.cache/L2Trader/RecordedLiveData/Bars/YYYY-MM-DD.db
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString dbPath = QString("%1/RecordedLiveData/Bars/%2.db").arg(cacheDir, date.toString("yyyy-MM-dd"));

    if (!QFile::exists(dbPath))
    {
        WARNING << "Database file does not exist:" << dbPath;
        return result;
    }

    // Use a scoped block to ensure QSqlQuery goes out of scope before removeDatabase
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "replay_query");
        db.setDatabaseName(dbPath);

        if (!db.open())
        {
            WARNING << "Failed to open database:" << db.lastError().text();
            QSqlDatabase::removeDatabase("replay_query");
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
        query.prepare(StockPriceChartQueries::SELECT_STOCK_TIME_RANGE);
        query.addBindValue(symbol);
        query.addBindValue(startEpochMs);
        query.addBindValue(endEpochMs);

        if (query.exec() && query.next())
        {
            qint64 minEpochMs = query.value(0).toLongLong();
            qint64 maxEpochMs = query.value(1).toLongLong();
            int barCount = query.value(2).toInt();

            if (minEpochMs > 0 && maxEpochMs > 0)
            {
                // Convert from UTC to New York timezone
                std::get<0>(result) = QDateTime::fromMSecsSinceEpoch(minEpochMs, Qt::UTC).toTimeZone(nyZone);
                std::get<1>(result) = QDateTime::fromMSecsSinceEpoch(maxEpochMs, Qt::UTC).toTimeZone(nyZone);
                std::get<2>(result) = barCount;

                qCInfo(ChartLog) << "Database query result for" << symbol << "on" << date.toString("yyyy-MM-dd") << ":"
                                 << barCount << "bars found";
            }
            else
            {
                qCInfo(ChartLog) << "No bars found for" << symbol << "on" << date.toString("yyyy-MM-dd");
            }
        }
        else
        {
            WARNING << "Query failed:" << query.lastError().text();
        }

        db.close();
    } // QSqlQuery and QSqlDatabase go out of scope here

    QSqlDatabase::removeDatabase("replay_query");

    return result;
}

void StockPriceChart::setReplayModeActive(bool active)
{
    if (m_isReplayModeActive == active)
    {
        return;
    }

    m_isReplayModeActive = active;

    // Update background color
    QColor bgColor = active ? REPLAY_BACKGROUND_COLOR : NORMAL_BACKGROUND_COLOR;
    m_customPlot->setBackground(QBrush(bgColor));
    m_volumeAxisRect->setBackground(QBrush(bgColor));

    m_customPlot->replot();

    qCInfo(ChartLog) << "Replay mode visual" << (active ? "activated" : "deactivated");
}

/**
 * @brief Updates the position of the current time vertical line.
 *
 * This slot is called every second to move the vertical white line to the current time position.
 * The line is positioned based on the fractional index calculated from the current time.
 * Each minute corresponds to 1 index unit, so each second moves the line by 1/60 of an index.
 *
 * Note: TradeStation timestamps represent the closing time of a bar. For example, a bar covering
 * 4:00:00-4:00:59 has timestamp 4:01:00. Therefore, we subtract 60 seconds from the bar's
 * timestamp to get the opening time, which is the actual start of index 0.
 *
 * The line stops advancing after market close (8:00 PM) and resumes at market open (4:00 AM).
 */
void StockPriceChart::updateCurrentTimeLine()
{
    // The timer that triggers the update of the current time line should only have been activated
    // after receiving the first real-time bar, so indexToBar should not be empty.
    OBJ_ASSUME_FALSE(indexToBar.isEmpty());

    // Get the timestamp of the bar at index 0 (the first bar received)
    auto it = indexToBar.find(0);
    OBJ_ASSUME_FALSE(it == indexToBar.end());

    // Get the current application time (NY timezone)
    QDateTime currentTime = MainApp::getCurrentAppTime();

    // TradeStation timestamps represent the closing time of the bar interval.
    // Subtract 60 seconds to get the opening time (actual start of the bar).
    QDateTime zeroIndexTime = it->getTimeStamp().addSecs(-60);

    // Check if current time is within trading hours (4:00 AM - 8:00 PM ET)
    QTime currentTimeOfDay = currentTime.time();

    // Market open is at 4:00 AM (early pre-market), first bar timestamp is 4:01 AM
    QTime marketOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.addSecs(-60);
    QTime marketClose = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION; // 8:00 PM

    // Calculate the time difference in seconds
    qint64 secondsDiff = zeroIndexTime.secsTo(currentTime);

    // If we're before market open, cap at the start (4:00 AM position)
    if (currentTimeOfDay < marketOpen)
    {
        // Position line at 4:00 AM (start of trading day)
        QDateTime marketOpenTime(currentTime.date(), marketOpen, TradingHours::MARKET_TIMEZONE);
        secondsDiff = zeroIndexTime.secsTo(marketOpenTime);
    }
    // If we're after market close, cap at the end (8:00 PM position)
    else if (currentTimeOfDay > marketClose)
    {
        // Position line at 8:00 PM (end of trading day)
        QDateTime marketCloseTime(currentTime.date(), marketClose, TradingHours::MARKET_TIMEZONE);
        secondsDiff = zeroIndexTime.secsTo(marketCloseTime);
    }

    // Convert to fractional index position
    // Each minute is 1 index unit, so each second is 1/60.0 of an index
    double currentIndex = secondsDiff / 60.0;

    // There is a particularity with how the index and bar printing works;
    // a bar is placed at an index, but half of the bar is before and half after the index.
    // To center the line within the current minute, we subtract 0.5
    currentIndex -= 0.5; // Center the line within the current minute

    // Get the current Y-axis range for the line
    QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();

    // Position the vertical line at the calculated index
    m_currentTimeLine->start->setCoords(currentIndex, yRange.lower);
    m_currentTimeLine->end->setCoords(currentIndex, yRange.upper);

    // Use queued replot for better performance - allows batching multiple updates
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

// ========== Order Visualization Implementation ==========

double StockPriceChart::getExactIndexForTimestamp(const QDateTime& timestamp) const
{
    if (timestampToIndex.isEmpty())
    {
        return 0.0;
    }

    // Find the closest bar timestamp
    auto it = timestampToIndex.lowerBound(timestamp);

    if (it == timestampToIndex.end())
    {
        // Timestamp is after all bars, use last bar
        --it;
        int lastIndex = it.value();
        QDateTime lastBarTime = it.key();
        qint64 msDiff = lastBarTime.msecsTo(timestamp);
        // Assume 1-minute bars: 60000ms per index
        return lastIndex + (msDiff / 60000.0);
    }

    if (it == timestampToIndex.begin())
    {
        // Timestamp is before all bars
        int firstIndex = it.value();
        QDateTime firstBarTime = it.key();
        qint64 msDiff = timestamp.msecsTo(firstBarTime);
        return firstIndex - (msDiff / 60000.0);
    }

    // Interpolate between two bars
    QDateTime upperTime = it.key();
    int upperIndex = it.value();
    --it;
    QDateTime lowerTime = it.key();
    int lowerIndex = it.value();

    qint64 totalMs = lowerTime.msecsTo(upperTime);
    qint64 elapsedMs = lowerTime.msecsTo(timestamp);

    if (totalMs <= 0)
    {
        return lowerIndex;
    }

    double fraction = static_cast<double>(elapsedMs) / static_cast<double>(totalMs);
    return lowerIndex + fraction * (upperIndex - lowerIndex);
}

OrderMarker*
StockPriceChart::createOrderMarker(const QString& orderID, double index, double price, bool isBuy, bool filled)
{
    auto* marker = new OrderMarker();
    marker->orderID = orderID;
    marker->price = price;
    marker->isBuy = isBuy;
    marker->state = filled ? OrderMarker::State::Filled : OrderMarker::State::Pending;

    QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
    QCPItemText* textLabel = new QCPItemText(m_customPlot);
    textLabel->position->setAxes(m_customPlot->xAxis, yAxis);
    textLabel->position->setCoords(index + 0.5, price);
    textLabel->setFont(QFont("Arial", 14, QFont::Bold));
    textLabel->setPadding(QMargins(4, 4, 4, 4));

    if (isBuy)
    {
        textLabel->setText("▲");
        textLabel->setColor(filled ? ORDER_VIZ_GREEN : ORDER_VIZ_GREEN.lighter(130));
        textLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    }
    else
    {
        textLabel->setText("▼");
        textLabel->setColor(filled ? ORDER_VIZ_RED : ORDER_VIZ_RED.lighter(130));
        textLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    }

    marker->priceLabel = textLabel;
    m_orderMarkers.insert(orderID, marker);
    return marker;
}

OrderMarker* StockPriceChart::createBuyMarker(const QString& orderID, double index, double price, bool filled)
{
    return createOrderMarker(orderID, index, price, true, filled);
}

OrderMarker* StockPriceChart::createSellMarker(const QString& orderID, double index, double price, bool filled)
{
    return createOrderMarker(orderID, index, price, false, filled);
}

OrderMarker* StockPriceChart::createCancelledMarker(const QString& orderID, double index, double price)
{
    auto* marker = new OrderMarker();
    marker->orderID = orderID;
    marker->price = price;
    marker->state = OrderMarker::State::Cancelled;

    // Get the right Y-axis
    QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);

    // Create a simple text label showing cancelled state
    QCPItemText* textLabel = new QCPItemText(m_customPlot);
    textLabel->position->setAxes(m_customPlot->xAxis, yAxis);
    // Apply +0.5 offset to align markers correctly with candle timing
    textLabel->position->setCoords(index + 0.5, price);
    textLabel->setText("✖");
    textLabel->setFont(QFont("Arial", 14, QFont::Bold));
    textLabel->setColor(ORDER_VIZ_GRAY);
    textLabel->setPadding(QMargins(4, 4, 4, 4));
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    marker->priceLabel = textLabel;

    m_orderMarkers.insert(orderID, marker);
    return marker;
}

void StockPriceChart::updateMarkerState(OrderMarker* marker, OrderMarker::State newState)
{
    if (!marker || !marker->priceLabel)
    {
        return;
    }

    marker->state = newState;

    struct MarkerStyle
    {
        QString text;
        QColor color;
    };

    auto getStyle = [&]() -> MarkerStyle
    {
        if (newState == OrderMarker::State::Cancelled || newState == OrderMarker::State::Rejected)
        {
            return {"✖", ORDER_VIZ_GRAY};
        }

        bool isPending = (newState == OrderMarker::State::Pending);
        if (marker->isBuy)
        {
            QColor color = isPending ? ORDER_VIZ_GREEN.lighter(130) : ORDER_VIZ_GREEN;
            return {"▲", color};
        }
        else
        {
            QColor color = isPending ? ORDER_VIZ_RED.lighter(130) : ORDER_VIZ_RED;
            return {"▼", color};
        }
    };

    MarkerStyle style = getStyle();
    marker->priceLabel->setText(style.text);
    marker->priceLabel->setColor(style.color);
}

void StockPriceChart::moveMarkerToPrice(OrderMarker* marker, double newPrice)
{
    if (!marker || !marker->priceLabel)
    {
        return;
    }

    marker->price = newPrice;

    // Move text label to new price
    auto coords = marker->priceLabel->position->coords();
    marker->priceLabel->position->setCoords(coords.x(), newPrice);
}

void StockPriceChart::removeOrderMarker(const QString& orderID)
{
    auto it = m_orderMarkers.find(orderID);
    if (it == m_orderMarkers.end())
    {
        return;
    }

    OrderMarker* marker = it.value();

    // Remove text label from plot
    if (marker->priceLabel)
    {
        m_customPlot->removeItem(marker->priceLabel);
    }

    delete marker;
    m_orderMarkers.erase(it);
}

double StockPriceChart::clampIndexToValidRange(double index, int barCount) const
{
    if (index < FIRST_CANDLE_WINDOW)
    {
        DEBUG << "Index too far negative (" << index << "), clamping to" << FIRST_CANDLE_WINDOW;
        return FIRST_CANDLE_WINDOW;
    }

    if (index > barCount + FUTURE_BAR_TOLERANCE)
    {
        DEBUG << "Index too far in future (" << index << "), clamping to" << (barCount - 1);
        return barCount - 1;
    }

    return index;
}

QCPItemLine* StockPriceChart::createPositionLine(double x1, double y1, double x2, double y2, bool profitable)
{
    auto* line = new QCPItemLine(m_customPlot);
    QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
    line->start->setAxes(m_customPlot->xAxis, yAxis);
    line->end->setAxes(m_customPlot->xAxis, yAxis);
    QColor color = profitable ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    QPen pen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine); // Thin dotted line
    line->setPen(pen);
    line->start->setCoords(x1, y1);
    line->end->setCoords(x2, y2);
    return line;
}

void StockPriceChart::updateOpenPositionDynamicLine(double currentPrice, double currentIndex)
{
    if (!m_currentOpenPosition || m_currentOpenPosition->isClosed)
    {
        return;
    }

    // Get the last entry marker position
    if (m_currentOpenPosition->entryMarkers.isEmpty())
    {
        return;
    }

    OrderMarker* lastEntry = m_currentOpenPosition->entryMarkers.last();
    double entryIndex = getExactIndexForTimestamp(lastEntry->timestamp);
    double entryPrice = lastEntry->price;

    // Calculate if position is currently profitable
    bool profitable;
    if (m_currentOpenPosition->isShort)
    {
        profitable = currentPrice < m_currentOpenPosition->avgEntryPrice;
    }
    else
    {
        profitable = currentPrice > m_currentOpenPosition->avgEntryPrice;
    }

    // Create or update dynamic line
    if (!m_currentOpenPosition->dynamicLine)
    {
        m_currentOpenPosition->dynamicLine =
            createPositionLine(entryIndex, entryPrice, currentIndex, currentPrice, profitable);
    }
    else
    {
        QColor color = profitable ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
        m_currentOpenPosition->dynamicLine->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
        m_currentOpenPosition->dynamicLine->start->setCoords(entryIndex, entryPrice);
        m_currentOpenPosition->dynamicLine->end->setCoords(currentIndex, currentPrice);
    }
}

void StockPriceChart::ensureOpenPositionPLBox()
{
    if (!m_openPositionPLBox)
    {
        // Create text label (has its own background via setBrush)
        m_openPositionPLBox = new QCPItemText(m_customPlot);
        m_openPositionPLBox->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
        m_openPositionPLBox->position->setType(QCPItemPosition::ptPlotCoords);
        m_openPositionPLBox->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_openPositionPLBox->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE, QFont::Bold));
        m_openPositionPLBox->setPadding(QMargins(6, 4, 6, 4));
        m_openPositionPLBox->setBrush(QBrush(QColor(40, 40, 40, 190)));
    }
}

void StockPriceChart::updateOpenPositionPLBox(double currentPrice)
{
    if (!m_currentOpenPosition || m_currentOpenPosition->isClosed)
    {
        hideOpenPositionPLBox();
        return;
    }

    ensureOpenPositionPLBox();

    // Calculate unrealized P&L
    double avgEntry = m_currentOpenPosition->avgEntryPrice;
    int quantity = m_currentOpenPosition->currentQuantity;
    double unrealizedPL;

    if (m_currentOpenPosition->isShort)
    {
        unrealizedPL = (avgEntry - currentPrice) * quantity;
    }
    else
    {
        unrealizedPL = (currentPrice - avgEntry) * quantity;
    }

    // Position label one bar to the right of current bar, above current price line
    m_openPositionPLBox->position->setCoords(m_latestBarIndex + 1, currentPrice);

    // Format and display
    QString plText = QString("%1$%2").arg(unrealizedPL >= 0 ? "+" : "").arg(QString::number(unrealizedPL, 'f', 2));

    QColor textColor = unrealizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    m_openPositionPLBox->setColor(textColor);
    m_openPositionPLBox->setText(plText);
    m_openPositionPLBox->setVisible(m_orderVisualizationsVisible);
}

void StockPriceChart::hideOpenPositionPLBox()
{
    if (m_openPositionPLBox)
    {
        m_openPositionPLBox->setVisible(false);
    }
}

void StockPriceChart::createClosedPositionPLLabel(PositionVisualization* posViz)
{
    if (!posViz || posViz->exitMarkers.isEmpty())
    {
        return;
    }

    // Position the label near the last exit marker
    OrderMarker* lastExit = posViz->exitMarkers.last();
    double exitIndex = getExactIndexForTimestamp(lastExit->timestamp);
    double exitPrice = lastExit->price;

    // Create label
    posViz->plLabel = new QCPItemText(m_customPlot);
    posViz->plLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    posViz->plLabel->position->setType(QCPItemPosition::ptPlotCoords);
    posViz->plLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    posViz->plLabel->position->setCoords(exitIndex + 0.5, exitPrice);
    posViz->plLabel->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE));

    // Format P&L text
    QString plText =
        QString("%1$%2").arg(posViz->realizedPL >= 0 ? "+" : "").arg(QString::number(posViz->realizedPL, 'f', 2));

    QColor textColor = posViz->realizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    posViz->plLabel->setColor(textColor);
    posViz->plLabel->setText(plText);
}

double StockPriceChart::calculateDCAPrice(const PositionVisualization* posViz) const
{
    if (!posViz || posViz->entryMarkers.isEmpty())
    {
        return 0.0;
    }

    double totalCost = 0.0;
    int totalQuantity = 0;

    for (const OrderMarker* marker: posViz->entryMarkers)
    {
        totalCost += marker->price * marker->quantity;
        totalQuantity += marker->quantity;
    }

    return totalQuantity > 0 ? totalCost / totalQuantity : 0.0;
}

void StockPriceChart::finalizeClosedPosition(PositionVisualization* posViz)
{
    if (!posViz)
    {
        return;
    }

    posViz->isClosed = true;

    // Remove dynamic line
    if (posViz->dynamicLine)
    {
        m_customPlot->removeItem(posViz->dynamicLine);
        posViz->dynamicLine = nullptr;
    }

    // Create connection lines from last entry to each exit
    if (!posViz->entryMarkers.isEmpty() && !posViz->exitMarkers.isEmpty())
    {
        OrderMarker* lastEntry = posViz->entryMarkers.last();
        double entryIndex = getExactIndexForTimestamp(lastEntry->timestamp);

        for (OrderMarker* exitMarker: posViz->exitMarkers)
        {
            double exitIndex = getExactIndexForTimestamp(exitMarker->timestamp);
            bool profitable = posViz->realizedPL >= 0;
            auto* line = createPositionLine(entryIndex, lastEntry->price, exitIndex, exitMarker->price, profitable);
            posViz->exitLines.append(line);
        }
    }

    // Create P&L label
    createClosedPositionPLLabel(posViz);

    // Clear current open position if this was it
    if (m_currentOpenPosition == posViz)
    {
        m_currentOpenPosition = nullptr;
        hideOpenPositionPLBox();
    }
}

void StockPriceChart::clearOrderVisualizations()
{
    DEBUG << "Clearing" << m_orderMarkers.size() << "order markers and" << m_positionVisualizations.size()
          << "position visualizations";

    // Remove all order markers
    for (auto it = m_orderMarkers.begin(); it != m_orderMarkers.end(); ++it)
    {
        OrderMarker* marker = it.value();
        if (marker->priceLabel)
        {
            m_customPlot->removeItem(marker->priceLabel);
        }
        delete marker;
    }
    m_orderMarkers.clear();

    // Remove all position visualizations
    for (auto it = m_positionVisualizations.begin(); it != m_positionVisualizations.end(); ++it)
    {
        PositionVisualization* posViz = it.value();

        // Remove connection lines
        for (QCPItemLine* line: posViz->entryConnectionLines)
        {
            m_customPlot->removeItem(line);
        }
        for (QCPItemLine* line: posViz->exitLines)
        {
            m_customPlot->removeItem(line);
        }
        if (posViz->dynamicLine)
        {
            m_customPlot->removeItem(posViz->dynamicLine);
        }
        if (posViz->plLabel)
        {
            m_customPlot->removeItem(posViz->plLabel);
        }

        delete posViz;
    }
    m_positionVisualizations.clear();
    m_currentOpenPosition = nullptr;

    // Hide P&L box
    hideOpenPositionPLBox();

    DEBUG << "Order visualizations cleared";
}

void StockPriceChart::loadHistoricalOrders()
{
    if (m_symbol.isEmpty())
    {
        qCDebug(ChartLog) << "loadHistoricalOrders() - No symbol set";
        return;
    }

    OrdersDatabase* ordersDb = OrdersDatabase::getInstance();
    OBJ_ASSUME_DIFF(ordersDb, nullptr);
    OBJ_ASSUME_TRUE(ordersDb->isOpen());

    QMap<QString, std::tuple<Order, std::optional<qint64>>> allOrders = ordersDb->loadAllOrders();

    // Must have bars loaded before loading historical orders
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        qCDebug(ChartLog) << "loadHistoricalOrders() - No bars loaded yet, deferring";
        return;
    }

    int loadedCount = 0;
    for (auto it = allOrders.begin(); it != allOrders.end(); ++it)
    {
        const Order& order = std::get<0>(it.value());

        // Only process orders for the current symbol
        if (order.getSymbol() != m_symbol)
        {
            continue;
        }

        Order::Status status = order.getOrderStatus();

        if (status == Order::Status::OPN || status == Order::Status::ACK)
        {
            // Pending order
            onOrderPlaced(order);
            loadedCount++;
        }
        else if (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR)
        {
            // Filled order
            onOrderFilled(order);
            loadedCount++;
        }
        else if (status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC)
        {
            // Cancelled order
            onOrderCancelled(order);
            loadedCount++;
        }
    }

    qCDebug(ChartLog) << "loadHistoricalOrders() - Loaded" << loadedCount << "orders for symbol" << m_symbol;
}

void StockPriceChart::loadHistoricalPositions()
{
    if (m_symbol.isEmpty())
    {
        qCDebug(ChartLog) << "loadHistoricalPositions() - No symbol set";
        return;
    }

    PositionsDatabase* positionsDb = PositionsDatabase::getInstance();
    if (!positionsDb || !positionsDb->isOpen())
    {
        WARNING << "loadHistoricalPositions() - PositionsDatabase not available";
        return;
    }

    QMap<QString, Position> allPositions = positionsDb->loadAllPositions();

    int loadedCount = 0;
    for (auto it = allPositions.begin(); it != allPositions.end(); ++it)
    {
        const Position& position = it.value();

        // Only process positions for the current symbol
        if (position.getSymbol() != m_symbol)
        {
            continue;
        }

        int quantity = position.getQuantity().toInt();

        if (quantity == 0)
        {
            // Closed position
            onPositionClosed(position);
        }
        else
        {
            // Open position
            onPositionUpdated(position);
        }
        loadedCount++;
    }

    qCDebug(ChartLog) << "loadHistoricalPositions() - Loaded" << loadedCount << "positions for symbol" << m_symbol;
}

void StockPriceChart::updateOrderVisualizationsVisibility()
{
    // Update visibility of all markers
    for (OrderMarker* marker: m_orderMarkers)
    {
        if (marker->priceLabel)
        {
            marker->priceLabel->setVisible(m_orderVisualizationsVisible);
        }
    }

    // Update visibility of all position lines
    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        for (QCPItemLine* line: posViz->entryConnectionLines)
        {
            line->setVisible(m_orderVisualizationsVisible);
        }
        for (QCPItemLine* line: posViz->exitLines)
        {
            line->setVisible(m_orderVisualizationsVisible);
        }
        if (posViz->dynamicLine)
        {
            posViz->dynamicLine->setVisible(m_orderVisualizationsVisible);
        }
        if (posViz->plLabel)
        {
            posViz->plLabel->setVisible(m_orderVisualizationsVisible);
        }
    }

    // Update P&L box
    if (m_openPositionPLBox)
    {
        m_openPositionPLBox->setVisible(m_orderVisualizationsVisible && m_currentOpenPosition != nullptr);
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::cullOrderVisualizationsToVisibleRange()
{
    QCPRange xRange = m_customPlot->xAxis->range();

    // Cull order markers
    for (OrderMarker* marker: m_orderMarkers)
    {
        double index = getExactIndexForTimestamp(marker->timestamp);
        bool inRange = (index >= xRange.lower - 1 && index <= xRange.upper + 1);
        bool visible = m_orderVisualizationsVisible && inRange;

        if (marker->priceLabel)
        {
            marker->priceLabel->setVisible(visible);
        }
    }

    // Cull position lines (more complex - check if any part of line is visible)
    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        if (posViz->plLabel)
        {
            double labelX = posViz->plLabel->position->coords().x();
            posViz->plLabel->setVisible(m_orderVisualizationsVisible && labelX >= xRange.lower &&
                                        labelX <= xRange.upper);
        }
    }
}

// ========== Order Event Slots ==========

void StockPriceChart::onOrderPlaced(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    if (m_orderMarkers.contains(orderID))
    {
        return;
    }

    std::optional<double> limitPrice = order.getLimitPrice();
    std::optional<double> stopPrice = order.getStopPrice();

    if (!limitPrice.has_value() && !stopPrice.has_value())
    {
        return;
    }

    double price = limitPrice.has_value() ? limitPrice.value() : stopPrice.value();
    OBJ_ASSUME_GT(price, 0.01);

    double index = getExactIndexForTimestamp(order.getOpenedDateTime());
    bool isBuy = order.getTradeAction().toUpper().contains("BUY");

    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    OBJ_ASSUME_GT(barCount, 0);

    index = clampIndexToValidRange(index, barCount);

    DEBUG << "Order placed marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
          << "index=" << index;

    OrderMarker* marker;
    if (isBuy)
    {
        marker = createBuyMarker(orderID, index, price, false);
    }
    else
    {
        marker = createSellMarker(orderID, index, price, false);
    }

    marker->symbol = order.getSymbol();
    marker->timestamp = order.getOpenedDateTime();
    marker->quantity = order.getQuantity().toInt();
    marker->accountID = order.getAccountID();

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onOrderFilled(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        OrderMarker* marker = it.value();
        updateMarkerState(marker, OrderMarker::State::Filled);

        double fillPrice = order.getFilledPrice();
        if (qAbs(marker->price - fillPrice) > 0.001)
        {
            moveMarkerToPrice(marker, fillPrice);
        }

        marker->timestamp = order.getClosedDateTime();
    }
    else
    {
        double price = order.getFilledPrice();
        OBJ_ASSUME_GT(price, 0.01);

        double index = getExactIndexForTimestamp(order.getClosedDateTime());
        bool isBuy = order.getTradeAction().toUpper().contains("BUY");

        int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        OBJ_ASSUME_GT(barCount, 0);

        index = clampIndexToValidRange(index, barCount);

        DEBUG << "Order filled marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
              << "index=" << index;

        OrderMarker* marker;
        if (isBuy)
        {
            marker = createBuyMarker(orderID, index, price, true);
        }
        else
        {
            marker = createSellMarker(orderID, index, price, true);
        }

        marker->symbol = order.getSymbol();
        marker->timestamp = order.getClosedDateTime();
        marker->quantity = order.getQuantity().toInt();
        marker->accountID = order.getAccountID();
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onOrderCancelled(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        // Convert existing marker to cancelled state
        OrderMarker* marker = it.value();
        updateMarkerState(marker, OrderMarker::State::Cancelled);

        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
        qCDebug(ChartLog) << "Order cancelled marker updated:" << orderID;
    }
}

void StockPriceChart::onOrderAmended(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        OrderMarker* marker = it.value();
        double newPrice = order.getLimitPrice().value_or(order.getStopPrice().value_or(marker->price));

        if (qAbs(marker->price - newPrice) > 0.001)
        {
            moveMarkerToPrice(marker, newPrice);
            m_customPlot->replot(QCustomPlot::rpQueuedReplot);
            qCDebug(ChartLog) << "Order amended marker moved:" << orderID << "to" << newPrice;
        }
    }
}

void StockPriceChart::onPositionOpened(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        Q_UNREACHABLE();
        return;
    }

    QString positionID = position.getPositionID();

    // Create new position visualization
    auto* posViz = new PositionVisualization();
    posViz->positionID = positionID;
    posViz->symbol = position.getSymbol();
    posViz->isShort = position.getLongShort().toUpper() == "SHORT";
    posViz->currentQuantity = position.getQuantity().toInt();
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();

    m_positionVisualizations.insert(positionID, posViz);
    m_currentOpenPosition = posViz;

    qCDebug(ChartLog) << "Position opened:" << positionID << "qty:" << posViz->currentQuantity;
}

void StockPriceChart::onPositionUpdated(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        return;
    }

    QString positionID = position.getPositionID();
    auto it = m_positionVisualizations.find(positionID);

    if (it == m_positionVisualizations.end())
    {
        // Position doesn't exist yet, create it
        onPositionOpened(position);
        return;
    }

    PositionVisualization* posViz = it.value();

    // Update position data
    int newQuantity = position.getQuantity().toInt();
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();
    posViz->currentQuantity = newQuantity;

    // Update dynamic line with latest bar price
    if (m_latestBarIndex >= 0)
    {
        double currentPrice = m_latestBar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    qCDebug(ChartLog) << "Position updated:" << positionID << "qty:" << newQuantity;
}

void StockPriceChart::onPositionClosed(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        return;
    }

    QString positionID = position.getPositionID();
    auto it = m_positionVisualizations.find(positionID);

    if (it == m_positionVisualizations.end())
    {
        return;
    }

    PositionVisualization* posViz = it.value();

    // Calculate realized P&L from position data
    posViz->realizedPL = position.getTodaysProfitLoss().toDouble();
    posViz->currentQuantity = 0;

    // Finalize the position visualization
    finalizeClosedPosition(posViz);

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    qCDebug(ChartLog) << "Position closed:" << positionID << "P&L:" << posViz->realizedPL;
}

void StockPriceChart::setOrderVisualizationsVisible(bool visible)
{
    m_orderVisualizationsVisible = visible;
    updateOrderVisualizationsVisibility();
}
