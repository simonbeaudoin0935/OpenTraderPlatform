#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QTimeZone>
#include <QMap>
#include <QLoggingCategory>
#include <QVBoxLayout>
#include <QTimer>
#include <atomic>
#include <functional>
#include <optional>

#include "qcustomplot.h"
#include "Bar.h"
#include "ChartToolbar.h"
#include "ChartTimeUtils.h"
#include "CONSTANTS.h"
#include "Misc/TimeFrame.h"
#include "QCPItemTriangle.h"
#include "QCPItemLogDot.h"
#include "OrdersDatabase.h"

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
    bool isBuy;   // true = buy/buytocover, false = sell/sellshort
    bool isEntry; // true = opening position (green), false = closing (red)
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
    // QCPItemTriangle for buy/sell markers, QCPItemText for cancelled/rejected
    QCPAbstractItem* markerItem = nullptr;
    bool isTriangleMarker = false; ///< true when markerItem is QCPItemTriangle
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
    void clearChart(bool p_replot = true);

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
     * @brief Snapshots the current X and Y axis ranges so that
     *        the next clearChart() + bar reload restores them instead of auto-scaling.
     * Call this immediately before clearChart() when switching timescale (not a full reset).
     */
    void preserveCurrentRanges();

    /**
     * @brief Updates the active display timescale and scales candlestick + volume bar widths.
     *
     * Must be called whenever the user switches timescale so that candles visually fill
     * their correct time slot.  Width = minutesPerBar(tf) × CANDLESTICK_BODY_WIDTH.
     * Only intraday timescales (1m–4h) are handled; daily+ are no-ops pending multi-day view.
     * Thread context: Called from Main/GUI thread.
     * @param tf New display timescale.
     */
    void setDisplayTimeFrame(TimeFrame tf);

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

    /**
     * @brief Called when a strategy emits a log-to-chart message.
     * Creates a log marker on the chart in real time.
     */
    void onStrategyLogEmitted(const StrategyLogEntry& entry);

  public slots:
    void onReplayDataLoadFailed(const QString& errorMessage);

  private slots:
    void onAxisRangeChanged();
    void onVolumeChartVisibilityChanged(bool visible);
    void onVolumeAutoRescaleChanged(bool enabled);
    void onOrderVisualizationsVisibilityChanged(bool visible);
    void onReplayDayChanged(const QDate& date);
    void onReplayTimeChanged(const QTime& time);
    void updateCurrentTimeLine();

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    // Track the current open bar
    Bar m_latestBar;
    int m_latestBarIndex = -1;

    // Index of the live (open) bar currently stored in the QCP data containers.
    // Used to do incremental updates (in-place mutation) instead of
    // rebuilding all N bars on every trade tick.
    int m_chartLiveBarIndex = -1;
    bool m_chartLiveBarIsUp = true; // tracks which volume series the live bar is in

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
    void initializeTimeAnchor();

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
    void drawHolidayDayMarker(const QDate& date, const QString& holidayName);
    void startLoadingSpinner();
    void stopLoadingSpinner();

    QString m_symbol;
    QCustomPlot* m_customPlot;
    QCPFinancial* m_candlesticks;
    QCPItemLine* m_lastPriceLine;
    QCPItemText* m_priceLabel;

    // Current time vertical line and timer
    QCPItemLine* m_currentTimeLine;
    QTimer* m_timeLineTimer;

    // Loading spinner shown while a missing-bars request is in flight
    QCPItemText* m_loadingSpinner; // Text item on the overlay layer
    QTimer* m_loadingSpinnerTimer; // Drives the animation frames
    int m_loadingSpinnerFrame = 0;

    // Debounce timer for onAxisRangeChanged — coalesces rapid successive calls
    // (e.g. both X and Y fire rangeChanged in a single wheel event)
    bool m_axisRangeChangePending = false;

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
    QSet<QDate> m_datesWithBackgrounds;  // Track which dates already have backgrounds drawn
    QList<QCPItemText*> m_holidayLabels; // Watermark text items for holidays, cleared with clearBackgroundRects()
    // The double associatives maps indexToBar and timestampToIndex are used to avoid caring about
    // the time when the market is
    //QList<QCPItemRect*> m_closedMarketRects;

    // Index-based positioning maps
    QMap<int, Bar> indexToBar;             // Map from index to Bar
    QMap<QDateTime, int> timestampToIndex; // Map from timestamp to index

    // Time-anchored chart index 0 (set on symbol selection, not on first bar receipt)
    QDateTime m_index0Timestamp;

    // Active display timescale — controls candlestick and volume bar widths
    TimeFrame m_displayTimeFrame = TimeFrame::ONE_MINUTE;

    // Timeframe selector widget
    ChartToolbar* chartToolbar;

    /// Request token for missing bars requests. Incremented on each new request.
    /// When a response arrives, it's only processed if its token matches m_currentMissingBarsRequestToken.
    /// This prevents stale responses (from cancelled requests due to rapid timescale switching) from being processed.
    std::atomic<uint64_t> m_currentMissingBarsRequestToken{0};

    /// Date of the most recently issued missing-bars request.
    /// Used in onRequestedMissingBarsFailed to record which date returned no data (holiday/non-trading day).
    QDate m_lastRequestedDate;

    /// Dates known to have no trading data (holidays, early closes with zero bars).
    /// Populated by onRequestedMissingBarsFailed; skipped when computing the previous trading day.
    /// Cleared on symbol change.
    QSet<QDate> m_knownEmptyDates;

    // Wheel zoom sensitivity ratio
    qreal wheelZoomRatio = 1.0;

    // Volume auto-rescale state
    bool m_volumeAutoRescaleEnabled = true;

    // Helper to convert index to time for axis labels
    QString indexToTimeString(double index) const;

    void checkAutoTimeFrame();

    bool startedReceivingRealtimeBars = false;

    /// True after first batch of historical bars sets Y-axis range (prevents resetting on subsequent loads)
    bool m_initialYAxisRangeSet = false;

    /// When set, the next initial Y-axis range computation is skipped and this range is used instead.
    /// Set by onReplayTimeChanged to preserve the user's zoom level across start-time changes.
    std::optional<QCPRange> m_preservedYRange;

    /// When set, the next X-axis range setup is skipped and this range is used instead.
    /// Set by preserveCurrentRanges() to preserve zoom level across timescale changes.
    std::optional<QCPRange> m_preservedXRange;

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

    // ========== Hover Tooltip ==========

    /**
     * @brief Associates a QCustomPlot item with a pixel-anchor getter + tooltip text.
     * Used in eventFilter to hit-test all tooltipped items on every mouse move.
     */
    struct HoverTarget
    {
        QCPAbstractItem* item;           ///< Owned by QCustomPlot; used as a live/removed guard
        std::function<QPointF()> getPos; ///< Returns current pixel coordinates of the item's anchor
        QString tooltip;
    };

    QVector<HoverTarget> m_hoverTargets;

    /**
     * @brief Register an item for hover tooltip hit-testing.
     */
    void registerTooltip(QCPAbstractItem* item, std::function<QPointF()> getPos, const QString& tooltip);

    /**
     * @brief Remove a previously registered item from tooltip hit-testing.
     */
    void unregisterTooltip(QCPAbstractItem* item);

    // ========== Log Markers ==========

    /** @brief Represents a strategy log marker on the chart. */
    struct LogMarker
    {
        int dbId = 0;
        QString symbol;
        QDateTime timestamp;
        QString message;
        QString strategyID;
        QCPItemLogDot* markerItem = nullptr; ///< Owned by QCustomPlot
    };

    QVector<LogMarker*> m_logMarkers;

    /**
     * @brief Create and add a single log marker to the chart.
     * @param entry The log entry to visualise
     * @return Pointer to the created LogMarker (owned by m_logMarkers)
     */
    LogMarker* createLogMarker(const StrategyLogEntry& entry);

    /**
     * @brief Load all strategy log entries for the current symbol from OrdersDatabase.
     * Must be called after bars are loaded.
     */
    void loadStrategyLogMarkers();

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
    OrderMarker*
    createOrderMarker(const QString& orderID, double index, double price, bool isBuy, bool isEntry, bool filled);

    /**
     * @brief Creates a buy marker (upward triangle) at the specified position.
     * @param orderID Unique order identifier
     * @param index Chart X-axis position (can be fractional)
     * @param price Chart Y-axis position
     * @param filled Whether the order is filled (solid) or pending (hollow)
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createBuyMarker(const QString& orderID, double index, double price, bool isEntry, bool filled);

    /**
     * @brief Creates a sell marker (downward triangle) at the specified position.
     * @param orderID Unique order identifier
     * @param index Chart X-axis position (can be fractional)
     * @param price Chart Y-axis position
     * @param filled Whether the order is filled (solid) or pending (hollow)
     * @return Pointer to the created OrderMarker
     */
    OrderMarker* createSellMarker(const QString& orderID, double index, double price, bool isEntry, bool filled);

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
