#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QTimeZone>
#include <QMap>
#include <QLoggingCategory>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QSemaphore>

#include "qcustomplot.h"
#include "Bar.h"
#include "ChartToolbar.h"
#include "CONSTANTS.h"

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
class StockPriceChart : public QWidget
{
    Q_OBJECT

  public:
    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);
    void clearSymbol();

    /**
     * @brief Populates the replay day dropdown with available dates from cache.
     */
    void populateAvailableReplayDays();

  signals:
    void requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime);

  public slots:
    void addLiveBar(const QString& symbol, const Bar& bar);
    void onRequestedMissingBarsReceived(const std::shared_ptr<QVector<Bar>>& barsPtr);
    void onRequestedMissingBarsFailed();
    void setReplayModeActive(bool active);

  private slots:
    void onAxisRangeChanged();
    void onVolumeChartVisibilityChanged(bool visible);
    void onVolumeAutoRescaleChanged(bool enabled);
    void onReplayDayChanged(const QDate& date);
    void onReplayTimeRangeQueryFinished();

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    // Track the current open bar
    Bar m_latestBar;
    int m_latestBarIndex = -1;

    /**
     * @brief Queries the database for the first and last timestamps of a stock on a specific date.
     * @param symbol The stock symbol to query
     * @param date The date to query
     * @return A tuple of QDateTime objects representing the first and last timestamps, and the bar count
     */
    std::tuple<QDateTime, QDateTime, int> queryStockTimeRangeForDate(const QString& symbol, const QDate& date);

    void redrawLastPriceLine();
    void handleVerticalPanning(QWheelEvent* event);
    void handleHorizontalPanning(QWheelEvent* event);
    void handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleVerticalZoom(QWheelEvent* event, bool isOverVolumeChart, qreal zoomFactor);
    void handleBothAxesZoom(QWheelEvent* event, bool isOverVolumeChart, qreal zoomFactor);
    void checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime);
    QDateTime getTimestampForIndex(int index) const;
    int getIndexForTimestamp(const QDateTime& timestamp) const;

    // Index-based positioning helpers
    void addHistoricalBarsToIndexMapping(const std::shared_ptr<QVector<Bar>>& bars);

    QDateTime getPreviousTradingMinute(const QDateTime& timestamp) const;
    QDateTime adjustToValidTradingTime(const QDateTime& timestamp) const;
    QDate getPreviousFriday(const QDate& date) const;
    void updateAxisLabelsDensity();
    void updateCandlestickData();
    void updateVolumeData();
    void rescaleVolumeAxisToVisibleRange();

    // Background rendering methods
    void drawBackgroundsForReceivedBars(const QVector<Bar>& bars);
    void clearBackgroundRects();
    void drawFixedBackgroundRect(const QDate& date,
                                 const QTime& rangeStart,
                                 const QTime& rangeEnd,
                                 const QColor& color,
                                 QList<QCPItemRect*>& rectList);

    QString m_symbol;
    QCustomPlot* m_customPlot;
    QCPFinancial* m_candlesticks;
    QCPItemLine* m_lastPriceLine;
    QCPItemText* m_priceLabel;

    // Replay mode visual elements
    QCPItemText* m_replayWatermark;
    bool m_isReplayModeActive = false;
    static constexpr QColor NORMAL_BACKGROUND_COLOR{75, 75, 80};
    static constexpr QColor REPLAY_BACKGROUND_COLOR{60, 60, 75}; // Slightly bluer tint

    // Volume chart components
    QCPAxisRect* m_volumeAxisRect;
    QCPBars* m_volumePos;
    QCPBars* m_volumeNeg;

    // Background rectangles for different market sessions
    QList<QCPItemRect*> m_preMarketRects;
    QList<QCPItemRect*> m_afterHoursRects;
    QSet<QDate> m_datesWithBackgrounds; // Track which dates already have backgrounds drawn
    // The double associatives maps indexToBar and timestampToIndex are used to avoid caring about
    // the time when the market is
    //QList<QCPItemRect*> m_closedMarketRects;

    // Index-based positioning maps
    QMap<int, Bar> indexToBar;             // Map from index to Bar
    QMap<QDateTime, int> timestampToIndex; // Map from timestamp to index

    // Timeframe selector widget
    ChartToolbar* chartToolbar;

    // Binary semaphore to track if a missing bars request is in progress
    // Initialized with count 1 (not acquired). Acquire before requesting, release when received.
    QSemaphore m_missingBarsRequestSemaphore{1};

    // Wheel zoom sensitivity ratio
    qreal wheelZoomRatio = 1.0;

    // Volume auto-rescale state
    bool m_volumeAutoRescaleEnabled = true;

    // Replay functionality
    QFutureWatcher<std::tuple<QDateTime, QDateTime, int>>* replayTimeRangeWatcher;

    // Helper to convert index to time for axis labels
    QString indexToTimeString(double index) const;

    bool startedReceivingRealtimeBars = false;
};
