# StockPriceChart Class Diagram

```mermaid
classDiagram
    class StockPriceChart {
        -QString symbol
        -QChart* chart
        -QLineSeries* lastPriceLine
        -QCandlestickSeries* candlestickSeries
        -QScatterSeries* voidBarSeries
        -QChartView* chartView
        -QDateTimeAxis* axisX
        -QValueAxis* axisY
        -QGraphicsTextItem* priceLabel
        -QList<QGraphicsRectItem*> afterHoursRects
        -QList<QGraphicsRectItem*> preMarketRects
        -QList<QGraphicsRectItem*> closedMarketRects
        -Bar currentOpenBar
        -bool hasOpenBar
        -double lastPrice
        -double lastValidClosePrice
        -QMap<QDateTime, Bar> completedBars
        -bool isPanning
        -QPoint lastMousePos
        -bool currentGetBarsRequestInProcess

        +StockPriceChart(QWidget* parent = nullptr)
        +~StockPriceChart()
        +setSymbol(QString symbol)
        +clearSymbol()
        +addBar(Bar bar)
        +onRequestedMissingBarsReceived(QVector<Bar> bars)

        #updateChart()
        #handleClosedBar(Bar bar)
        #handleOpenBar(Bar bar)
        #updateLastPriceLine(double price, bool isUpTick)
        #updatePriceLabelPosition()
        #isAfterMarketHours(QDateTime localTime)
        #updateAfterHoursBackground()
        #maintainBarLimit()
        #handleVerticalPanning(QWheelEvent* event)
        #handleHorizontalPanning(QWheelEvent* event)
        #handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor)
        #handleVerticalZoom(QWheelEvent* event, qreal zoomFactor)
        #handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor)
        #updateLastPriceLineIfNeeded()
        #handlePanning(QMouseEvent* mouseEvent)
        #checkForMissingBars(QDateTime viewStartTime, QDateTime viewEndTime)
        #createBackgroundRect(QColor color, int zValue)
        #clearBackgroundRects()

        #resizeEvent(QResizeEvent* event)
        #wheelEvent(QWheelEvent* event)
        #eventFilter(QObject* object, QEvent* event)

        #requestMissingBars(QDateTime viewStartTimeRounded, QDateTime firstBarTime)

        MAX_BARS: 1000
        CANDLESTICK_BODY_WIDTH: 0.9
    }

    class QWidget {
        <<Qt Base Class>>
    }

    class Bar {
        <<External Class>>
        -QDateTime timestamp
        -double open
        -double high
        -double low
        -double close
        -BarStatus status

        +getTimeStamp() QDateTime
        +getHigh() double
        +getLow() double
        +getClose() double
        +getBarStatus() BarStatus
        +isValid() bool
    }

    StockPriceChart --> QWidget : inherits
    StockPriceChart --> Bar : uses

    note for StockPriceChart "Qt-based stock price chart widget with candlestick visualization, interactive zooming/panning, market hours background highlighting, last price line tracking, void bar markers, and performance optimization (max 1000 bars)"
```

## Key Features

- **Data Visualization**: Displays stock price data using candlestick charts
- **Interactivity**: Supports mouse panning and wheel-based zooming (horizontal, vertical, combined)
- **Market Hours**: Visual background indicators for pre-market, regular hours, after-hours, and closed periods
- **Real-time Updates**: Handles both completed bars and open (in-progress) bars
- **Performance**: Maintains a rolling window of maximum 1000 bars
- **Signals**: Emits requests for missing historical data when zooming out beyond available data

## Qt Components Used

- `QChart` and `QChartView` for the main charting framework
- `QCandlestickSeries` for price visualization
- `QLineSeries` for last price line
- `QScatterSeries` for void bar markers
- `QDateTimeAxis` and `QValueAxis` for time and price axes
- `QGraphicsTextItem` and `QGraphicsRectItem` for overlays and backgrounds