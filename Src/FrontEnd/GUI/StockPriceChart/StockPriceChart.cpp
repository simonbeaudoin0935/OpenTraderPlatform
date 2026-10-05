#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "IndexToTimeTicker.h"
#include "Indicators/ChartIndicatorManager.h"
#include "Indicators/EmaIndicator.h"
#include "Indicators/MacdIndicator.h"
#include "Indicators/RsiIndicator.h"
#include "Indicators/VolumeIndicator.h"
#include "Indicators/VwapIndicator.h"
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

namespace
{
    const QColor MANUAL_CONFIRMATION_COLOR_ON(255, 40, 40, 230);
    const QColor MANUAL_CONFIRMATION_COLOR_OFF(255, 40, 40, 120);
    const QColor MANUAL_CONFIRMATION_BG_BRUSH(20, 0, 0, 140);
    const QColor MANUAL_CONFIRMATION_BORDER(255, 90, 90, 210);
    const QColor MANUAL_CONFIRMATION_MUTED_COLOR(255, 190, 190, 200);
    const QColor MANUAL_CONFIRMATION_MUTED_BG_BRUSH(20, 0, 0, 130);
    const QColor MANUAL_CONFIRMATION_MUTED_BORDER(255, 120, 120, 180);
    const QColor MANUAL_CONFIRMATION_CHART_BORDER_ON(255, 60, 60, 245);
    constexpr int MANUAL_CONFIRMATION_BORDER_WIDTH = 6;
    constexpr int MANUAL_CONFIRMATION_FLASH_INTERVAL_MS = 260;
    constexpr int LEVEL2_DEPTH_ROLLING_MAX_WINDOW_MS = 4000;
    constexpr double LEVEL2_DEPTH_MAX_WIDTH_FRACTION = 0.18;
    constexpr double LEVEL2_DEPTH_MIN_WIDTH_INDEX_UNITS = 4.0;
    constexpr double LEVEL2_DEPTH_MIN_DRAWN_WIDTH_INDEX_UNITS = 0.4;
    const QColor LEVEL2_DEPTH_BID_COLOR(0, 180, 0, 175);
    const QColor LEVEL2_DEPTH_ASK_COLOR(200, 0, 0, 175);

    QColor parseColorSettingWithFallback(const QVariant& settingValue, const QColor& fallback)
    {
        const QColor parsedColor(settingValue.toString());
        return parsedColor.isValid() ? parsedColor : fallback;
    }

    const char* emaShowKey(const int slot)
    {
        switch (slot)
        {
        case 0:
            return ChartIndicatorConstants::SETTINGS_KEY_SHOW_EMA1;
        case 1:
            return ChartIndicatorConstants::SETTINGS_KEY_SHOW_EMA2;
        case 2:
            return ChartIndicatorConstants::SETTINGS_KEY_SHOW_EMA3;
        default:
            return ChartIndicatorConstants::SETTINGS_KEY_SHOW_EMA1;
        }
    }

    bool defaultShowEmaForSlot(const int slot)
    {
        switch (slot)
        {
        case 0:
            return ChartIndicatorConstants::DEFAULT_SHOW_EMA1;
        case 1:
            return ChartIndicatorConstants::DEFAULT_SHOW_EMA2;
        case 2:
            return ChartIndicatorConstants::DEFAULT_SHOW_EMA3;
        default:
            return ChartIndicatorConstants::DEFAULT_SHOW_EMA1;
        }
    }

    const char* emaPeriodKey(const int slot)
    {
        switch (slot)
        {
        case 0:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA1_PERIOD;
        case 1:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA2_PERIOD;
        case 2:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA3_PERIOD;
        default:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA1_PERIOD;
        }
    }

    int defaultEmaPeriodForSlot(const int slot)
    {
        switch (slot)
        {
        case 0:
            return ChartIndicatorConstants::DEFAULT_EMA1_PERIOD;
        case 1:
            return ChartIndicatorConstants::DEFAULT_EMA2_PERIOD;
        case 2:
            return ChartIndicatorConstants::DEFAULT_EMA3_PERIOD;
        default:
            return ChartIndicatorConstants::DEFAULT_EMA1_PERIOD;
        }
    }

    const char* emaColorKey(const int slot)
    {
        switch (slot)
        {
        case 0:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA1_COLOR;
        case 1:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA2_COLOR;
        case 2:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA3_COLOR;
        default:
            return ChartIndicatorConstants::SETTINGS_KEY_EMA1_COLOR;
        }
    }

