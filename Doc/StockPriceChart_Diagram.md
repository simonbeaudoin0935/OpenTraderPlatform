# StockPriceChart Architecture

This document provides a comprehensive overview of the StockPriceChart component architecture using Mermaid diagrams.

## Table of Contents
- [Class Diagram](#class-diagram)
- [Data Flow Sequence](#data-flow-sequence)
- [User Interaction Flow](#user-interaction-flow)
- [Chart Update State Machine](#chart-update-state-machine)
- [Component Interaction Architecture](#component-interaction-architecture)
- [Bar Processing Pipeline](#bar-processing-pipeline)
- [Zoom and Pan Interaction Matrix](#zoom-and-pan-interaction-matrix)
- [Market Hours Background System](#market-hours-background-system)
- [Event Handling Architecture](#event-handling-architecture)
- [Rendering Pipeline](#rendering-pipeline)
- [Data Synchronization Flow](#data-synchronization-flow)
- [Memory Management Strategy](#memory-management-strategy)
- [Thread Safety and Signal Flow](#thread-safety-and-signal-flow)
- [Error Handling and Edge Cases](#error-handling-and-edge-cases)
- [Key Features and Implementation Details](#key-features-and-implementation-details)
- [Dependencies](#dependencies)

## Class Diagram

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
        -QList~QGraphicsRectItem*~ afterHoursRects
        -QList~QGraphicsRectItem*~ preMarketRects
        -QList~QGraphicsRectItem*~ closedMarketRects
        -Bar currentOpenBar
        -bool hasOpenBar
        -double lastPrice
        -double lastValidClosePrice
        -QMap~QDateTime, Bar~ completedBars
        -bool isPanning
        -QPoint lastMousePos
        -bool currentGetBarsRequestInProcess
        +setSymbol(QString)
        +clearSymbol()
        +addBar(Bar)
        +onRequestedMissingBarsReceived(QVector~Bar~)
        +requestMissingBars(QDateTime, QDateTime)
        #resizeEvent(QResizeEvent*)
        #wheelEvent(QWheelEvent*)
        #eventFilter(QObject*, QEvent*)
        -updateChart()
        -handleClosedBar(Bar)
        -handleOpenBar(Bar)
        -updateLastPriceLine(double, bool)
        -updatePriceLabelPosition()
        -isAfterMarketHours(QDateTime)
        -updateAfterHoursBackground()
        -maintainBarLimit()
        -handleVerticalPanning(QWheelEvent*)
        -handleHorizontalPanning(QWheelEvent*)
        -handleHorizontalZoom(QWheelEvent*, qreal)
        -handleVerticalZoom(QWheelEvent*, qreal)
        -handleBothAxesZoom(QWheelEvent*, qreal)
        -updateLastPriceLineIfNeeded()
        -handlePanning(QMouseEvent*)
        -checkForMissingBars(QDateTime, QDateTime)
        -createBackgroundRect(QColor, int)
        -clearBackgroundRects()
    }

    class QWidget {
        <<Qt>>
    }

    class Bar {
        <<enum>> BarStatus
        <<enum>> BarUnit
        <<enum>> BarSessionTemplate
        +getHigh() double
        +getLow() double
        +getOpen() double
        +getClose() double
        +getTimeStamp() QDateTime
        +getTotalVolume() quint64
        +getBarStatus() BarStatus
        +isValid() bool
    }

    class GuiFrontend {
        +onRequestedMissingBarsDisplayedStockReceived(QVector~Bar~)
    }

    class AppFrontend {
        <<interface>>
        +requestMissingBars(QDateTime, QDateTime)
    }

    class MarketHours {
        <<utility>>
        +isPreMarket(QDateTime) bool
        +isAfterHours(QDateTime) bool
        +isRegularHours(QDateTime) bool
    }

    QWidget <|-- StockPriceChart
    StockPriceChart --> Bar : uses
    StockPriceChart --> MarketHours : uses
    GuiFrontend --> StockPriceChart : owns
    AppFrontend <|-- GuiFrontend
    StockPriceChart --> AppFrontend : signals
```

## Data Flow Sequence

```mermaid
sequenceDiagram
    participant MainAlgo
    participant GuiFrontend
    participant StockPriceChart
    participant BarCache
    participant QChart

    Note over MainAlgo,QChart: Real-time Bar Updates
    MainAlgo->>GuiFrontend: onCurrentHighlightedStockBarReceived(symbol, bar)
    GuiFrontend->>StockPriceChart: addBar(bar)
    
    alt Bar is Closed
        StockPriceChart->>StockPriceChart: handleClosedBar(bar)
        StockPriceChart->>StockPriceChart: Insert into completedBars
        StockPriceChart->>StockPriceChart: maintainBarLimit()
    else Bar is Open
        StockPriceChart->>StockPriceChart: handleOpenBar(bar)
        StockPriceChart->>StockPriceChart: Store as currentOpenBar
        StockPriceChart->>StockPriceChart: updateLastPriceLine(price, isUpTick)
    end
    
    StockPriceChart->>StockPriceChart: updateChart()
    StockPriceChart->>QChart: Render candlesticks
    StockPriceChart->>StockPriceChart: updateAfterHoursBackground()

    Note over MainAlgo,QChart: Historical Bar Request (Pan/Zoom)
    StockPriceChart->>StockPriceChart: checkForMissingBars(viewStart, viewEnd)
    StockPriceChart->>GuiFrontend: requestMissingBars(startTime, firstBarTime)
    GuiFrontend->>MainAlgo: Forward signal
    MainAlgo->>BarCache: Request bars for time range
    BarCache-->>MainAlgo: Return QVector<Bar>
    MainAlgo->>GuiFrontend: onRequestedMissingBarsDisplayedStockReceived(bars)
    GuiFrontend->>StockPriceChart: onRequestedMissingBarsReceived(bars)
    StockPriceChart->>StockPriceChart: Insert bars into completedBars
    StockPriceChart->>StockPriceChart: updateChart()
```

## User Interaction Flow

```mermaid
stateDiagram-v2
    [*] --> Idle : Chart Initialized
    
    Idle --> MouseWheelZoom : Wheel Event
    Idle --> MousePanning : Left Click + Drag
    Idle --> RightClickRecenter : Right Click
    
    MouseWheelZoom --> CheckModifiers : Process Event
    CheckModifiers --> HorizontalZoom : Ctrl pressed
    CheckModifiers --> VerticalZoom : Shift pressed
    CheckModifiers --> BothAxesZoom : No modifiers
    CheckModifiers --> HorizontalPan : Alt pressed
    CheckModifiers --> VerticalPan : Shift + Ctrl
    
    HorizontalZoom --> UpdateAxes
    VerticalZoom --> UpdateAxes
    BothAxesZoom --> UpdateAxes
    HorizontalPan --> UpdateAxes
    VerticalPan --> UpdateAxes
    
    UpdateAxes --> CheckMissingBars : View range changed
    CheckMissingBars --> RequestBars : Bars missing
    CheckMissingBars --> UpdateVisuals : Bars available
    RequestBars --> UpdateVisuals
    UpdateVisuals --> Idle
    
    MousePanning --> DragUpdate : Mouse Move
    DragUpdate --> CalculateOffset : Convert pixels to units
    CalculateOffset --> UpdateAxes
    DragUpdate --> MouseRelease : Left Release
    MouseRelease --> Idle
    
    RightClickRecenter --> CalculateDefault : Get current bar time
    CalculateDefault --> SetDefaultRange : 30min window
    SetDefaultRange --> UpdateVisuals
```

## Chart Update State Machine

```mermaid
stateDiagram-v2
    [*] --> Empty : Chart Created
    
    Empty --> FirstBar : addBar(bar)
    FirstBar --> InitialView : Set 30min window
    
    InitialView --> HasData
    
    HasData --> UpdatingOpen : Bar.Status == Open
    HasData --> UpdatingClosed : Bar.Status == Closed
    
    UpdatingOpen --> StoringOpenBar : hasOpenBar == false
    UpdatingOpen --> ReplacingOpenBar : hasOpenBar == true
    
    StoringOpenBar --> UpdateChart
    ReplacingOpenBar --> MoveToCompleted : Different timestamp
    MoveToCompleted --> UpdateChart
    
    UpdatingClosed --> StoreCompleted : Add to completedBars
    StoreCompleted --> EnforceLimitCheck
    EnforceLimitCheck --> RemoveOldest : Size > MAX_BARS (1000)
    EnforceLimitCheck --> UpdateChart : Size <= MAX_BARS
    RemoveOldest --> UpdateChart
    
    UpdateChart --> RenderCandlesticks
    RenderCandlesticks --> UpdateLastPriceLine
    UpdateLastPriceLine --> UpdateBackgrounds
    UpdateBackgrounds --> UpdatePriceLabel
    UpdatePriceLabel --> HasData
    
    HasData --> [*] : clearSymbol()
```

## Component Interaction Architecture

```mermaid
graph TB
    subgraph "GUI Layer"
        GF[GuiFrontend]
        SPC[StockPriceChart]
        UI[UI Components]
    end
    
    subgraph "Chart Components"
        QCV[QChartView]
        QC[QChart]
        CS[QCandlestickSeries]
        LPL[Last Price Line]
        VBS[Void Bar Series]
        AX[QDateTimeAxis]
        AY[QValueAxis]
        PL[Price Label]
        BGR[Background Rects]
    end
    
    subgraph "Data Storage"
        CB[Completed Bars Map]
        OB[Current Open Bar]
    end
    
    subgraph "Business Logic"
        MA[MainAlgo]
        BC[BarCache]
        MH[MarketHours]
    end
    
    subgraph "External Data"
        TS[TradeStation API]
    end
    
    GF -->|owns| SPC
    SPC -->|contains| QCV
    QCV -->|displays| QC
    QC -->|series| CS
    QC -->|series| LPL
    QC -->|series| VBS
    QC -->|axis| AX
    QC -->|axis| AY
    QC -->|graphics item| PL
    QC -->|graphics item| BGR
    
    SPC -->|stores| CB
    SPC -->|stores| OB
    SPC -->|uses| MH
    
    MA -->|sends bars| GF
    GF -->|addBar| SPC
    SPC -->|requestMissingBars| GF
    GF -->|forward| MA
    MA -->|queries| BC
    MA -->|receives| TS
    
    style SPC fill:#4a90e2
    style QC fill:#50c878
    style MA fill:#ff6b6b
    style BC fill:#ffd93d
```

## Bar Processing Pipeline

```mermaid
flowchart TD
    Start([Bar Received]) --> Valid{Bar Valid?}
    Valid -->|No| End([Discard])
    Valid -->|Yes| CheckStatus{Bar Status?}
    
    CheckStatus -->|Open| HasOpen{Has Open Bar?}
    CheckStatus -->|Closed| HasOpen2{Has Open Bar?}
    CheckStatus -->|Void| AddVoid[Add to Void Series]
    
    HasOpen -->|No| StoreOpen[Store as currentOpenBar]
    HasOpen -->|Yes| CheckTime{Same Timestamp?}
    
    CheckTime -->|Yes| UpdateOpen[Update currentOpenBar]
    CheckTime -->|No| MoveComplete[Move old to completedBars]
    
    MoveComplete --> StoreOpen
    StoreOpen --> UpdatePrice[Update Last Price Line]
    UpdateOpen --> UpdatePrice
    UpdatePrice --> Render[Render Chart]
    
    HasOpen2 -->|No| StoreComplete[Add to completedBars]
    HasOpen2 -->|Yes| CheckTime2{Open = Closed Time?}
    
    CheckTime2 -->|Yes| DiscardOpen[Replace open with closed]
    CheckTime2 -->|No| MoveComplete2[Move open to completed]
    
    MoveComplete2 --> StoreComplete
    DiscardOpen --> StoreComplete
    StoreComplete --> Limit{Size > MAX_BARS?}
    
    Limit -->|Yes| RemoveOld[Remove Oldest Bar]
    Limit -->|No| Render
    RemoveOld --> Render
    
    AddVoid --> UseLastPrice[Position at lastValidClosePrice]
    UseLastPrice --> Render
    
    Render --> Background[Update Market Hours Background]
    Background --> End
    
    style Start fill:#90ee90
    style End fill:#ffb6c1
    style Valid fill:#ffd93d
    style CheckStatus fill:#ffd93d
    style Render fill:#4a90e2
```

## Zoom and Pan Interaction Matrix

```mermaid
graph LR
    subgraph "Mouse Wheel Events"
        WE[Wheel Event]
    end
    
    subgraph "Modifier Keys"
        None[No Modifiers]
        Ctrl[Ctrl Key]
        Shift[Shift Key]
        Alt[Alt Key]
        CS[Ctrl + Shift]
    end
    
    subgraph "Actions"
        BAZ[Both Axes Zoom]
        HZ[Horizontal Zoom]
        VZ[Vertical Zoom]
        HP[Horizontal Pan]
        VP[Vertical Pan]
    end
    
    subgraph "Results"
        TimeChange[Time Range Change]
        PriceChange[Price Range Change]
        CheckBars[Check Missing Bars]
        UpdateLabel[Update Price Label]
        UpdateBG[Update Backgrounds]
    end
    
    WE --> None
    WE --> Ctrl
    WE --> Shift
    WE --> Alt
    WE --> CS
    
    None --> BAZ
    Ctrl --> HZ
    Shift --> VZ
    Alt --> HP
    CS --> VP
    
    BAZ --> TimeChange
    BAZ --> PriceChange
    HZ --> TimeChange
    VZ --> PriceChange
    HP --> TimeChange
    VP --> PriceChange
    
    TimeChange --> CheckBars
    TimeChange --> UpdateLabel
    TimeChange --> UpdateBG
    PriceChange --> UpdateLabel
    PriceChange --> UpdateBG
    
    style WE fill:#90ee90
    style BAZ fill:#4a90e2
    style HZ fill:#4a90e2
    style VZ fill:#4a90e2
    style HP fill:#50c878
    style VP fill:#50c878
```

## Market Hours Background System

```mermaid
flowchart TD
    Start([updateAfterHoursBackground]) --> Clear[Clear existing rectangles]
    Clear --> GetRange[Get visible time range]
    GetRange --> ConvertNY[Convert to NY timezone]
    ConvertNY --> InitLoop[Initialize date loop]
    
    InitLoop --> LoopDate{For each date}
    LoopDate -->|Next| CheckWeekend{Is Weekend?}
    LoopDate -->|Done| End([Complete])
    
    CheckWeekend -->|Yes| DrawClosed[Draw closed market rect]
    CheckWeekend -->|No| LoopHours[Loop through hours 0-23]
    
    DrawClosed --> LoopDate
    
    LoopHours --> CheckHour{Check hour type}
    
    CheckHour -->|Pre-market| DrawPre[Draw pre-market rect<br/>Color: Brown tint<br/>Z-Value: -1]
    CheckHour -->|Regular hours| NextHour[Continue to next hour]
    CheckHour -->|After-hours| DrawAfter[Draw after-hours rect<br/>Color: Blue tint<br/>Z-Value: -1]
    CheckHour -->|Closed| DrawClosed2[Draw closed rect<br/>Color: Dark gray<br/>Z-Value: -2]
    
    DrawPre --> NextHour
    DrawAfter --> NextHour
    DrawClosed2 --> NextHour
    NextHour --> MoreHours{More hours?}
    
    MoreHours -->|Yes| LoopHours
    MoreHours -->|No| LoopDate
    
    style Start fill:#90ee90
    style End fill:#ffb6c1
    style DrawPre fill:#d2691e
    style DrawAfter fill:#4169e1
    style DrawClosed fill:#2f4f4f
    style DrawClosed2 fill:#2f4f4f
```

## Event Handling Architecture

```mermaid
flowchart TD
    subgraph "Event Sources"
        WE[Wheel Events]
        ME[Mouse Events]
        RE[Resize Events]
        AS[Axis Signals]
    end
    
    subgraph "Event Filter System"
        EF[eventFilter]
        VP[Viewport Filter]
    end
    
    subgraph "Event Handlers"
        WH[wheelEvent]
        MBP[MouseButtonPress]
        MBR[MouseButtonRelease]
        MM[MouseMove]
        RS[resizeEvent]
    end
    
    subgraph "Processing"
        CM[Check Modifiers]
        CP[Calculate Pan]
        CZ[Calculate Zoom]
        CR[Calculate Recenter]
    end
    
    subgraph "Actions"
        UA[Update Axes]
        UL[Update Label]
        UB[Update Background]
        CC[Check Cache]
        UCursor[Update Cursor]
    end
    
    WE --> WH
    ME --> VP
    RE --> RS
    AS --> UL
    AS --> UB
    
    VP --> EF
    EF --> MBP
    EF --> MBR
    EF --> MM
    
    WH --> CM
    MBP --> UCursor
    MBR --> UCursor
    MM --> CP
    
    CM --> CZ
    MBP --> CP
    
    CZ --> UA
    CP --> UA
    CR --> UA
    RS --> UL
    RS --> UB
    
    UA --> CC
    UA --> UL
    UA --> UB
    
    style EF fill:#ff6b6b
    style UA fill:#4a90e2
    style CC fill:#ffd93d
```

## Rendering Pipeline

```mermaid
flowchart LR
    subgraph "Data Preparation"
        CB[Completed Bars Map]
        OB[Current Open Bar]
        VB[Void Bars]
    end
    
    subgraph "Series Population"
        CCS[Clear Candlestick Series]
        ACB[Add Completed Bars]
        AOB[Add Open Bar]
        AVB[Add Void Markers]
    end
    
    subgraph "Axis Management"
        CVR[Check View Range]
        SXR[Set X Range]
        SYR[Set Y Range]
        CTR[Calculate Tick Range]
    end
    
    subgraph "Visual Elements"
        RC[Render Candlesticks]
        RPL[Render Price Line]
        RVM[Render Void Markers]
        RBG[Render Backgrounds]
        RLB[Render Label]
    end
    
    subgraph "Output"
        QCV[QChartView]
        SCN[QGraphicsScene]
    end
    
    CB --> CCS
    OB --> CCS
    VB --> CCS
    
    CCS --> ACB
    ACB --> AOB
    AOB --> AVB
    
    AVB --> CVR
    CVR --> SXR
    CVR --> SYR
    SXR --> CTR
    
    CTR --> RC
    RC --> RPL
    RPL --> RVM
    RVM --> RBG
    RBG --> RLB
    
    RLB --> QCV
    QCV --> SCN
    
    style CCS fill:#ff6b6b
    style CVR fill:#ffd93d
    style RC fill:#4a90e2
    style QCV fill:#50c878
```

## Data Synchronization Flow

```mermaid
sequenceDiagram
    participant User
    participant StockPriceChart
    participant completedBars
    participant currentOpenBar
    participant QChart
    participant MainAlgo
    
    Note over User,MainAlgo: Scenario 1: First Bar Received
    MainAlgo->>StockPriceChart: addBar(bar) [Open]
    StockPriceChart->>StockPriceChart: hasOpenBar = false
    StockPriceChart->>currentOpenBar: Store bar
    StockPriceChart->>StockPriceChart: hasOpenBar = true
    StockPriceChart->>QChart: updateChart()
    QChart-->>User: Display open bar
    
    Note over User,MainAlgo: Scenario 2: Same Bar Updated
    MainAlgo->>StockPriceChart: addBar(bar) [Open, same time]
    StockPriceChart->>currentOpenBar: Update values
    StockPriceChart->>QChart: updateChart()
    QChart-->>User: Display updated bar
    
    Note over User,MainAlgo: Scenario 3: New Bar Starts
    MainAlgo->>StockPriceChart: addBar(bar) [Open, new time]
    StockPriceChart->>completedBars: Move currentOpenBar
    StockPriceChart->>currentOpenBar: Store new bar
    StockPriceChart->>QChart: updateChart()
    QChart-->>User: Display both bars
    
    Note over User,MainAlgo: Scenario 4: Bar Closes
    MainAlgo->>StockPriceChart: addBar(bar) [Closed]
    StockPriceChart->>completedBars: Add closed bar
    StockPriceChart->>currentOpenBar: Clear if same time
    StockPriceChart->>StockPriceChart: hasOpenBar = false
    StockPriceChart->>StockPriceChart: maintainBarLimit()
    StockPriceChart->>QChart: updateChart()
    QChart-->>User: Display final bar
    
    Note over User,MainAlgo: Scenario 5: Historical Request
    User->>StockPriceChart: Pan/Zoom beyond data
    StockPriceChart->>StockPriceChart: checkForMissingBars()
    StockPriceChart->>MainAlgo: requestMissingBars signal
    MainAlgo->>MainAlgo: Query BarCache
    MainAlgo-->>StockPriceChart: onRequestedMissingBarsReceived()
    StockPriceChart->>completedBars: Insert historical bars
    StockPriceChart->>QChart: updateChart()
    QChart-->>User: Display extended view
```

## Memory Management Strategy

```mermaid
flowchart TD
    Start([New Bar Added]) --> CheckType{Bar Type?}
    
    CheckType -->|Open| StoreOpen[Store in currentOpenBar<br/>Size: 1 Bar<br/>Memory: ~200 bytes]
    CheckType -->|Closed| AddComplete[Add to completedBars]
    CheckType -->|Void| AddScatter[Add to voidBarSeries]
    
    AddComplete --> CheckSize{Size > MAX_BARS?}
    
    CheckSize -->|No| StoreMap[Store in QMap<br/>Current: N bars<br/>Memory: N * 200 bytes]
    CheckSize -->|Yes| RemoveOld[Remove Oldest Entry<br/>FIFO Policy]
    
    RemoveOld --> StoreMap
    
    StoreMap --> UpdateSeries{Update Series?}
    AddScatter --> UpdateSeries
    StoreOpen --> UpdateSeries
    
    UpdateSeries -->|Yes| ClearSeries[Clear QCandlestickSeries]
    UpdateSeries -->|No| End([Complete])
    
    ClearSeries --> CreateSets[Create New QCandlestickSet<br/>for each bar]
    CreateSets --> AttachSeries[Attach to Chart]
    AttachSeries --> OldCleanup[Qt Parent Cleanup<br/>Auto-delete old sets]
    OldCleanup --> End
    
    style RemoveOld fill:#ff6b6b
    style OldCleanup fill:#50c878
    style CheckSize fill:#ffd93d
```

## Thread Safety and Signal Flow

```mermaid
flowchart TD
    subgraph "Main Thread - GUI"
        SPC[StockPriceChart]
        GF[GuiFrontend]
        QCV[QChartView]
    end
    
    subgraph "Worker Thread - Data Processing"
        MA[MainAlgo]
        BC[BarCache]
        TS[TSClient]
    end
    
    subgraph "Qt Signal System"
        SC[Signal/Slot Connections]
        ED[Event Dispatcher]
    end
    
    TS -->|Bar Data| MA
    MA -->|Qt::DirectConnection| SC
    SC -->|currentHighlightedStockBarReceived| GF
    GF -->|Direct Call| SPC
    SPC -->|Update UI| QCV
    
    SPC -->|requestMissingBars signal| SC
    SC -->|Forward| GF
    GF -->|Forward| MA
    MA -->|Query| BC
    BC -->|Result| MA
    MA -->|Qt::DirectConnection| SC
    SC -->|onRequestedMissingBars| GF
    GF -->|Direct Call| SPC
    
    ED -.->|Dispatch| SC
    
    style MA fill:#ff6b6b
    style SPC fill:#4a90e2
    style SC fill:#ffd93d
```

## Error Handling and Edge Cases

```mermaid
flowchart TD
    Start([Input Event]) --> Validate{Validate Input}
    
    Validate -->|Invalid Bar| LogError1[qCritical - Invalid bar]
    Validate -->|Empty Vector| LogError2[qCCritical - Empty bars]
    Validate -->|Valid| Process[Process Event]
    
    Process --> CheckState{Check State}
    
    CheckState -->|Empty Bars| HandleEmpty[Skip calculations<br/>Return early]
    CheckState -->|Request In Progress| WaitRequest[Skip duplicate request]
    CheckState -->|Normal| Continue[Continue Processing]
    
    Continue --> CheckView{View Valid?}
    
    CheckView -->|Invalid Range| ResetView[Reset to default view]
    CheckView -->|Valid| UpdateView[Update View]
    
    UpdateView --> CheckAssert{Assertions}
    
    CheckAssert -->|Flag mismatch| Assert1[Q_ASSERT failure]
    CheckAssert -->|All OK| Success[Success]
    
    LogError1 --> End([End])
    LogError2 --> End
    HandleEmpty --> End
    WaitRequest --> End
    ResetView --> End
    Assert1 --> End
    Success --> End
    
    style LogError1 fill:#ff6b6b
    style LogError2 fill:#ff6b6b
    style Assert1 fill:#ff6b6b
    style Success fill:#50c878
```

## Key Features and Implementation Details

### 1. Real-time Updates
- **Open Bar Tracking**: Maintains `currentOpenBar` that updates in real-time
- **Price Line**: Dynamic horizontal line showing current price with color (green=up, red=down)
- **Price Label**: Floating label in right margin showing current price

### 2. Data Management
- **Circular Buffer**: Maintains maximum of 1000 bars (`MAX_BARS`)
- **Efficient Storage**: Uses `QMap<QDateTime, Bar>` for O(log n) lookups
- **Void Bar Handling**: Special markers for bars with no trading activity

### 3. Visualization
- **Candlestick Chart**: Green for price increase, red for decrease
- **Market Hours Overlay**: 
  - Pre-market: Brown tint (90, 60, 30, 100 RGBA)
  - After-hours: Blue tint (50, 50, 80, 100 RGBA)
  - Closed: Dark gray (40, 40, 50, 120 RGBA)
  - Weekend: Dark gray (deeper shade)
- **Dark Theme**: Consistent with application theme

### 4. Interaction Controls
- **Mouse Wheel**:
  - Scroll: Zoom both axes
  - Ctrl+Scroll: Zoom horizontally (time axis)
  - Shift+Scroll: Zoom vertically (price axis)
  - Alt+Scroll: Pan horizontally
  - Ctrl+Shift+Scroll: Pan vertically
- **Mouse Drag**: Left-click drag for pan in both directions
- **Right Click**: Reset to default 30-minute view centered on current bar
- **Keyboard**: 'i' to focus stock input (from GuiFrontend)

### 5. Performance Optimizations
- **View-based Rendering**: Only processes visible bars for min/max calculations using `QMap::lowerBound`
- **Request Throttling**: `currentGetBarsRequestInProcess` prevents duplicate requests
- **State Preservation**: Maintains view state (zoom/pan) during updates
- **Lazy Background Updates**: Only updates on range changes via axis signals
- **Z-Value Layering**: Backgrounds at -2 and -1, chart data at 0+

### 6. Smart Bar Fetching
- **Automatic Detection**: Detects when user pans/zooms beyond available data
- **Signal-based Request**: Emits `requestMissingBars` signal to MainAlgo via GuiFrontend
- **Asynchronous Loading**: Non-blocking bar retrieval from cache
- **Seamless Integration**: Automatically renders new bars when received
- **Request Guard**: Prevents multiple simultaneous requests with flag

## Configuration Constants

```cpp
#define CANDLESTICK_BODY_WIDTH 0.9  // 90% of available space
const int MAX_BARS = 1000;          // Maximum bars to display
const int DEFAULT_VIEW_MINUTES = 30; // Default time window
const int TIME_TICK_COUNT = 7;       // Ticks for 30-min view (every 5 min)
const double PRICE_PADDING = 0.0002; // 0.02% padding for Y axis
const double MIN_PRICE_RANGE = 0.0005; // 0.05% minimum range
const double ZOOM_FACTOR_IN = 0.9;   // Zoom in factor
const double ZOOM_FACTOR_OUT = 1.1;  // Zoom out factor
const double PAN_SHIFT_PERCENT = 0.05; // 5% shift for pan operations
```

## Dependencies

```mermaid
graph TD
    SPC[StockPriceChart] --> Qt[Qt 6 Framework]
    SPC --> Bar[Bar Class]
    SPC --> MH[MarketHours Utility]
    
    Qt --> Charts[QtCharts]
    Qt --> Core[QtCore]
    Qt --> GUI[QtGui]
    Qt --> Widgets[QtWidgets]
    
    Charts --> QChart
    Charts --> QCandlestickSeries
    Charts --> QLineSeries
    Charts --> QScatterSeries
    Charts --> QDateTimeAxis
    Charts --> QValueAxis
    
    Bar --> TS[TradeStation API]
    MH --> TZ[QTimeZone - America/New_York]
    
    SPC --> Logger[Q_LOGGING_CATEGORY - ChartLog]
    
    style SPC fill:#4a90e2
    style Qt fill:#50c878
    style Charts fill:#ff6b6b
```

## Performance Characteristics

| Operation | Time Complexity | Space Complexity | Notes |
|-----------|----------------|------------------|-------|
| Add Bar | O(log n) | O(1) | QMap insertion |
| Update Chart | O(n) | O(n) | n = visible bars only |
| Find Bar by Time | O(log n) | O(1) | QMap lookup |
| Maintain Bar Limit | O(1) amortized | O(1) | Removes oldest when > MAX_BARS |
| Zoom/Pan | O(m) | O(1) | m = bars in new view |
| Background Update | O(h × d) | O(h × d) | h = hours, d = days in view |
| Missing Bar Check | O(1) | O(1) | Simple time comparison |

Where:
- n = total bars in completedBars
- m = bars visible in current view
- h = hours visible in view
- d = days visible in view

## Qt Components Used

- **QChart** and **QChartView**: Main charting framework
- **QCandlestickSeries**: Price visualization
- **QLineSeries**: Last price line
- **QScatterSeries**: Void bar markers
- **QDateTimeAxis** and **QValueAxis**: Time and price axes
- **QGraphicsTextItem** and **QGraphicsRectItem**: Overlays and backgrounds
- **QMap**: Efficient bar storage with time-based lookups
- **Q_LOGGING_CATEGORY**: Debug logging (ChartLog category)