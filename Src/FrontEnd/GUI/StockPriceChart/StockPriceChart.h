#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QTimeZone>
#include <QMap>
#include <QLoggingCategory>
#include <QVBoxLayout>

#include "qcustomplot.h"
#include "Bar.h"
#include "TimeFrameSelector.h"

Q_DECLARE_LOGGING_CATEGORY(ChartLog)

/**
 * @class StockPriceChart
 * @brief A chart widget that displays stock price data using candlesticks via qcustomplot.
 * 
 * This chart displays bars continuously without gaps for closed market periods.
 * It uses an index-based positioning system where each bar is assigned a sequential
 * index (0, 1, 2, ...) for continuous display, while maintaining mappings to actual
 * timestamps. This allows the chart to show:
 * - Last bar Friday 7:59pm → next to Monday 4:00am (no weekend gap)
 * - Last bar 7:59pm → next to next day 4:00am (no overnight gap)
 * - Only 4am-8pm ET trading hours on weekdays
 * 
 * Uses qcustomplot library for rendering instead of Qt Charts.
 */
class StockPriceChart : public QWidget {
    Q_OBJECT

public:
    // Trading hours constants (America/New_York timezone)
    static constexpr int TRADING_START_HOUR = 6;   // 6:00 AM ET
    static constexpr int TRADING_END_HOUR = 20;    // 8:00 PM ET (20:00)
    static constexpr int LAST_TRADING_MINUTE = 59; // Last bar is at 7:59 PM
    static constexpr int MONDAY = 1;               // Qt::Monday
    static constexpr int FRIDAY = 5;               // Qt::Friday

    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);
    void clearSymbol();

signals:
    void requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime);

public slots:
    void addLiveBar(const QString& symbol, const Bar& bar);
    void onRequestedMissingBarsReceived(const QVector<Bar>& bars);

private slots:
    void onAxisRangeChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    static const int MAX_BARS = 1000;

    // Track the current open bar
    Bar m_latestBar;
    int m_latestBarIndex = -1;

    // Configurable thresholds for axis label density (pixels per tick)
    static constexpr int MIN_PIXELS_PER_TICK_X = 40;
    static constexpr int MIN_PIXELS_PER_TICK_Y = 30;

    void redrawLastPriceLine();
    void maintainBarLimit();
    void handleVerticalPanning(QWheelEvent* event);
    void handleHorizontalPanning(QWheelEvent* event);
    void handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleVerticalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor);
    void checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime);
    QDateTime getTimestampForIndex(int index) const;
    
    // Index-based positioning helpers
    void addHistoricalBarsToIndexMapping(const QVector<Bar>& bars);

    QDateTime getPreviousTradingMinute(const QDateTime& timestamp) const;
    QDateTime adjustToValidTradingTime(const QDateTime& timestamp) const;
    QDate getPreviousFriday(const QDate& date) const;
    void updateAxisLabelsDensity();
    void updateCandlestickData();
    
    QString m_symbol;
    QCustomPlot* m_customPlot;
    QCPFinancial* m_candlesticks;
    QCPItemLine* m_lastPriceLine;
    QCPItemText* m_priceLabel;
    
    // Volume chart components
    QCPAxisRect* m_volumeAxisRect;
    QCPBars* m_volumePos;
    QCPBars* m_volumeNeg;
    
    // Index-based positioning maps
    QMap<int, Bar> indexToBar;  // Map from index to Bar
    QMap<QDateTime, int> timestampToIndex;  // Map from timestamp to index

    // Timeframe selector widget
    TimeFrameSelector* timeframeSelector;

    bool currentGetBarsRequestInProcess = false;
    
    // Helper to convert index to time for axis labels
    QString indexToTimeString(double index) const;
    void updateVolumeData();
};