    QColor defaultEmaColorForSlot(const int slot)
    {
        switch (slot)
        {
        case 0:
            return parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_EMA1_COLOR, QColor(243, 198, 35));
        case 1:
            return parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_EMA2_COLOR, QColor(41, 182, 246));
        case 2:
            return parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_EMA3_COLOR, QColor(240, 98, 146));
        default:
            return parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_EMA1_COLOR, QColor(243, 198, 35));
        }
    }

    QColor defaultRsiColor()
    {
        return parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_RSI_COLOR, QColor(179, 136, 255));
    }

} // namespace

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

    // Setup axes - right axis for price, left axis reserved for blended volume scale
    m_customPlot->xAxis->setLabel("");
    m_customPlot->yAxis->setLabel("");
    m_customPlot->yAxis->setVisible(false);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setVisible(true);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setLabel("");

    // Apply dark theme
    m_customPlot->setBackground(QBrush(QColor(75, 75, 80)));
    m_customPlot->xAxis->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setBasePen(QPen(QColor(200, 200, 200)));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setBasePen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setTickPen(QPen(QColor(200, 200, 200)));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->yAxis->setSubTickPen(Qt::NoPen);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setSubTickPen(QPen(QColor(220, 220, 220)));
    m_customPlot->xAxis->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->yAxis->setTickLabelColor(QColor(210, 210, 210));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->setLabelColor(QColor(220, 220, 220));
    m_customPlot->yAxis->setLabelColor(QColor(210, 210, 210));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setLabelColor(QColor(220, 220, 220));
    m_customPlot->xAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin
    m_customPlot->yAxis->grid()->setVisible(false);
    m_customPlot->yAxis->setNumberFormat("gb");
    m_customPlot->yAxis->setNumberPrecision(3);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setVisible(true);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin

    // Keep a collapsed legacy volume axis rect so existing x-range sync wiring remains stable.
    m_volumeAxisRect = new QCPAxisRect(m_customPlot);
    Q_CHECK_PTR(m_volumeAxisRect);
    m_customPlot->plotLayout()->addElement(1, 0, m_volumeAxisRect);
    m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_volumeAxisRect->setVisible(false);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");

    // Bring bottom and main axis rect closer together
    m_customPlot->plotLayout()->setRowSpacing(0);
    m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
    m_volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));

    // Keep the legacy pane fully hidden.
    m_volumeAxisRect->setBackground(QBrush(QColor(75, 75, 80)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setBasePen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
    m_volumeAxisRect->axis(QCPAxis::atRight)->setVisible(false);
    m_volumeAxisRect->axis(QCPAxis::atRight)->setBasePen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickPen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atRight)->setTickPen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220, 0));
    m_volumeAxisRect->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220, 0));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setVisible(false);
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setPen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atRight)->grid()->setZeroLinePen(Qt::NoPen); // Disable zero-line at origin

    // Create third axis rect for MACD chart (collapsible)
    m_macdAxisRect = new QCPAxisRect(m_customPlot);
    Q_CHECK_PTR(m_macdAxisRect);
    m_customPlot->plotLayout()->addElement(2, 0, m_macdAxisRect);
    m_macdAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_macdAxisRect->setVisible(false);
    m_macdAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_macdAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    m_macdAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
    m_macdAxisRect->setMargins(QMargins(0, 0, 0, 0));

    m_macdAxisRect->setBackground(QBrush(QColor(75, 75, 80)));
    m_macdAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(QColor(220, 220, 220)));
    m_macdAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
    m_macdAxisRect->axis(QCPAxis::atRight)->setVisible(true);
    m_macdAxisRect->axis(QCPAxis::atRight)->setBasePen(QPen(QColor(220, 220, 220)));
    m_macdAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(QColor(220, 220, 220)));
    m_macdAxisRect->axis(QCPAxis::atRight)->setTickPen(QPen(QColor(220, 220, 220)));
    m_macdAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220));
    m_macdAxisRect->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220));
    m_macdAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_macdAxisRect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen);
    m_macdAxisRect->axis(QCPAxis::atRight)->grid()->setVisible(true);
    m_macdAxisRect->axis(QCPAxis::atRight)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_macdAxisRect->axis(QCPAxis::atRight)->grid()->setZeroLinePen(QPen(QColor(110, 110, 110), 1, Qt::DashLine));

    // Create fourth axis rect for RSI chart (collapsible)
    m_rsiAxisRect = new QCPAxisRect(m_customPlot);
    Q_CHECK_PTR(m_rsiAxisRect);
    m_customPlot->plotLayout()->addElement(3, 0, m_rsiAxisRect);
    m_rsiAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_rsiAxisRect->setVisible(false);
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_rsiAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    m_rsiAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
    m_rsiAxisRect->setMargins(QMargins(0, 0, 0, 0));

    m_rsiAxisRect->setBackground(QBrush(QColor(75, 75, 80)));
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(QColor(220, 220, 220)));
    m_rsiAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
    m_rsiAxisRect->axis(QCPAxis::atRight)->setVisible(true);
    m_rsiAxisRect->axis(QCPAxis::atRight)->setBasePen(QPen(QColor(220, 220, 220)));
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(QColor(220, 220, 220)));
    m_rsiAxisRect->axis(QCPAxis::atRight)->setTickPen(QPen(QColor(220, 220, 220)));
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220));
    m_rsiAxisRect->axis(QCPAxis::atRight)->setTickLabelColor(QColor(220, 220, 220));
    m_rsiAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_rsiAxisRect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen);
    m_rsiAxisRect->axis(QCPAxis::atRight)->grid()->setVisible(true);
    m_rsiAxisRect->axis(QCPAxis::atRight)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_rsiAxisRect->axis(QCPAxis::atRight)->grid()->setZeroLinePen(QPen(QColor(110, 110, 110), 1, Qt::DashLine));

    // Create fifth axis rect for strategy status panel (collapsible)
    m_statusAxisRect = new QCPAxisRect(m_customPlot);
    Q_CHECK_PTR(m_statusAxisRect);
    m_customPlot->plotLayout()->addElement(4, 0, m_statusAxisRect);
    m_statusAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_statusAxisRect->setVisible(false);
    m_statusAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_statusAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    m_statusAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight);
    m_statusAxisRect->setMargins(QMargins(0, 0, 0, 0));

    m_statusAxisRect->setBackground(QBrush(QColor(52, 52, 58)));
    m_statusAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(QColor(220, 220, 220)));
    m_statusAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
    m_statusAxisRect->axis(QCPAxis::atRight)->setVisible(false);
    m_statusAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(QColor(220, 220, 220)));
    m_statusAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(QColor(220, 220, 220));
    m_statusAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    m_statusAxisRect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen);

    m_customPlot->setAutoAddPlottableToLegend(false);
    if (m_customPlot->layer(QStringLiteral("volume-bars")) == nullptr)
    {
        const bool layerAdded =
            m_customPlot->addLayer(QStringLiteral("volume-bars"), m_customPlot->layer("main"), QCustomPlot::limBelow);
        ASSUME_TRUE(layerAdded);
    }
    if (m_customPlot->layer(QStringLiteral("overlay-bbo")) == nullptr)
    {
        const bool layerAdded = m_customPlot->addLayer(QStringLiteral("overlay-bbo"),
                                                       m_customPlot->layer("overlay"),
                                                       QCustomPlot::limAbove);
        ASSUME_TRUE(layerAdded);
    }

    // Interconnect x axis ranges of main and bottom axis rects (bidirectional for horizontal zoom)
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_volumeAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_volumeAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_macdAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_macdAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_rsiAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_rsiAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_statusAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    connect(m_statusAxisRect->axis(QCPAxis::atBottom),
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            m_customPlot->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));

    // Configure axes of both main and bottom axis rect
    // Note: Custom time ticker is set up later when first bar is received,
    // because it needs indexToBar to be populated to convert indices to timestamps.
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setBasePen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickPen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setSubTickPen(Qt::NoPen);
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicks(false);
    m_macdAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_macdAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_statusAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_statusAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    m_customPlot->xAxis->setTickLabelRotation(15);
    m_customPlot->xAxis->setTicks(true);
    m_customPlot->xAxis->setTickLabels(true);

    // Make axis rects' left side line up
    QCPMarginGroup* group = new QCPMarginGroup(m_customPlot);
    m_customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_volumeAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_macdAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_rsiAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_statusAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);

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

    // Create symbol watermark (bottom-right, behind candlesticks)
    m_symbolWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_symbolWatermark);
    m_symbolWatermark->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_symbolWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_symbolWatermark->position->setCoords(0.985, 0.985); // Bottom-right of chart
    m_symbolWatermark->setText("");
    m_symbolWatermark->setFont(QFont(font().family(), 52, QFont::Black));
    m_symbolWatermark->setColor(QColor(255, 255, 255, 110)); // Stronger contrast
    m_symbolWatermark->setLayer("background");               // Draw behind candlesticks

    // Create date watermark (bottom-left, behind candlesticks)
    m_dateWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_dateWatermark);
    m_dateWatermark->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    m_dateWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_dateWatermark->position->setCoords(0.015, 0.985);
    m_dateWatermark->setText("");
    m_dateWatermark->setFont(QFont(font().family(), 22, QFont::Bold));
    m_dateWatermark->setColor(QColor(255, 255, 255, 85));
    m_dateWatermark->setLayer("background");

    // Create timescale watermark (top-right, behind candlesticks)
    m_timeFrameWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_timeFrameWatermark);
    m_timeFrameWatermark->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
    m_timeFrameWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_timeFrameWatermark->position->setCoords(0.985, 0.02);
    m_timeFrameWatermark->setText(timeFrameToString(m_displayTimeFrame));
    m_timeFrameWatermark->setFont(QFont(font().family(), 22, QFont::Bold));
    m_timeFrameWatermark->setColor(QColor(255, 255, 255, 85));
    m_timeFrameWatermark->setLayer("background");

    m_manualConfirmationHeader = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_manualConfirmationHeader);
    m_manualConfirmationHeader->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_manualConfirmationHeader->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationHeader->position->setCoords(0.5, 0.015);
    m_manualConfirmationHeader->setText("");
    m_manualConfirmationHeader->setFont(QFont(font().family(), 16, QFont::Bold));
    m_manualConfirmationHeader->setColor(MANUAL_CONFIRMATION_COLOR_OFF);
    m_manualConfirmationHeader->setPadding(QMargins(10, 4, 10, 4));
    m_manualConfirmationHeader->setBrush(QBrush(MANUAL_CONFIRMATION_BG_BRUSH));
    m_manualConfirmationHeader->setPen(QPen(MANUAL_CONFIRMATION_BORDER, 1));
    m_manualConfirmationHeader->setLayer("overlay");
    m_manualConfirmationHeader->setVisible(false);

    m_manualConfirmationWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_manualConfirmationWatermark);
    m_manualConfirmationWatermark->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    m_manualConfirmationWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationWatermark->position->setCoords(0.5, 0.985);
    m_manualConfirmationWatermark->setText("");
    m_manualConfirmationWatermark->setFont(QFont(font().family(), 15, QFont::Bold));
    m_manualConfirmationWatermark->setColor(MANUAL_CONFIRMATION_COLOR_OFF);
    m_manualConfirmationWatermark->setPadding(QMargins(12, 4, 12, 4));
    m_manualConfirmationWatermark->setBrush(QBrush(MANUAL_CONFIRMATION_BG_BRUSH));
    m_manualConfirmationWatermark->setPen(QPen(MANUAL_CONFIRMATION_BORDER, 1));
    m_manualConfirmationWatermark->setLayer("overlay");
    m_manualConfirmationWatermark->setVisible(false);

    m_manualConfirmationMutedWatermark = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_manualConfirmationMutedWatermark);
    m_manualConfirmationMutedWatermark->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    m_manualConfirmationMutedWatermark->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationMutedWatermark->position->setCoords(0.5, 0.945);
    m_manualConfirmationMutedWatermark->setText("Muted");
    m_manualConfirmationMutedWatermark->setFont(QFont(font().family(), 11, QFont::Bold));
    m_manualConfirmationMutedWatermark->setColor(MANUAL_CONFIRMATION_MUTED_COLOR);
    m_manualConfirmationMutedWatermark->setPadding(QMargins(8, 3, 8, 3));
    m_manualConfirmationMutedWatermark->setBrush(QBrush(MANUAL_CONFIRMATION_MUTED_BG_BRUSH));
    m_manualConfirmationMutedWatermark->setPen(QPen(MANUAL_CONFIRMATION_MUTED_BORDER, 1));
    m_manualConfirmationMutedWatermark->setLayer("overlay");
    m_manualConfirmationMutedWatermark->setVisible(false);

    m_manualConfirmationMainBorder = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(m_manualConfirmationMainBorder);
    m_manualConfirmationMainBorder->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationMainBorder->topLeft->setAxisRect(m_customPlot->axisRect());
    m_manualConfirmationMainBorder->topLeft->setCoords(0.003, 0.003);
    m_manualConfirmationMainBorder->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationMainBorder->bottomRight->setAxisRect(m_customPlot->axisRect());
    m_manualConfirmationMainBorder->bottomRight->setCoords(0.997, 0.997);
    m_manualConfirmationMainBorder->setClipAxisRect(m_customPlot->axisRect());
    m_manualConfirmationMainBorder->setBrush(Qt::NoBrush);
    m_manualConfirmationMainBorder->setPen(QPen(MANUAL_CONFIRMATION_CHART_BORDER_ON, MANUAL_CONFIRMATION_BORDER_WIDTH));
    m_manualConfirmationMainBorder->setLayer("overlay");
    m_manualConfirmationMainBorder->setVisible(false);

    m_manualConfirmationVolumeBorder = new QCPItemRect(m_customPlot);
    Q_CHECK_PTR(m_manualConfirmationVolumeBorder);
    m_manualConfirmationVolumeBorder->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationVolumeBorder->topLeft->setAxisRect(m_volumeAxisRect);
    m_manualConfirmationVolumeBorder->topLeft->setCoords(0.003, 0.003);
    m_manualConfirmationVolumeBorder->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
    m_manualConfirmationVolumeBorder->bottomRight->setAxisRect(m_volumeAxisRect);
    m_manualConfirmationVolumeBorder->bottomRight->setCoords(0.997, 0.997);
    m_manualConfirmationVolumeBorder->setClipAxisRect(m_volumeAxisRect);
    m_manualConfirmationVolumeBorder->setBrush(Qt::NoBrush);
    m_manualConfirmationVolumeBorder->setPen(
        QPen(MANUAL_CONFIRMATION_CHART_BORDER_ON, MANUAL_CONFIRMATION_BORDER_WIDTH));
    m_manualConfirmationVolumeBorder->setLayer("overlay");
    m_manualConfirmationVolumeBorder->setVisible(false);

    m_manualConfirmationFlashTimer = new QTimer(this);
    Q_CHECK_PTR(m_manualConfirmationFlashTimer);
    m_manualConfirmationFlashTimer->setInterval(MANUAL_CONFIRMATION_FLASH_INTERVAL_MS);
    connect(m_manualConfirmationFlashTimer,
            &QTimer::timeout,
            this,
            &StockPriceChart::onManualConfirmationFlashTick,
            Qt::UniqueConnection);

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

    // Create BBO overlay lines and labels (hidden by default)
    m_bestBidLine = new QCPItemLine(m_customPlot);
    Q_CHECK_PTR(m_bestBidLine);
    m_bestBidLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestBidLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestBidLine->setPen(QPen(QColor(80, 220, 120), 2, Qt::SolidLine));
    m_bestBidLine->setLayer("overlay-bbo");
    m_bestBidLine->setVisible(false);

    m_bestAskLine = new QCPItemLine(m_customPlot);
    Q_CHECK_PTR(m_bestAskLine);
    m_bestAskLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestAskLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestAskLine->setPen(QPen(QColor(255, 110, 110), 2, Qt::SolidLine));
    m_bestAskLine->setLayer("overlay-bbo");
    m_bestAskLine->setVisible(false);

    m_bestBidLabel = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_bestBidLabel);
    m_bestBidLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_bestBidLabel->position->setType(QCPItemPosition::ptPlotCoords);
    m_bestBidLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestBidLabel->setFont(QFont(font().family(), 9, QFont::Bold));
    m_bestBidLabel->setColor(QColor(100, 255, 140));
    m_bestBidLabel->setBrush(QBrush(QColor(0, 0, 0, 140)));
    m_bestBidLabel->setPadding(QMargins(4, 1, 4, 1));
    m_bestBidLabel->setLayer("overlay-bbo");
    m_bestBidLabel->setVisible(false);

    m_bestAskLabel = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_bestAskLabel);
    m_bestAskLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_bestAskLabel->position->setType(QCPItemPosition::ptPlotCoords);
    m_bestAskLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_bestAskLabel->setFont(QFont(font().family(), 9, QFont::Bold));
    m_bestAskLabel->setColor(QColor(255, 140, 140));
    m_bestAskLabel->setBrush(QBrush(QColor(0, 0, 0, 140)));
    m_bestAskLabel->setPadding(QMargins(4, 1, 4, 1));
    m_bestAskLabel->setLayer("overlay-bbo");
    m_bestAskLabel->setVisible(false);

    for (QCPItemLine*& bidLine: m_depthBidLines)
    {
        bidLine = new QCPItemLine(m_customPlot);
        Q_CHECK_PTR(bidLine);
        bidLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        bidLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        bidLine->setPen(QPen(LEVEL2_DEPTH_BID_COLOR, 4.0, Qt::SolidLine, Qt::RoundCap));
        bidLine->setLayer("overlay");
        bidLine->setVisible(false);
    }

    for (QCPItemLine*& askLine: m_depthAskLines)
    {
        askLine = new QCPItemLine(m_customPlot);
        Q_CHECK_PTR(askLine);
        askLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        askLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        askLine->setPen(QPen(LEVEL2_DEPTH_ASK_COLOR, 4.0, Qt::SolidLine, Qt::RoundCap));
        askLine->setLayer("overlay");
        askLine->setVisible(false);
    }

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

    m_bracketWheelModeBadge = new QCPItemText(m_customPlot);
    Q_CHECK_PTR(m_bracketWheelModeBadge);
    m_bracketWheelModeBadge->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
    m_bracketWheelModeBadge->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_bracketWheelModeBadge->position->setAxisRect(m_customPlot->axisRect());
    m_bracketWheelModeBadge->position->setCoords(0.985, 0.085);
    m_bracketWheelModeBadge->setFont(QFont(font().family(), 10, QFont::Bold));
    m_bracketWheelModeBadge->setPadding(QMargins(8, 3, 8, 3));
    m_bracketWheelModeBadge->setBrush(QBrush(QColor(10, 10, 10, 180)));
    m_bracketWheelModeBadge->setPen(QPen(QColor(220, 220, 220, 170), 1));
    m_bracketWheelModeBadge->setLayer("overlay");
    m_bracketWheelModeBadge->setVisible(false);

    m_strategyStatusLabel = new QLabel(m_customPlot);
    Q_CHECK_PTR(m_strategyStatusLabel);
    m_strategyStatusLabel->setTextFormat(Qt::RichText);
    m_strategyStatusLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_strategyStatusLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_strategyStatusLabel->setFont(QFont(font().family(), 10, QFont::Bold));
    m_strategyStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_strategyStatusLabel->setWordWrap(true);
    m_strategyStatusLabel->setStyleSheet("QLabel {"
                                         "color: rgba(210, 210, 210, 235);"
                                         "background-color: rgba(10, 10, 10, 125);"
                                         "border: 1px solid rgba(180, 180, 180, 95);"
                                         "border-radius: 4px;"
                                         "padding: 6px 8px;"
                                         "}");
    m_strategyStatusLabel->setVisible(false);
    connect(m_customPlot,
            &QCustomPlot::afterReplot,
            this,
            [this]()
            {
                if (m_strategyStatusLabel != nullptr && m_strategyStatusLabel->isVisible())
                {
                    repositionStrategyStatusVisual();
                }
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
    m_macdAxisRect->setRangeDrag(Qt::Horizontal);
    m_macdAxisRect->setRangeZoom(Qt::Horizontal);
    m_rsiAxisRect->setRangeDrag(Qt::Horizontal);
    m_rsiAxisRect->setRangeZoom(Qt::Horizontal);
    m_statusAxisRect->setRangeDrag(Qt::Horizontal);
    m_statusAxisRect->setRangeZoom(Qt::Horizontal);

    // Set initial ranges
    m_customPlot->xAxis->setRange(-3, 3);
    m_customPlot->yAxis->setRange(0, 1000);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(0, 100);

    m_indicatorManager = std::make_unique<ChartIndicatorManager>();
    auto volumeIndicator = std::make_unique<VolumeIndicator>(m_customPlot, m_customPlot->xAxis, m_customPlot->yAxis);
    m_volumeIndicator = volumeIndicator.get();
    m_volumeIndicator->setBarWidth(ChartConstants::CANDLESTICK_BODY_WIDTH);
    m_indicatorManager->addIndicator(std::move(volumeIndicator));

    auto vwapIndicator = std::make_unique<VwapIndicator>(m_customPlot,
                                                         m_customPlot->xAxis,
                                                         m_customPlot->axisRect()->axis(QCPAxis::atRight));
    m_vwapIndicator = vwapIndicator.get();
    m_indicatorManager->addIndicator(std::move(vwapIndicator));

    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        auto emaIndicator = std::make_unique<EmaIndicator>(QStringLiteral("ema%1").arg(slot + 1),
                                                           m_customPlot,
                                                           m_customPlot->xAxis,
                                                           m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_emaIndicators[slot] = emaIndicator.get();
        m_indicatorManager->addIndicator(std::move(emaIndicator));
    }

    auto macdIndicator = std::make_unique<MacdIndicator>(m_customPlot,
                                                         m_macdAxisRect->axis(QCPAxis::atBottom),
                                                         m_macdAxisRect->axis(QCPAxis::atRight));
    m_macdIndicator = macdIndicator.get();
    m_indicatorManager->addIndicator(std::move(macdIndicator));

    auto rsiIndicator = std::make_unique<RsiIndicator>(m_customPlot,
                                                       m_rsiAxisRect->axis(QCPAxis::atBottom),
                                                       m_rsiAxisRect->axis(QCPAxis::atRight));
    m_rsiIndicator = rsiIndicator.get();
    m_indicatorManager->addIndicator(std::move(rsiIndicator));

    // Create layout and add widgets
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create and add the timeframe selector at the top
    chartToolbar = new ChartToolbar(this);
    Q_CHECK_PTR(chartToolbar);
    chartToolbar->setTenSecondTimeFrameEnabled(MainApp::isInReplayMode());
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
    connect(chartToolbar, &ChartToolbar::volumeSettingsChanged, this, &StockPriceChart::onVolumeSettingsChanged);
    connect(chartToolbar,
            &ChartToolbar::orderVisualizationsVisibilityChanged,
            this,
            &StockPriceChart::onOrderVisualizationsVisibilityChanged);
    connect(chartToolbar,
            &ChartToolbar::bboOverlayVisibilityChanged,
            this,
            &StockPriceChart::onBboOverlayVisibilityChanged);
    connect(chartToolbar,
            &ChartToolbar::level2DepthOverlayVisibilityChanged,
            this,
            &StockPriceChart::onLevel2DepthOverlayVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::vwapVisibilityChanged, this, &StockPriceChart::onVwapVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::vwapSettingsChanged, this, &StockPriceChart::onVwapSettingsChanged);
    connect(chartToolbar, &ChartToolbar::emaVisibilityChanged, this, &StockPriceChart::onEmaVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::emaSettingsChanged, this, &StockPriceChart::onEmaSettingsChanged);
    connect(chartToolbar, &ChartToolbar::macdVisibilityChanged, this, &StockPriceChart::onMacdVisibilityChanged);
    connect(chartToolbar,
            &ChartToolbar::strategyStatusPanelVisibilityChanged,
            this,
            &StockPriceChart::onStrategyStatusPanelVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::macdSettingsChanged, this, &StockPriceChart::onMacdSettingsChanged);
    connect(chartToolbar, &ChartToolbar::rsiVisibilityChanged, this, &StockPriceChart::onRsiVisibilityChanged);
    connect(chartToolbar, &ChartToolbar::rsiSettingsChanged, this, &StockPriceChart::onRsiSettingsChanged);
    connect(chartToolbar,
            &ChartToolbar::sessionBackgroundColorsChanged,
            this,
            &StockPriceChart::onSessionBackgroundColorsChanged);
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

    const QColor defaultEarlyPreMarketColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_EARLY_PRE_MARKET_COLOR,
                                      QColor(255, 165, 0, 90));
    const QColor defaultPreMarketColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_PRE_MARKET_COLOR, QColor(255, 165, 0, 180));
    const QColor defaultAfterHoursColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_AFTER_HOURS_COLOR, QColor(138, 43, 226, 180));

    m_earlyPreMarketBackgroundColor = parseColorSettingWithFallback(
        appStateSettings->value(ChartBackgroundConstants::SETTINGS_KEY_EARLY_PRE_MARKET_COLOR,
                                defaultEarlyPreMarketColor.name(QColor::HexArgb)),
        defaultEarlyPreMarketColor);
    m_preMarketBackgroundColor =
        parseColorSettingWithFallback(appStateSettings->value(ChartBackgroundConstants::SETTINGS_KEY_PRE_MARKET_COLOR,
                                                              defaultPreMarketColor.name(QColor::HexArgb)),
                                      defaultPreMarketColor);
    m_afterHoursBackgroundColor =
        parseColorSettingWithFallback(appStateSettings->value(ChartBackgroundConstants::SETTINGS_KEY_AFTER_HOURS_COLOR,
                                                              defaultAfterHoursColor.name(QColor::HexArgb)),
                                      defaultAfterHoursColor);
    chartToolbar->setSessionBackgroundColors(m_earlyPreMarketBackgroundColor,
                                             m_preMarketBackgroundColor,
                                             m_afterHoursBackgroundColor);

    qreal savedRatio = appStateSettings->value("Chart/WheelZoomRatio", 1.0).toReal();
    chartToolbar->setWheelRatio(savedRatio);
    wheelZoomRatio = savedRatio;

    const bool showBbo = appStateSettings->value("Chart/ShowBbo", false).toBool();
    const bool showLevel2Depth = appStateSettings
                                     ->value(ChartIndicatorConstants::SETTINGS_KEY_SHOW_LEVEL2_DEPTH_OVERLAY,
                                             ChartIndicatorConstants::DEFAULT_SHOW_LEVEL2_DEPTH_OVERLAY)
                                     .toBool();
    chartToolbar->setBboOverlayVisible(showBbo);
    chartToolbar->setLevel2DepthOverlayVisible(showLevel2Depth);
    m_showBboOverlay = chartToolbar->isBboOverlayVisible();
    m_showLevel2DepthOverlay = chartToolbar->isLevel2DepthOverlayVisible();
    if (m_showBboOverlay != showBbo || m_showLevel2DepthOverlay != showLevel2Depth)
    {
        appStateSettings->setValue("Chart/ShowBbo", m_showBboOverlay);
        appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_LEVEL2_DEPTH_OVERLAY,
                                   m_showLevel2DepthOverlay);
        appStateSettings->sync();
    }

    ASSUME_TRUE(m_volumeIndicator != nullptr);
    m_volumeIndicator->setVisible(chartToolbar->isVolumeChartVisible());

    const bool volumeAutoScaleEnabled = appStateSettings
                                            ->value(ChartIndicatorConstants::SETTINGS_KEY_VOLUME_AUTO_SCALE_ENABLED,
                                                    ChartIndicatorConstants::DEFAULT_VOLUME_AUTO_SCALE_ENABLED)
                                            .toBool();
    const int volumeAutoScaleMode = appStateSettings
                                        ->value(ChartIndicatorConstants::SETTINGS_KEY_VOLUME_AUTO_SCALE_MODE,
                                                ChartIndicatorConstants::DEFAULT_VOLUME_AUTO_SCALE_MODE)
                                        .toInt();
    chartToolbar->setVolumeSettings(volumeAutoScaleEnabled, volumeAutoScaleMode);
    onVolumeSettingsChanged(chartToolbar->isVolumeAutoScaleEnabled(), chartToolbar->getVolumeAutoScaleMode());

    const int vwapSourceMode = appStateSettings
                                   ->value(ChartIndicatorConstants::SETTINGS_KEY_VWAP_SOURCE_MODE,
                                           ChartIndicatorConstants::DEFAULT_VWAP_SOURCE_MODE)
                                   .toInt();
    chartToolbar->setVwapSourceMode(vwapSourceMode);

    const QString vwapResetTimeString = appStateSettings
                                            ->value(ChartIndicatorConstants::SETTINGS_KEY_VWAP_SESSION_RESET_TIME,
                                                    ChartIndicatorConstants::DEFAULT_VWAP_SESSION_RESET_TIME)
                                            .toString();
    QTime vwapResetTime = QTime::fromString(vwapResetTimeString, "HH:mm");
    if (!vwapResetTime.isValid())
    {
        vwapResetTime = QTime(4, 0);
    }
    chartToolbar->setVwapSessionResetTime(vwapResetTime);

    const QColor vwapColor = parseColorSettingWithFallback(
        appStateSettings->value(ChartIndicatorConstants::SETTINGS_KEY_VWAP_COLOR,
                                ChartIndicatorConstants::DEFAULT_VWAP_COLOR),
        parseColorSettingWithFallback(ChartIndicatorConstants::DEFAULT_VWAP_COLOR, QColor(0, 220, 220)));
    chartToolbar->setVwapLineColor(vwapColor);
    onVwapSettingsChanged(chartToolbar->getVwapSourceMode(),
                          chartToolbar->getVwapSessionResetTime(),
                          chartToolbar->getVwapLineColor());

    const bool showVwap =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_SHOW_VWAP, ChartIndicatorConstants::DEFAULT_SHOW_VWAP)
            .toBool();
    chartToolbar->setVwapVisible(showVwap);

    for (int slot = 0; slot < EMA_SLOT_COUNT; ++slot)
    {
        const bool showEma = appStateSettings->value(emaShowKey(slot), defaultShowEmaForSlot(slot)).toBool();
        const int emaPeriod = appStateSettings->value(emaPeriodKey(slot), defaultEmaPeriodForSlot(slot)).toInt();
        const QColor emaColor =
            parseColorSettingWithFallback(appStateSettings->value(emaColorKey(slot), defaultEmaColorForSlot(slot)),
                                          defaultEmaColorForSlot(slot));

        chartToolbar->setEmaSettings(slot, emaPeriod, emaColor);
        onEmaSettingsChanged(slot, chartToolbar->getEmaPeriod(slot), chartToolbar->getEmaColor(slot));
        chartToolbar->setEmaVisible(slot, showEma);
        onEmaVisibilityChanged(slot, showEma);
    }

    const int macdFastLength = appStateSettings
                                   ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_FAST_LENGTH,
                                           ChartIndicatorConstants::DEFAULT_MACD_FAST_LENGTH)
                                   .toInt();
    const int macdSlowLength = appStateSettings
                                   ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_SLOW_LENGTH,
                                           ChartIndicatorConstants::DEFAULT_MACD_SLOW_LENGTH)
                                   .toInt();
    const int macdSignalLength = appStateSettings
                                     ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_SIGNAL_LENGTH,
                                             ChartIndicatorConstants::DEFAULT_MACD_SIGNAL_LENGTH)
                                     .toInt();
    const int macdMaType =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_MA_TYPE, ChartIndicatorConstants::DEFAULT_MACD_MA_TYPE)
            .toInt();
    const int macdSignalMaType = appStateSettings
                                     ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_SIGNAL_MA_TYPE,
                                             ChartIndicatorConstants::DEFAULT_MACD_SIGNAL_MA_TYPE)
                                     .toInt();
    const bool macdShowHistogram = appStateSettings
                                       ->value(ChartIndicatorConstants::SETTINGS_KEY_MACD_SHOW_HISTOGRAM,
                                               ChartIndicatorConstants::DEFAULT_MACD_SHOW_HISTOGRAM)
                                       .toBool();
    chartToolbar->setMacdSettings(macdFastLength,
                                  macdSlowLength,
                                  macdSignalLength,
                                  macdMaType,
                                  macdSignalMaType,
                                  macdShowHistogram);
    onMacdSettingsChanged(chartToolbar->getMacdFastLength(),
                          chartToolbar->getMacdSlowLength(),
                          chartToolbar->getMacdSignalLength(),
                          chartToolbar->getMacdMaType(),
                          chartToolbar->getMacdSignalMaType(),
                          chartToolbar->isMacdHistogramVisible());

    const bool showMacd =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_SHOW_MACD, ChartIndicatorConstants::DEFAULT_SHOW_MACD)
            .toBool();
    chartToolbar->setMacdVisible(showMacd);

    const int rsiPeriod =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_RSI_PERIOD, ChartIndicatorConstants::DEFAULT_RSI_PERIOD)
            .toInt();
    const int rsiOverbought = appStateSettings
                                  ->value(ChartIndicatorConstants::SETTINGS_KEY_RSI_OVERBOUGHT,
                                          ChartIndicatorConstants::DEFAULT_RSI_OVERBOUGHT)
                                  .toInt();
    const int rsiOversold =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_RSI_OVERSOLD, ChartIndicatorConstants::DEFAULT_RSI_OVERSOLD)
            .toInt();
    const QColor rsiColor =
        parseColorSettingWithFallback(appStateSettings->value(ChartIndicatorConstants::SETTINGS_KEY_RSI_COLOR,
                                                              ChartIndicatorConstants::DEFAULT_RSI_COLOR),
                                      defaultRsiColor());
    chartToolbar->setRsiSettings(rsiPeriod, rsiOverbought, rsiOversold, rsiColor);
    onRsiSettingsChanged(chartToolbar->getRsiPeriod(),
                         chartToolbar->getRsiOverboughtLevel(),
                         chartToolbar->getRsiOversoldLevel(),
                         chartToolbar->getRsiColor());

    const bool showRsi =
        appStateSettings
            ->value(ChartIndicatorConstants::SETTINGS_KEY_SHOW_RSI, ChartIndicatorConstants::DEFAULT_SHOW_RSI)
            .toBool();
    chartToolbar->setRsiVisible(showRsi);
    onRsiVisibilityChanged(showRsi);

    const bool showStrategyStatusPanel = appStateSettings
                                             ->value(ChartIndicatorConstants::SETTINGS_KEY_SHOW_STRATEGY_STATUS_PANEL,
                                                     ChartIndicatorConstants::DEFAULT_SHOW_STRATEGY_STATUS_PANEL)
                                             .toBool();
    chartToolbar->setStrategyStatusPanelVisible(showStrategyStatusPanel);

    updateLowerPaneLayout();

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
    const double w = chartIndexUnitsPerBar(tf) * ChartConstants::CANDLESTICK_BODY_WIDTH;
    m_candlesticks->setWidth(w);
    ASSUME_TRUE(m_volumeIndicator != nullptr);
    m_volumeIndicator->setBarWidth(w);
    updateTimeFrameWatermark(m_displayTimeFrame);
    refreshIndicators();
}

