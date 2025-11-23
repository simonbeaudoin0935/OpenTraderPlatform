#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QScatterSeries>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QDateTime>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QTimeZone>
#include <QMap>
#include <QLoggingCategory>

#include "Bar.h"
#include "TimeFrameSelector.h"

Q_DECLARE_LOGGING_CATEGORY(ChartLog)

class QGraphicsRectItem;

/**
 * @class StockPriceChart
 * @brief A chart widget that displays stock price data using candlesticks.
 * 
 * This chart displays bars continuously without gaps for closed market periods.
 * It uses an index-based positioning system where each bar is assigned a sequential
 * index (0, 1, 2, ...) for continuous display, while maintaining mappings to actual
 * timestamps. This allows the chart to show:
 * - Last bar Friday 7:59pm → next to Monday 4:00am (no weekend gap)
 * - Last bar 7:59pm → next to next day 4:00am (no overnight gap)
 * - Only 4am-8pm ET trading hours on weekdays
 * 
 * The X-axis uses QValueAxis with indices, and custom labels show actual timestamps.
 */
class StockPriceChart : public QWidget {
    Q_OBJECT

public:
    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);
    void clearSymbol();

signals:
    void requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime);

public slots:
    void addBar(const Bar& bar);
    void onRequestedMissingBarsReceived(const QVector<Bar> &bars);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    static const int MAX_BARS = 1000;

    void updateChart();
    void handleClosedBar(const Bar& bar);
    void handleOpenBar(const Bar& bar);
    void updateLastPriceLine(double price, bool isUpTick);
    void updatePriceLabelPosition();
    bool isAfterMarketHours(const QDateTime& localTime);
    void updateAfterHoursBackground();
    void maintainBarLimit();
    void handleVerticalPanning(QWheelEvent* event);
    void handleHorizontalPanning(QWheelEvent* event);
    void handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleVerticalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor);
    void updateLastPriceLineIfNeeded();
    void handlePanning(QMouseEvent* mouseEvent);
    void checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime);
    
    // Index-based positioning helpers
    void rebuildIndexMapping();
    int getIndexForTimestamp(const QDateTime& timestamp) const;
    QDateTime getTimestampForIndex(int index) const;
    void updateAxisLabels();
    void drawBackgroundForTimeRange(const QDateTime& rangeStart, const QDateTime& rangeEnd, 
                                     const QColor& color, int zValue,
                                     QList<QGraphicsRectItem*>& rectList);

    QString symbol;
    QChart* chart;
    QLineSeries* lastPriceLine;
    QCandlestickSeries* candlestickSeries;
    QScatterSeries* voidBarSeries;
    QChartView* chartView;
    QValueAxis* axisX;  // Changed from QDateTimeAxis - now uses indices
    QValueAxis* axisY;
    QGraphicsTextItem* priceLabel;
    QList<QGraphicsRectItem*> afterHoursRects;  // List of rectangles for after-hours sessions
    QList<QGraphicsRectItem*> preMarketRects;   // List of rectangles for pre-market sessions
    QList<QGraphicsRectItem*> closedMarketRects; // List of rectangles for closed market periods

    // Track the current open bar
    Bar currentOpenBar;
    bool hasOpenBar = false;
    double lastPrice = 0.0;
    double lastValidClosePrice = 0.0;  // Track the last valid close price for void bar positioning

    // Store completed bars in a map with timestamp as key
    QMap<QDateTime, Bar> completedBars;
    
    // Track void bars (bars with BarStatus::Null)
    QMap<QDateTime, double> voidBars;  // timestamp -> price to display
    
    // Index-based positioning maps
    QMap<int, QDateTime> indexToTimestamp;  // Map from index to timestamp
    QMap<QDateTime, int> timestampToIndex;  // Map from timestamp to index

    // Mouse tracking for panning
    bool isPanning = false;
    QPoint lastMousePos;

    // Timeframe selector widget
    TimeFrameSelector* timeframeSelector;

    // Helper method to create a background rectangle
    QGraphicsRectItem* createBackgroundRect(const QColor& color, int zValue);
    // Helper method to clear all background rectangles
    void clearBackgroundRects();

    // Event handling helper functions
    bool handleMouseButtonPress(QMouseEvent* event);
    bool handleMouseButtonRelease(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);

    bool currentGetBarsRequestInProcess = false;
};
