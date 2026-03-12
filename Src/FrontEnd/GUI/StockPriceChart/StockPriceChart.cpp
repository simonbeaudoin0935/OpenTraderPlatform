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
#include "BarUtils.h"
#include "SQL/StockPriceChartQueries.h"
#include "BarCache.h"
#include "MainApp.h"
#include "Order.h"
#include "DBClient.h"
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

    // Create loading spinner (shown while a missing-bars API request is in flight)
    m_loadingSpinner = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_loadingSpinner);
    m_loadingSpinner->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_loadingSpinner->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_loadingSpinner->position->setCoords(0.01, 0.5); // Left edge, vertically centred
    m_loadingSpinner->setFont(QFont(font().family(), 13));
    m_loadingSpinner->setColor(QColor(200, 200, 200, 220));
    m_loadingSpinner->setLayer("overlay");
    m_loadingSpinner->setVisible(false);

    m_loadingSpinnerTimer = new QTimer(this);
    Q_CHECK_PTR(m_loadingSpinnerTimer);
    m_loadingSpinnerTimer->setInterval(80); // ~12 fps
    connect(m_loadingSpinnerTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                // Braille dot spinner — 10 frames, smooth circular motion
                static const std::array<const char*, 10> FRAMES = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                m_loadingSpinnerFrame = (m_loadingSpinnerFrame + 1) % static_cast<int>(FRAMES.size());
                m_loadingSpinner->setText(QString("%1 Loading…").arg(FRAMES[m_loadingSpinnerFrame]));
                m_customPlot->replot(QCustomPlot::rpQueuedReplot);
            });

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

                    // Use the toolbar's own ReplayState (GUI thread) to determine
                    // whether to resume or start fresh. Do NOT use isReplayPaused()
                    // which reads MainAlgo state cross-thread — it returns false when
                    // enterReplayModePaused is still queued but not yet executed,
                    // causing a double-enter race that fires the m_replayEngine==null ASSERT.
                    const ChartToolbar::ReplayState toolbarState = chartToolbar->getReplayState();
                    const bool shouldResume = (toolbarState == ChartToolbar::ReplayState::Paused ||
                                               toolbarState == ChartToolbar::ReplayState::PreloadingPaused);

                    if (shouldResume)
                    {
                        // Resume an existing (or preloading) session
                        MainApp::getInstance()->resumeReplayPlayback();
                    }
                    else if (date.isValid())
                    {
                        // Start new playback (no engine created yet)
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

    // When auto-timeframe is enabled, immediately check if we need to switch
    connect(chartToolbar,
            &ChartToolbar::autoTimeFrameChanged,
            this,
            [this](bool enabled)
            {
                if (enabled)
                    checkAutoTimeFrame();
            });

    // Load wheel zoom ratio from settings
    Q_CHECK_PTR(appStateSettings);
    qreal savedRatio = appStateSettings->value("Chart/WheelZoomRatio", 1.0).toReal();
    chartToolbar->setWheelRatio(savedRatio);
    wheelZoomRatio = savedRatio;

    // Restore timescale and auto-mode from AppState
    const bool autoEnabled = appStateSettings->value("Chart/AutoTimeFrame", false).toBool();
    chartToolbar->setAutoTimeFrameEnabled(autoEnabled);
    if (!autoEnabled)
    {
        const int savedTf = appStateSettings->value("Chart/TimeFrame", static_cast<int>(TimeFrame::ONE_MINUTE)).toInt();
        chartToolbar->setCurrentTimeFrame(static_cast<TimeFrame>(savedTf));
    }

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

void StockPriceChart::setDisplayTimeFrame(TimeFrame tf)
{
    if (!BarUtils::isIntradayTimeFrame(tf))
        return;

    m_displayTimeFrame = tf;
    const double w = BarUtils::minutesPerBar(tf) * ChartConstants::CANDLESTICK_BODY_WIDTH;
    m_candlesticks->setWidth(w);
    m_volumePos->setWidth(w);
    m_volumeNeg->setWidth(w);
}

void StockPriceChart::preserveCurrentRanges()
{
    m_preservedXRange = m_customPlot->xAxis->range();
    m_preservedYRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
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
    // Clear all bar data, index mappings and in-flight requests from the previous symbol.
    clearSymbol();
    clearOrderVisualizations();

    m_symbol = symbol;
    m_candlesticks->setName(symbol + " (Bars)");

    // Update symbol watermark
    m_symbolWatermark->setText(symbol);
    m_customPlot->replot();

    // Populate available replay days when symbol changes
    populateAvailableReplayDays();

    // Compute index 0 from current time and request initial historical bars
    initializeTimeAnchor();

    DEBUG << "Set chart symbol to" << symbol;
}

/**
 * @brief Computes m_index0Timestamp from clock time and sets up the chart.
 *
 * This is called on symbol selection — no live data needed.
 * Sets up the time ticker, view range, and requests initial historical bars.
 */
void StockPriceChart::initializeTimeAnchor()
{
    QDateTime now;
    if (MainApp::isInReplayMode())
    {
        // Replay mode: use the replay date + start time as "now"
        // MainApp::getCurrentAppTime() returns currentAppReplayTime in NY timezone
        now = MainApp::getCurrentAppTime();
    }
    else
    {
        now = QDateTime::currentDateTimeUtc();
    }

    m_index0Timestamp = ChartTimeUtils::computeIndex0Timestamp(now);

    DEBUG << "Time anchor set: index 0 =" << m_index0Timestamp.toString(Qt::ISODate);

    // Set up custom time ticker
    QSharedPointer<IndexToTimeTicker> indexToTimeTicker(new IndexToTimeTicker);
    indexToTimeTicker->setTimeFormat("hh:mm");
    indexToTimeTicker->setIndexToTimestampFunction([this](int index) { return this->getTimestampForIndex(index); });
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(true);
    m_customPlot->xAxis->setTicker(indexToTimeTicker);

    // Center view on index 0 with ~60 bars left, ~30 bars right (unless preserved)
    if (m_preservedXRange.has_value())
    {
        m_customPlot->xAxis->setRange(m_preservedXRange.value());
        m_preservedXRange.reset();
    }
    else
    {
        m_customPlot->xAxis->setRange(-60, 30);
    }

    // Start the current time line
    m_currentTimeLine->setVisible(true);
    m_timeLineTimer->start();
    updateCurrentTimeLine();

    // Defer initial bar request to next event loop iteration.
    // This is necessary because setSymbol() fires before onSelectDisplayedStock
    // reaches MainAlgo (queued cross-thread connection). The deferred call ensures
    // the StockInstrument is created before we request bars from its cache.
    QTimer::singleShot(0,
                       this,
                       [this]()
                       {
                           if (!m_index0Timestamp.isValid() || m_symbol.isEmpty())
                               return;

                           double minIndex = m_customPlot->xAxis->range().lower;
                           QDateTime requestTime = getTimestampForIndex(static_cast<int>(minIndex));
                           checkForMissingBars(requestTime, m_index0Timestamp);
                       });
}

/**
 * @brief Populates the replay day dropdown with available dates from cache.
 */
void StockPriceChart::populateAvailableReplayDays()
{
    // Scan ReplayData directory for date-named subdirectories containing .dbn.zst files
    QString replayBaseDir = DBClient::getReplayDataDir(QDate::currentDate());
    QDir base(replayBaseDir);
    base.cdUp(); // Go from ReplayData/YYYY-MM-DD to ReplayData/

    QList<QDate> availableDates;
    if (base.exists())
    {
        QStringList dateDirs = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& dirName: dateDirs)
        {
            QDate date = QDate::fromString(dirName, Qt::ISODate);
            if (!date.isValid())
                continue;

            QDir dateDir(base.absoluteFilePath(dirName));
            if (!dateDir.entryList({"*.dbn.zst"}, QDir::Files).isEmpty())
            {
                availableDates.append(date);
            }
        }

        std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());
    }

    chartToolbar->setAvailableReplayDays(availableDates);

    qCInfo(ChartLog) << "Found" << availableDates.size() << "available replay dates";
}