void StockPriceChart::onLevel2Update(const QString& symbol, const Level2& level2)
{
    if (symbol != m_symbol)
    {
        return;
    }

    m_latestLevel2Snapshot = level2;

    const double bestBid = level2.m_bids[0].m_price;
    const double bestAsk = level2.m_asks[0].m_price;

    if (bestBid > 0.0 && bestAsk > 0.0)
    {
        m_bestBidPrice = bestBid;
        m_bestAskPrice = bestAsk;
    }
    else
    {
        m_bestBidPrice.reset();
        m_bestAskPrice.reset();
    }

    int snapshotMaxSize = 0;
    for (const Level2Row& level: level2.m_bids)
    {
        if (level.m_price > 0.0 && level.m_size > 0)
        {
            snapshotMaxSize = qMax(snapshotMaxSize, level.m_size);
        }
    }
    for (const Level2Row& level: level2.m_asks)
    {
        if (level.m_price > 0.0 && level.m_size > 0)
        {
            snapshotMaxSize = qMax(snapshotMaxSize, level.m_size);
        }
    }

    if (snapshotMaxSize > 0)
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_level2DepthSizeSamples.push_back(Level2DepthSizeSample{nowMs, snapshotMaxSize});
        while (!m_level2DepthSizeSamples.empty() &&
               (nowMs - m_level2DepthSizeSamples.front().timestampMs) > LEVEL2_DEPTH_ROLLING_MAX_WINDOW_MS)
        {
            m_level2DepthSizeSamples.pop_front();
        }
    }

    updateBboOverlay();
    updateLevel2DepthOverlay();
    if (m_currentOpenPosition != nullptr && !m_currentOpenPosition->isClosed && m_latestBarIndex >= 0)
    {
        updateOpenPositionPLBox(m_latestBar.getClose());
    }
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onBboOverlayVisibilityChanged(bool visible)
{
    m_showBboOverlay = visible;
    if (m_showBboOverlay && m_showLevel2DepthOverlay)
    {
        m_showLevel2DepthOverlay = false;
        ASSUME_TRUE(chartToolbar != nullptr);
        chartToolbar->setLevel2DepthOverlayVisible(false);
    }

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("Chart/ShowBbo", m_showBboOverlay);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_LEVEL2_DEPTH_OVERLAY,
                               m_showLevel2DepthOverlay);
    appStateSettings->sync();

    updateBboOverlay();
    updateLevel2DepthOverlay();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onLevel2DepthOverlayVisibilityChanged(bool visible)
{
    m_showLevel2DepthOverlay = visible;
    if (m_showLevel2DepthOverlay && m_showBboOverlay)
    {
        m_showBboOverlay = false;
        ASSUME_TRUE(chartToolbar != nullptr);
        chartToolbar->setBboOverlayVisible(false);
    }

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_LEVEL2_DEPTH_OVERLAY,
                               m_showLevel2DepthOverlay);
    appStateSettings->setValue("Chart/ShowBbo", m_showBboOverlay);
    appStateSettings->sync();

    updateBboOverlay();
    updateLevel2DepthOverlay();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onVwapVisibilityChanged(bool visible)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_VWAP, visible);
    appStateSettings->sync();

    ASSUME_TRUE(m_vwapIndicator != nullptr);
    m_vwapIndicator->setVisible(visible);
    ASSUME_TRUE(m_indicatorManager != nullptr);
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onVwapSettingsChanged(const int sourceMode,
                                            const QTime& sessionResetTime,
                                            const QColor& lineColor)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_VWAP_SOURCE_MODE, sourceMode);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_VWAP_SESSION_RESET_TIME,
                               sessionResetTime.toString("HH:mm"));
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_VWAP_COLOR, lineColor.name(QColor::HexArgb));
    appStateSettings->sync();

    ASSUME_TRUE(m_vwapIndicator != nullptr);
    VwapIndicator::Settings settings;
    settings.sourcePrice = (sourceMode == static_cast<int>(VwapIndicator::PriceSource::Hlc3))
                               ? VwapIndicator::PriceSource::Hlc3
                               : VwapIndicator::PriceSource::Close;
    settings.sessionResetTime = sessionResetTime.isValid() ? sessionResetTime : QTime(4, 0);
    settings.lineColor = lineColor.isValid() ? lineColor : QColor(0, 220, 220);
    m_vwapIndicator->setSettings(settings);
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onEmaVisibilityChanged(const int slot, const bool visible)
{
    if (slot < 0 || slot >= EMA_SLOT_COUNT)
    {
        return;
    }

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(emaShowKey(slot), visible);
    appStateSettings->sync();

    ASSUME_TRUE(m_emaIndicators[slot] != nullptr);
    m_emaIndicators[slot]->setVisible(visible);
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onEmaSettingsChanged(const int slot, const int period, const QColor& color)
{
    if (slot < 0 || slot >= EMA_SLOT_COUNT)
    {
        return;
    }

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(emaPeriodKey(slot), period);
    appStateSettings->setValue(emaColorKey(slot), color.name(QColor::HexArgb));
    appStateSettings->sync();

    ASSUME_TRUE(m_emaIndicators[slot] != nullptr);
    EmaIndicator::Settings settings;
    settings.period = qBound(1, period, 5000);
    settings.lineColor = color.isValid() ? color : defaultEmaColorForSlot(slot);
    m_emaIndicators[slot]->setSettings(settings);
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onMacdVisibilityChanged(const bool visible)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_MACD, visible);
    appStateSettings->sync();

    ASSUME_TRUE(m_macdIndicator != nullptr);
    m_macdIndicator->setVisible(visible);
    updateLowerPaneLayout();
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onStrategyStatusPanelVisibilityChanged(const bool visible)
{
    m_strategyStatusPanelVisible = visible;

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_STRATEGY_STATUS_PANEL, visible);
    appStateSettings->sync();

    updateLowerPaneLayout();
    updateStrategyStatusVisual();
}

