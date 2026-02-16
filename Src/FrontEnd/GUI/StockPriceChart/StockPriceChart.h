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
#include <QTimer>

#include "qcustomplot.h"
#include "Bar.h"
#include "ChartToolbar.h"
#include "CONSTANTS.h"

// Forward declarations
class Order;
class Position;

Q_DECLARE_LOGGING_CATEGORY(ChartLog)

/**
 * @brief Represents a visual marker for an order on the chart.
 *
 * Each order (pending, filled, or cancelled) gets a marker showing its
 * exact price and time position. Markers scale with zoom level and support
 * click interaction to show order details.
 */
struct OrderMarker
{
    QString orderID;
    QString symbol;
    QDateTime timestamp; // Fill time for filled, placement time for pending
    double price;        // Fill price for filled, order price for pending
    int quantity;
    bool isBuy; // true = buy, false = sell
    QString accountID;

    enum class State
    {
        Pending,   // Text with "(pending)"
        Filled,    // Solid triangle text
        Cancelled, // Gray X text
        Rejected   // Gray X (similar to cancelled)
    };
    State state = State::Pending;

    // Visual element (owned by QCustomPlot)
    QCPItemText* priceLabel = nullptr; // Text label showing BUY/SELL/CANCELLED
};

/**
 * @brief Represents a position's visual elements on the chart.
 *
 * A position is a round-trip from 0 shares to some quantity back to 0.
 * Multiple entries (DCA) are connected sequentially, and the position
 * shows a dynamic line to current price while open.
 */
struct PositionVisualization
{
    QString positionID;
    QString symbol;

    // Order markers that form this position
    QVector<OrderMarker*> entryMarkers; // Buy orders for long, sell orders for short
    QVector<OrderMarker*> exitMarkers;  // Sell orders for long, buy orders for short

    // Lines connecting entries sequentially
    QVector<QCPItemLine*> entryConnectionLines;

    // Dynamic line from last entry to current price (open positions only)
    QCPItemLine* dynamicLine = nullptr;

    // Lines from entries to exits (closed positions)
    QVector<QCPItemLine*> exitLines;

    // P&L label near last exit (closed positions only)
    QCPItemText* plLabel = nullptr;