void StockPriceChart::onMacdSettingsChanged(const int fastLength,
                                            const int slowLength,
                                            const int signalLength,
                                            const int macdMaType,
                                            const int signalMaType,
                                            const bool showHistogram)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_FAST_LENGTH, fastLength);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_SLOW_LENGTH, slowLength);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_SIGNAL_LENGTH, signalLength);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_MA_TYPE, macdMaType);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_SIGNAL_MA_TYPE, signalMaType);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_MACD_SHOW_HISTOGRAM, showHistogram);
    appStateSettings->sync();

    ASSUME_TRUE(m_macdIndicator != nullptr);
    MacdIndicator::Settings settings;
    settings.fastLength = fastLength;
    settings.slowLength = slowLength;
    settings.signalLength = signalLength;
    settings.macdMaType = (macdMaType == static_cast<int>(MacdIndicator::MaType::Sma)) ? MacdIndicator::MaType::Sma
                                                                                       : MacdIndicator::MaType::Ema;
    settings.signalMaType = (signalMaType == static_cast<int>(MacdIndicator::MaType::Sma)) ? MacdIndicator::MaType::Sma
                                                                                           : MacdIndicator::MaType::Ema;
    settings.showHistogram = showHistogram;
    m_macdIndicator->setSettings(settings);
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onRsiVisibilityChanged(const bool visible)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_SHOW_RSI, visible);
    appStateSettings->sync();

    ASSUME_TRUE(m_rsiIndicator != nullptr);
    m_rsiIndicator->setVisible(visible);
    updateLowerPaneLayout();
    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onRsiSettingsChanged(const int period,
                                           const int overboughtLevel,
                                           const int oversoldLevel,
                                           const QColor& color)
{
    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_RSI_PERIOD, period);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_RSI_OVERBOUGHT, overboughtLevel);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_RSI_OVERSOLD, oversoldLevel);
    appStateSettings->setValue(ChartIndicatorConstants::SETTINGS_KEY_RSI_COLOR, color.name(QColor::HexArgb));
    appStateSettings->sync();

    ASSUME_TRUE(m_rsiIndicator != nullptr);
    RsiIndicator::Settings settings;
    settings.period = qBound(1, period, 5000);
    settings.overboughtLevel = qBound(1, overboughtLevel, 100);
    settings.oversoldLevel = qBound(0, oversoldLevel, qMin(99, settings.overboughtLevel - 1));
    settings.lineColor = color.isValid() ? color : defaultRsiColor();
    m_rsiIndicator->setSettings(settings);

    refreshIndicators();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onSessionBackgroundColorsChanged(const QColor& earlyPreMarketColor,
                                                       const QColor& preMarketColor,
                                                       const QColor& afterHoursColor)
{
    const QColor defaultEarlyColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_EARLY_PRE_MARKET_COLOR,
                                      QColor(255, 165, 0, 90));
    const QColor defaultPreColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_PRE_MARKET_COLOR, QColor(255, 165, 0, 180));
    const QColor defaultAfterColor =
        parseColorSettingWithFallback(ChartBackgroundConstants::DEFAULT_AFTER_HOURS_COLOR, QColor(138, 43, 226, 180));

    const QColor normalizedEarly = earlyPreMarketColor.isValid() ? earlyPreMarketColor : defaultEarlyColor;
    const QColor normalizedPre = preMarketColor.isValid() ? preMarketColor : defaultPreColor;
    const QColor normalizedAfter = afterHoursColor.isValid() ? afterHoursColor : defaultAfterColor;

    if (normalizedEarly == m_earlyPreMarketBackgroundColor && normalizedPre == m_preMarketBackgroundColor &&
        normalizedAfter == m_afterHoursBackgroundColor)
    {
        return;
    }

    m_earlyPreMarketBackgroundColor = normalizedEarly;
    m_preMarketBackgroundColor = normalizedPre;
    m_afterHoursBackgroundColor = normalizedAfter;

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue(ChartBackgroundConstants::SETTINGS_KEY_EARLY_PRE_MARKET_COLOR,
                               m_earlyPreMarketBackgroundColor.name(QColor::HexArgb));
    appStateSettings->setValue(ChartBackgroundConstants::SETTINGS_KEY_PRE_MARKET_COLOR,
                               m_preMarketBackgroundColor.name(QColor::HexArgb));
    appStateSettings->setValue(ChartBackgroundConstants::SETTINGS_KEY_AFTER_HOURS_COLOR,
                               m_afterHoursBackgroundColor.name(QColor::HexArgb));
    appStateSettings->sync();

    updateSessionBackgroundRectBrushes();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::updateBboOverlay()
{
    const bool hasPrices = m_bestBidPrice.has_value() && m_bestAskPrice.has_value();
    const bool canDisplay =
        m_showBboOverlay && hasPrices && m_currentTimeLine != nullptr && m_currentTimeLine->visible();

    if (!canDisplay)
    {
        m_bestBidLine->setVisible(false);
        m_bestAskLine->setVisible(false);
        m_bestBidLabel->setVisible(false);
        m_bestAskLabel->setVisible(false);
        return;
    }

    const double bestBid = m_bestBidPrice.value();
    const double bestAsk = m_bestAskPrice.value();

    const QCPRange xRange = m_customPlot->xAxis->range();
    const QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    const double xStart = m_currentTimeLine->start->coords().x();
    const double xEnd = qMax(xStart, xRange.upper);

    const double bidY = qBound(yRange.lower, bestBid, yRange.upper);
    const double askY = qBound(yRange.lower, bestAsk, yRange.upper);

    // Keep labels farther from the now-line so they do not clash with nearby P&L widgets.
    const double labelOffset = qMax((xEnd - xStart) * 0.12, 4.0);
    const double labelX = qMin(xStart + labelOffset, xEnd);

    // Draw guide lines only from the now-line to the label anchor (not to chart edge).
    m_bestBidLine->start->setCoords(xStart, bidY);
    m_bestBidLine->end->setCoords(labelX, bidY);
    m_bestBidLine->setVisible(true);

    m_bestAskLine->start->setCoords(xStart, askY);
    m_bestAskLine->end->setCoords(labelX, askY);
    m_bestAskLine->setVisible(true);

    m_bestBidLabel->position->setCoords(labelX, bidY);
    m_bestBidLabel->setText(QString("Bid %1").arg(bestBid, 0, 'f', 2));
    m_bestBidLabel->setVisible(true);

    m_bestAskLabel->position->setCoords(labelX, askY);
    m_bestAskLabel->setText(QString("Ask %1").arg(bestAsk, 0, 'f', 2));
    m_bestAskLabel->setVisible(true);
}

void StockPriceChart::updateLevel2DepthOverlay()
{
    const auto hideAllDepthLines = [this]()
    {
        for (QCPItemLine* line: m_depthBidLines)
        {
            if (line != nullptr)
            {
                line->setVisible(false);
            }
        }
        for (QCPItemLine* line: m_depthAskLines)
        {
            if (line != nullptr)
            {
                line->setVisible(false);
            }
        }
    };

    if (!m_showLevel2DepthOverlay || !m_latestLevel2Snapshot.has_value() || m_currentTimeLine == nullptr ||
        !m_currentTimeLine->visible())
    {
        hideAllDepthLines();
        return;
    }

    const QCPRange xRange = m_customPlot->xAxis->range();
    const QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    const double visibleSpan = xRange.upper - xRange.lower;
    if (visibleSpan <= 0.0)
    {
        hideAllDepthLines();
        return;
    }

    int rollingMaxSize = 0;
    for (const Level2DepthSizeSample& sample: m_level2DepthSizeSamples)
    {
        rollingMaxSize = qMax(rollingMaxSize, sample.maxSize);
    }

    const Level2& level2 = m_latestLevel2Snapshot.value();
    if (rollingMaxSize <= 0)
    {
        for (const Level2Row& level: level2.m_bids)
        {
            if (level.m_price > 0.0 && level.m_size > 0)
            {
                rollingMaxSize = qMax(rollingMaxSize, level.m_size);
            }
        }
        for (const Level2Row& level: level2.m_asks)
        {
            if (level.m_price > 0.0 && level.m_size > 0)
            {
                rollingMaxSize = qMax(rollingMaxSize, level.m_size);
            }
        }
    }

    if (rollingMaxSize <= 0)
    {
        hideAllDepthLines();
        return;
    }

    // Anchor depth bars to the right edge of the active candle slot, not to the
    // moving timeline position inside the candle. This keeps depth bars fixed
    // during the bar and shifts them one full candle width right only when a
    // new bar starts.
    const double timelineX = m_currentTimeLine->start->coords().x();
    const double indexUnitsPerBar = chartIndexUnitsPerBar(m_displayTimeFrame);
    if (indexUnitsPerBar <= 0.0)
    {
        hideAllDepthLines();
        return;
    }
    const double currentBarStart = qFloor(timelineX / indexUnitsPerBar) * indexUnitsPerBar;
    const double xStart = currentBarStart + indexUnitsPerBar;
    const double maxWidth = qMax(visibleSpan * LEVEL2_DEPTH_MAX_WIDTH_FRACTION, LEVEL2_DEPTH_MIN_WIDTH_INDEX_UNITS);

    const auto renderDepthSide = [&](const std::array<Level2Row, 10>& levels, const std::array<QCPItemLine*, 10>& lines)
    {
        int visibleLineCount = 0;
        for (const Level2Row& level: levels)
        {
            if (level.m_price <= 0.0 || level.m_size <= 0)
            {
                continue;
            }

            if (level.m_price < yRange.lower || level.m_price > yRange.upper)
            {
                continue;
            }

            if (visibleLineCount >= static_cast<int>(lines.size()))
            {
                break;
            }

            const double ratio =
                qBound(0.0, static_cast<double>(level.m_size) / static_cast<double>(rollingMaxSize), 1.0);
            if (ratio <= 0.0)
            {
                continue;
            }

            const double width = qMax(maxWidth * ratio, LEVEL2_DEPTH_MIN_DRAWN_WIDTH_INDEX_UNITS);
            QCPItemLine* line = lines[visibleLineCount];
            ++visibleLineCount;
            OBJ_ASSUME_DIFF(line, nullptr);
            line->start->setCoords(xStart, level.m_price);
            line->end->setCoords(xStart + width, level.m_price);
            line->setVisible(true);
        }

        for (int i = visibleLineCount; i < static_cast<int>(lines.size()); ++i)
        {
            QCPItemLine* line = lines[i];
            if (line != nullptr)
            {
                line->setVisible(false);
            }
        }
    };

    renderDepthSide(level2.m_bids, m_depthBidLines);
    renderDepthSide(level2.m_asks, m_depthAskLines);
}