    // Position state
    bool isClosed = false;
    bool isShort = false; // true for short positions (inverted P&L logic)
    double avgEntryPrice = 0.0;
    int currentQuantity = 0;
    int totalBought = 0;
    int totalSold = 0;
    double realizedPL = 0.0;
};

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
     * @brief Clears all chart data, index mappings, and background rects.
     * Used when entering replay mode to start fresh.
     */
    void clearChart();

    /**
     * @brief Populates the replay day dropdown with available dates from cache.
     */
    void populateAvailableReplayDays();

    /**
     * @brief Gets the currently displayed symbol.
     * @return The current symbol string.
     */
    [[nodiscard]] QString getCurrentSymbol() const
    {
        return m_symbol;
    }

  signals:
    void requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime);

  public slots:
    void addLiveBar(const QString& symbol, const Bar& bar);
    void onRequestedMissingBarsReceived(const std::shared_ptr<QVector<Bar>>& barsPtr);
    void onRequestedMissingBarsFailed();
    void setReplayModeActive(bool active);

    /**
     * @brief Gets the chart toolbar widget.
     * @return Pointer to the ChartToolbar.
     */
    [[nodiscard]] ChartToolbar* toolbar() const
    {
        return chartToolbar;
    }

    // ========== Order Visualization Slots ==========

    /**
     * @brief Called when a new order is placed (pending state).
     * Creates a hollow triangle marker at the order price.
     */
    void onOrderPlaced(const Order& order);

    /**
     * @brief Called when an order is filled.
     * Converts pending marker to solid, updates position visualization.
     */
    void onOrderFilled(const Order& order);

    /**
     * @brief Called when an order is cancelled.
     * Converts pending marker to gray X.
     */
    void onOrderCancelled(const Order& order);

    /**
     * @brief Called when an order is amended (price changed).
     * Moves the marker to the new price.
     */
    void onOrderAmended(const Order& order);

    /**
     * @brief Called when a new position is opened.
     * Creates position visualization with entry marker.
     */
    void onPositionOpened(const Position& position);

    /**
     * @brief Called when a position is updated (quantity changed).
     * Updates position lines and P&L display.
     */
    void onPositionUpdated(const Position& position);

    /**
     * @brief Called when a position is closed (quantity reaches 0).
     * Finalizes position lines and adds realized P&L label.
     */
    void onPositionClosed(const Position& position);

    /**
     * @brief Toggles visibility of all order visualizations.
     */
    void setOrderVisualizationsVisible(bool visible);

  public slots:
    void onReplayDataLoadFailed(const QString& errorMessage);

  private slots:
    void onAxisRangeChanged();
    void onVolumeChartVisibilityChanged(bool visible);
    void onVolumeAutoRescaleChanged(bool enabled);
    void onOrderVisualizationsVisibilityChanged(bool visible);
    void onReplayDayChanged(const QDate& date);
    void onReplayTimeChanged(const QTime& time);
    void onReplayTimeRangeQueryFinished();
    void updateCurrentTimeLine();

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

    // Current time vertical line and timer
    QCPItemLine* m_currentTimeLine;
    QTimer* m_timeLineTimer;

    // Chart watermark
    QCPItemText* m_symbolWatermark; // Stock symbol at center-top
    bool m_isReplayModeActive = false;
    static constexpr QColor NORMAL_BACKGROUND_COLOR{75, 75, 80};
    static constexpr QColor REPLAY_BACKGROUND_COLOR{60, 60, 75}; // Slightly bluer tint

    // Volume chart components
    QCPAxisRect* m_volumeAxisRect;
    QCPBars* m_volumePos;
    QCPBars* m_volumeNeg;

    // Background rectangles for different market sessions
    QList<QCPItemRect*> m_earlyPreMarketRects;
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

    /// True after first batch of historical bars sets Y-axis range (prevents resetting on subsequent loads)
    bool m_initialYAxisRangeSet = false;

    // ========== Order Visualization Members ==========

    // Storage for order markers by orderID
    QMap<QString, OrderMarker*> m_orderMarkers;

    // Storage for position visualizations by positionID
    QMap<QString, PositionVisualization*> m_positionVisualizations;

    // Current open position for the displayed symbol (nullptr if none)
    PositionVisualization* m_currentOpenPosition = nullptr;

    // P&L box for open position (top-right corner)
    QCPItemText* m_openPositionPLBox = nullptr;

    // Whether order visualizations are visible (toggle support)
    bool m_orderVisualizationsVisible = true;

    // Colors for order visualization
    static constexpr QColor ORDER_VIZ_GREEN{0, 255, 100};  // #00FF64 - Bright lime green
    static constexpr QColor ORDER_VIZ_RED{255, 80, 0};     // #FF5000 - Bright orange-red
    static constexpr QColor ORDER_VIZ_GRAY{128, 128, 128}; // Gray for cancelled
    static constexpr int ORDER_VIZ_LINE_WIDTH = 2;         // Line thickness
    static constexpr int ORDER_VIZ_MARKER_SIZE = 8;        // Marker size in pixels
    static constexpr int ORDER_VIZ_PL_FONT_SIZE = 9;       // P&L label font size

    // Index clamping constants for marker positioning
    static constexpr double FIRST_CANDLE_WINDOW = -1.0; // Allow markers within first candle
    static constexpr int FUTURE_BAR_TOLERANCE = 10;     // Allow markers slightly ahead of current bar

    // ========== Order Visualization Helper Methods ==========

    /**
     * @brief Gets exact fractional index for a timestamp (sub-candle precision).
     * @param timestamp The timestamp to convert
     * @return Fractional index (e.g., 2.35 = 35% through bar at index 2)
     */
    double getExactIndexForTimestamp(const QDateTime& timestamp) const;

    /**
     * @brief Creates an order marker (common implementation for buy/sell).
     * @param orderID Unique order identifier
     * @param index Chart X-axis position (can be fractional)
     * @param price Chart Y-axis position
     * @param isBuy true for buy (upward triangle), false for sell (downward triangle)
     * @param filled Whether the order is filled (solid) or pending (lighter color)
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createOrderMarker(const QString& orderID, double index, double price, bool isBuy, bool filled);

    /**
     * @brief Creates a buy marker (upward triangle) at the specified position.
     * @param orderID Unique order identifier
     * @param index Chart X-axis position (can be fractional)
     * @param price Chart Y-axis position
     * @param filled Whether the order is filled (solid) or pending (hollow)
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createBuyMarker(const QString& orderID, double index, double price, bool filled);

    /**
     * @brief Creates a sell marker (downward triangle) at the specified position.
     * @param orderID Unique order identifier
     * @param index Chart X-axis position (can be fractional)
     * @param price Chart Y-axis position
     * @param filled Whether the order is filled (solid) or pending (hollow)
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createSellMarker(const QString& orderID, double index, double price, bool filled);

    /**
     * @brief Creates a cancelled/rejected marker (gray X) at the specified position.
     * @param orderID Unique order identifier
     * @param index Chart X-axis position
     * @param price Chart Y-axis position
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createCancelledMarker(const QString& orderID, double index, double price);

    /**
     * @brief Updates an existing marker to a new state.
     * @param marker The marker to update
     * @param newState The new state (Pending, Filled, Cancelled)
     */
    void updateMarkerState(OrderMarker* marker, OrderMarker::State newState);

    /**
     * @brief Moves an existing marker to a new price (for order amendments).
     * @param marker The marker to move
     * @param newPrice The new price position
     */
    void moveMarkerToPrice(OrderMarker* marker, double newPrice);

    /**
     * @brief Removes and deletes an order marker.
     * @param orderID The order ID of the marker to remove
     */
    void removeOrderMarker(const QString& orderID);

    /**
     * @brief Clamps index to valid range for marker positioning.
     * @param index The calculated index from timestamp
     * @param barCount Number of bars currently loaded in chart
     * @return Clamped index within valid range
     */
    double clampIndexToValidRange(double index, int barCount) const;

    /**
     * @brief Creates a dotted line connecting two points.
     * @param x1 Start X position
     * @param y1 Start Y position
     * @param x2 End X position
     * @param y2 End Y position
     * @param profitable Whether the line represents a profitable trade (green) or loss (red)
     * @return Pointer to the created line
     */
    QCPItemLine* createPositionLine(double x1, double y1, double x2, double y2, bool profitable);

    /**
     * @brief Updates the dynamic line for an open position to the current price.
     * @param currentPrice The current stock price
     * @param currentIndex The current time index
     */
    void updateOpenPositionDynamicLine(double currentPrice, double currentIndex);

    /**
     * @brief Updates the P&L box display for the open position.
     * @param currentPrice The current stock price
     */
    void updateOpenPositionPLBox(double currentPrice);

    /**
     * @brief Creates or updates the P&L box for open positions.
     */
    void ensureOpenPositionPLBox();

    /**
     * @brief Hides the P&L box for open positions.
     */
    void hideOpenPositionPLBox();

    /**
     * @brief Creates a P&L label for a closed position.
     * @param posViz The position visualization to add the label to
     */
    void createClosedPositionPLLabel(PositionVisualization* posViz);

    /**
     * @brief Calculates the DCA (dollar cost average) entry price.
     * @param posViz The position visualization
     * @return The average entry price
     */
    double calculateDCAPrice(const PositionVisualization* posViz) const;

    /**
     * @brief Finalizes a position when it closes (shares reach 0).
     * @param posViz The position visualization to finalize
     */
    void finalizeClosedPosition(PositionVisualization* posViz);

    /**
     * @brief Clears all order visualizations (markers, lines, labels).
     * Called when symbol changes or chart is cleared.
     */
    void clearOrderVisualizations();

    /**
     * @brief Loads historical orders for the current symbol from database.
     */
    void loadHistoricalOrders();

    /**
     * @brief Loads historical positions for the current symbol from database.
     */
    void loadHistoricalPositions();

    /**
     * @brief Updates visibility of all order visualization elements.
     */
    void updateOrderVisualizationsVisibility();

    /**
     * @brief Culls (hides) markers and lines outside visible range for performance.
     */
    void cullOrderVisualizationsToVisibleRange();
};