void StockPriceChart::updateDateWatermark(const QDate& p_date)
{
    ASSUME_DIFF(m_dateWatermark, nullptr);
    m_dateWatermark->setText(p_date.isValid() ? p_date.toString("yyyy-MM-dd") : QString());
}

void StockPriceChart::updateTimeFrameWatermark(const TimeFrame p_tf)
{
    ASSUME_DIFF(m_timeFrameWatermark, nullptr);
    m_timeFrameWatermark->setText(timeFrameToString(p_tf));
}

void StockPriceChart::applyManualConfirmationFlashState(bool p_flashOn)
{
    ASSUME_DIFF(m_manualConfirmationHeader, nullptr);
    ASSUME_DIFF(m_manualConfirmationWatermark, nullptr);
    ASSUME_DIFF(m_customPlot, nullptr);
    ASSUME_DIFF(m_manualConfirmationMainBorder, nullptr);
    ASSUME_DIFF(m_manualConfirmationVolumeBorder, nullptr);

    m_manualConfirmationHeader->setColor(p_flashOn ? MANUAL_CONFIRMATION_COLOR_ON : MANUAL_CONFIRMATION_COLOR_OFF);
    m_manualConfirmationWatermark->setColor(p_flashOn ? MANUAL_CONFIRMATION_COLOR_ON : MANUAL_CONFIRMATION_COLOR_OFF);
    m_manualConfirmationMainBorder->setVisible(p_flashOn);
    // Keep the volume pane border static; confirmation flash should only highlight the main price pane.
    m_manualConfirmationVolumeBorder->setVisible(false);
}

void StockPriceChart::refreshIndicators()
{
    if (m_indicatorManager == nullptr)
    {
        return;
    }

    const ChartIndicator::UpdateContext context{indexToBar, m_index0Timestamp, m_displayTimeFrame};
    m_indicatorManager->rebuildAll(context);
}

void StockPriceChart::updateLowerPaneLayout()
{
    const bool showVolume = m_volumeIndicator != nullptr ? m_volumeIndicator->isVisible() : true;
    const bool showMacd = m_macdIndicator != nullptr ? m_macdIndicator->isVisible() : false;
    const bool showRsi = m_rsiIndicator != nullptr ? m_rsiIndicator->isVisible() : false;

    // Volume is blended in the main pane; keep the legacy volume subpane collapsed.
    m_volumeAxisRect->setMinimumSize(QSize(0, 0));
    m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_volumeAxisRect->setVisible(false);

    m_customPlot->yAxis->setVisible(showVolume);
    m_customPlot->yAxis->setTicks(showVolume);
    m_customPlot->yAxis->setTickLabels(showVolume);

    if (showMacd)
    {
        m_macdAxisRect->setMinimumSize(QSize(0, 0));
        m_macdAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 120));
        m_macdAxisRect->setVisible(true);
    }
    else
    {
        m_macdAxisRect->setMinimumSize(QSize(0, 0));
        m_macdAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
        m_macdAxisRect->setVisible(false);
    }

    // Strategy status is now an overlay in the main chart area (no dedicated subpanel).
    m_statusAxisRect->setMinimumSize(QSize(0, 0));
    m_statusAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
    m_statusAxisRect->setVisible(false);

    if (showRsi)
    {
        m_rsiAxisRect->setMinimumSize(QSize(0, 0));
        m_rsiAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 110));
        m_rsiAxisRect->setVisible(true);
    }
    else
    {
        m_rsiAxisRect->setMinimumSize(QSize(0, 0));
        m_rsiAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 0));
        m_rsiAxisRect->setVisible(false);
    }

    m_customPlot->xAxis->setTicks(true);
    m_customPlot->xAxis->setTickLabels(true);

    // Keep the timeline anchored to the main candlestick axis.
    // Lower panes stay aligned to x-range but do not render time labels.
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight);

    m_macdAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_macdAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight);

    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_rsiAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight);

    m_statusAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);
    m_statusAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight);

    if (!m_strategyStatusPanelVisible && m_strategyStatusLabel != nullptr)
    {
        m_strategyStatusLabel->setVisible(false);
    }

    m_customPlot->plotLayout()->setRowSpacing(0);
    m_customPlot->plotLayout()->updateLayout();
    repositionStrategyStatusVisual();
}

double StockPriceChart::chartIndexUnitsPerBar(TimeFrame p_tf)
{
    return (p_tf == TimeFrame::TEN_SECONDS) ? 1.0 : static_cast<double>(BarUtils::minutesPerBar(p_tf));
}

double StockPriceChart::chartIndexKeyOffset(TimeFrame p_tf)
{
    return chartIndexUnitsPerBar(p_tf) / 2.0;
}

double StockPriceChart::chartSecondsPerIndexUnit(TimeFrame p_tf)
{
    return (p_tf == TimeFrame::TEN_SECONDS) ? 10.0 : 60.0;
}

void StockPriceChart::preserveCurrentRanges()
{
    m_preservedXRange = m_customPlot->xAxis->range();
    m_preservedYRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
}

bool StockPriceChart::snapshotCurrentViewRanges(double& p_xLower,
                                                double& p_xUpper,
                                                double& p_yLower,
                                                double& p_yUpper) const
{
    const QCPRange xRange = m_customPlot->xAxis->range();
    const QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    if (!QCPRange::validRange(xRange) || !QCPRange::validRange(yRange))
    {
        return false;
    }

    p_xLower = xRange.lower;
    p_xUpper = xRange.upper;
    p_yLower = yRange.lower;
    p_yUpper = yRange.upper;
    return true;
}

void StockPriceChart::applyViewRanges(const double p_xLower,
                                      const double p_xUpper,
                                      const double p_yLower,
                                      const double p_yUpper,
                                      const bool p_preserveYForInitialBarBatch)
{
    const QCPRange xRange(p_xLower, p_xUpper);
    const QCPRange yRange(p_yLower, p_yUpper);
    if (!QCPRange::validRange(xRange) || !QCPRange::validRange(yRange))
    {
        return;
    }

    m_customPlot->xAxis->setRange(xRange);
    m_customPlot->axisRect()->axis(QCPAxis::atRight)->setRange(yRange);
    if (p_preserveYForInitialBarBatch)
    {
        m_preservedYRange = yRange;
    }
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::connectReplayControls(ReplayControlsBar* controls)
{
    OBJ_ASSUME_DIFF(controls, nullptr);
    m_replayControls = controls;

    connect(m_replayControls, &ReplayControlsBar::replayDayChanged, this, &StockPriceChart::onReplayDayChanged);
    connect(m_replayControls, &ReplayControlsBar::replayStartTimeChanged, this, &StockPriceChart::onReplayTimeChanged);
    connect(m_replayControls,
            &ReplayControlsBar::replaySpeedChanged,
            this,
            [](Playback::Speed speed) { MainApp::getInstance()->setReplaySpeed(speed); });

    connect(m_replayControls,
            &ReplayControlsBar::replayPlayPauseToggled,
            this,
            [this](bool playing)
            {
                if (playing)
                {
                    QDate date = m_replayControls->getSelectedReplayDay();
                    QTime startTime = m_replayControls->getReplayStartTime();
                    Playback::Speed speed = m_replayControls->getReplaySpeed();

                    // Use the controls' own ReplayState (GUI thread) to determine
                    // whether to resume or start fresh. Do NOT use isReplayPaused()
                    // which reads MainAlgo state cross-thread — it returns false when
                    // enterReplayModePaused is still queued but not yet executed,
                    // causing a double-enter race that fires the m_replayEngine==null ASSERT.
                    const ReplayControlsBar::ReplayState ctrlState = m_replayControls->getReplayState();
                    const bool shouldResume = (ctrlState == ReplayControlsBar::ReplayState::Paused ||
                                               ctrlState == ReplayControlsBar::ReplayState::PreloadingPaused);

                    if (shouldResume)
                    {
                        MainApp::getInstance()->resumeReplayPlayback();
                    }
                    else if (date.isValid())
                    {
                        MainApp::getInstance()->startReplayPlayback(date, startTime, speed);
                    }
                    else
                    {
                        qWarning() << "Cannot start replay: no valid date selected";
                        m_replayControls->setReplayPlaying(false);
                    }
                }
                else
                {
                    MainApp::getInstance()->pauseReplayPlayback();
                }
            });

    // Populate replay days immediately if a symbol is already set
    if (!m_symbol.isEmpty())
        populateAvailableReplayDays();
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
    // Preserve the current X (time) range so the new symbol opens at the same view position,
    // but only when the chart actually has content (valid time anchor).
    // If there's no valid anchor (e.g., first load) we skip so the default (-60, 30) is used.
    // Y range intentionally NOT preserved — the new symbol has different price levels.
    if (m_index0Timestamp.isValid())
    {
        m_preservedXRange = m_customPlot->xAxis->range();
    }

    // Clear all bar data, index mappings and in-flight requests from the previous symbol.
    clearSymbol();
    clearOrderVisualizations();

    m_symbol = symbol;
    m_candlesticks->setName(symbol + " (Bars)");

    m_bestBidPrice.reset();
    m_bestAskPrice.reset();
    updateBboOverlay();

    // Update symbol watermark
    m_symbolWatermark->setText(symbol);
    if (symbol.isEmpty())
    {
        updateDateWatermark(QDate());
    }
    m_customPlot->replot();

    // Compute index 0 from current time and request initial historical bars
    initializeTimeAnchor();

    DEBUG << "Set chart symbol to" << symbol;
}

void StockPriceChart::showManualConfirmationCue(const QString& p_text, const bool p_flashEnabled)
{
    ASSUME_DIFF(m_manualConfirmationHeader, nullptr);
    ASSUME_DIFF(m_manualConfirmationWatermark, nullptr);
    ASSUME_DIFF(m_manualConfirmationFlashTimer, nullptr);

    const QStringList lines = p_text.split('\n', Qt::SkipEmptyParts);
    const QString headerText = lines.value(0);
    const QString footerText = lines.size() > 1 ? lines.last() : lines.value(0);

    m_manualConfirmationHeader->setText(headerText);
    m_manualConfirmationHeader->setVisible(true);
    m_manualConfirmationWatermark->setText(footerText);
    m_manualConfirmationWatermark->setVisible(true);

    if (p_flashEnabled)
    {
        if (!m_manualConfirmationFlashTimer->isActive())
        {
            m_manualConfirmationFlashOn = true;
            applyManualConfirmationFlashState(true);
            m_manualConfirmationFlashTimer->start();
        }
    }
    else
    {
        if (m_manualConfirmationFlashTimer->isActive())
        {
            m_manualConfirmationFlashTimer->stop();
        }
        m_manualConfirmationFlashOn = false;
        applyManualConfirmationFlashState(false);
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::clearManualConfirmationCue()
{
    ASSUME_DIFF(m_manualConfirmationHeader, nullptr);
    ASSUME_DIFF(m_manualConfirmationWatermark, nullptr);
    ASSUME_DIFF(m_manualConfirmationFlashTimer, nullptr);

    m_manualConfirmationFlashTimer->stop();
    m_manualConfirmationFlashOn = false;
    m_manualConfirmationHeader->setVisible(false);
    m_manualConfirmationHeader->setText(QString());
    m_manualConfirmationWatermark->setVisible(false);
    m_manualConfirmationWatermark->setText(QString());
    applyManualConfirmationFlashState(false);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::setManualConfirmationMutedWatermarkVisible(const bool p_visible)
{
    ASSUME_DIFF(m_manualConfirmationMutedWatermark, nullptr);
    if (m_manualConfirmationMutedWatermark->visible() == p_visible)
    {
        return;
    }

    m_manualConfirmationMutedWatermark->setVisible(p_visible);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onManualConfirmationFlashTick()
{
    ASSUME_DIFF(m_manualConfirmationWatermark, nullptr);

    m_manualConfirmationFlashOn = !m_manualConfirmationFlashOn;
    applyManualConfirmationFlashState(m_manualConfirmationFlashOn);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

/**
 * @brief Computes m_index0Timestamp from clock time and sets up the chart.
 *
 * This is called on symbol selection — no live data needed.
 * Sets up the time ticker, view range, and requests initial historical bars.
 */
void StockPriceChart::initializeTimeAnchor()
{
    m_isReplayNoDataState = false;

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
    m_macdAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
    m_rsiAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
    m_statusAxisRect->axis(QCPAxis::atBottom)->setTicker(indexToTimeTicker);
    m_customPlot->xAxis->setTicker(indexToTimeTicker);
    updateLowerPaneLayout();

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

    drawSessionBackgroundsForDate(m_index0Timestamp.date());

    // Defer initial bar request to next event loop iteration.
    // This is necessary because setSymbol() fires before onSelectDisplayedStock
    // reaches MainAlgo (queued cross-thread connection). The deferred call ensures
    // the StockInstrument is created before we request bars from its cache.
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (!m_index0Timestamp.isValid() || m_symbol.isEmpty())
                return;

            if (MainApp::isInReplayMode())
            {
                const QDate replayAnchorDate = MainApp::getCurrentAppTime().date();
                if (!DBClient::getInstance()->hasReplayData(replayAnchorDate, m_symbol))
                {
                    enterReplayNoDataState(
                        QString("No replay data for %1 on %2").arg(m_symbol, replayAnchorDate.toString(Qt::ISODate)));
                    return;
                }
            }

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
    if (m_replayControls)
    {
        m_replayControls->scanAndPopulateReplayDays();
        qCInfo(ChartLog) << "Updated replay-day availability in replay controls";
    }
}
