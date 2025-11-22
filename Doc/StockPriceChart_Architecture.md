# StockPriceChart Architecture Documentation

This document provides comprehensive documentation of the StockPriceChart implementation to assist in debugging crashes and understanding the chart mechanics, particularly related to zoom and drag operations.

## Table of Contents
1. [Overview](#overview)
2. [Class Architecture](#class-architecture)
3. [Data Flow](#data-flow)
4. [Event Handling](#event-handling)
5. [State Management](#state-management)
6. [Missing Bars Detection](#missing-bars-detection)
7. [View State Preservation](#view-state-preservation)
8. [Potential Crash Points](#potential-crash-points)

---

## Overview

The `StockPriceChart` is a Qt-based widget that displays real-time stock price data using candlestick charts. It provides interactive features including:
- Zoom (horizontal, vertical, both axes)
- Pan (mouse drag and keyboard modifiers)
- Real-time price updates
- Market hours visualization (pre-market, regular hours, after-hours, closed)
- Dynamic bar loading when viewing historical data

---

## Class Architecture

### Class Diagram

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
        -QMap~QDateTime, Bar~ completedBars
        -Bar currentOpenBar
        -bool hasOpenBar
        -double lastPrice
        -double lastValidClosePrice
        -bool isPanning
        -QPoint lastMousePos
        -bool currentGetBarsRequestInProcess
        
        +setSymbol(QString)
        +clearSymbol()
        +addBar(Bar)
        +onRequestedMissingBarsReceived(QVector~Bar~)
        -updateChart()
        -handleClosedBar(Bar)
        -handleOpenBar(Bar)
        -updateLastPriceLine(double, bool)
        -checkForMissingBars(QDateTime, QDateTime)
        -handlePanning(QMouseEvent*)
        -handleHorizontalZoom(QWheelEvent*, qreal)
        -handleVerticalZoom(QWheelEvent*, qreal)
        -handleBothAxesZoom(QWheelEvent*, qreal)
    }
    
    class QWidget {
        <<Qt Framework>>
    }
    
    class Bar {
        +enum BarStatus
        -double high
        -double low
        -double open
        -double close
        -QDateTime timeStamp
        -quint64 totalVolume
        -BarStatus barStatus
        
        +getHigh() double
        +getLow() double
        +getOpen() double
        +getClose() double
        +getTimeStamp() QDateTime
        +getBarStatus() BarStatus
        +isValid() bool
    }
    
    class MarketHours {
        <<Utility>>
        +enum Session
        +isPreMarket(QDateTime) bool
        +isRegularHours(QDateTime) bool
        +isAfterHours(QDateTime) bool
    }
    
    class GUIFrontend {
        -Ui::GUIFrontend* ui
        +onRequestedMissingBarsDisplayedStockReceived(QVector~Bar~)
    }
    
    class AppFrontend {
        <<Interface>>
    }
    
    StockPriceChart --|> QWidget
    StockPriceChart --> Bar : uses
    StockPriceChart --> MarketHours : uses
    GUIFrontend --> StockPriceChart : contains
    GUIFrontend --|> AppFrontend
    StockPriceChart --> GUIFrontend : signals requestMissingBars
```

### Component Dependencies

```mermaid
graph TB
    subgraph "Qt Charts Framework"
        QChart[QChart]
        QCandlestickSeries[QCandlestickSeries]
        QLineSeries[QLineSeries]
        QScatterSeries[QScatterSeries]
        QDateTimeAxis[QDateTimeAxis]
        QValueAxis[QValueAxis]
        QChartView[QChartView]
    end
    
    subgraph "StockPriceChart"
        Chart[chart]
        CandleSeries[candlestickSeries]
        LastPrice[lastPriceLine]
        VoidBars[voidBarSeries]
        AxisX[axisX]
        AxisY[axisY]
        ChartView[chartView]
        CompletedBars[completedBars Map]
        CurrentOpen[currentOpenBar]
    end
    
    subgraph "Data Sources"
        TSClient[TradeStation Client]
        BarCache[Bar Cache]
    end
    
    QChart -.-> Chart
    QCandlestickSeries -.-> CandleSeries
    QLineSeries -.-> LastPrice
    QScatterSeries -.-> VoidBars
    QDateTimeAxis -.-> AxisX
    QValueAxis -.-> AxisY
    QChartView -.-> ChartView
    
    Chart --> CandleSeries
    Chart --> LastPrice
    Chart --> VoidBars
    Chart --> AxisX
    Chart --> AxisY
    ChartView --> Chart
    
    TSClient --> CurrentOpen
    BarCache --> CompletedBars
```

---

## Data Flow

### Bar Addition Flow

This diagram shows how bars flow from external sources into the chart display:

```mermaid
sequenceDiagram
    participant TS as TradeStation/Cache
    participant GF as GUIFrontend
    participant SPC as StockPriceChart
    participant Chart as Qt Chart Components
    
    alt Real-time Bar Update
        TS->>GF: New Bar (Open/Closed)
        GF->>SPC: addBar(bar)
        
        alt Bar Status = Closed
            SPC->>SPC: handleClosedBar(bar)
            SPC->>SPC: Store in completedBars map
            SPC->>SPC: Clear hasOpenBar flag
        else Bar Status = Open
            SPC->>SPC: handleOpenBar(bar)
            SPC->>SPC: Update currentOpenBar
            SPC->>SPC: Set hasOpenBar = true
        end
        
        SPC->>SPC: updateChart()
        SPC->>Chart: Clear and rebuild candlesticks
        SPC->>Chart: Add all completedBars
        alt hasOpenBar
            SPC->>Chart: Add currentOpenBar
        end
        SPC->>SPC: updateLastPriceLine()
    end
    
    alt Missing Bars Request
        SPC->>GF: emit requestMissingBars(startTime, endTime)
        GF->>TS: Request bars from cache
        TS->>GF: Return QVector<Bar>
        GF->>SPC: onRequestedMissingBarsReceived(bars)
        
        Note over SPC: Assert: currentGetBarsRequestInProcess == true
        
        SPC->>SPC: Reset currentGetBarsRequestInProcess = false
        loop For each bar in bars
            alt Bar Status != Void
                SPC->>SPC: Insert into completedBars
                SPC->>SPC: Update lastValidClosePrice
            else Bar Status = Void
                SPC->>Chart: Add to voidBarSeries
            end
        end
        SPC->>SPC: maintainBarLimit()
        SPC->>SPC: updateChart()
        SPC->>SPC: updateAfterHoursBackground()
    end
```

### Bar Data Structure Flow

```mermaid
graph LR
    subgraph "External Data"
        RT[Real-time Stream]
        HIST[Historical Cache]
    end
    
    subgraph "Bar States"
        OPEN[Open Bar<br/>hasOpenBar=true<br/>currentOpenBar]
        CLOSED[Closed Bar<br/>completedBars Map]
        VOID[Void Bar<br/>voidBarSeries]
    end
    
    subgraph "Display"
        CANDLE[Candlestick Series]
        SCATTER[Scatter Series X]
    end
    
    RT -->|BarStatus::Open| OPEN
    RT -->|BarStatus::Closed| CLOSED
    HIST -->|BarStatus::Closed| CLOSED
    HIST -->|BarStatus::Void| VOID
    
    OPEN -->|updateChart| CANDLE
    CLOSED -->|updateChart| CANDLE
    VOID -->|append| SCATTER
    
    OPEN -.->|Bar closes| CLOSED
```

---

## Event Handling

### Mouse and Wheel Event Processing

```mermaid
graph TD
    Start[Event Received] --> EventType{Event Type?}
    
    EventType -->|wheelEvent| WheelMod{Modifiers?}
    EventType -->|MousePress| MouseBtn{Button?}
    EventType -->|MouseMove| IsPan{isPanning?}
    EventType -->|MouseRelease| RelBtn{Left Button?}
    
    WheelMod -->|Shift+Ctrl| VertPan[handleVerticalPanning]
    WheelMod -->|Alt| HorizPan[handleHorizontalPanning]
    WheelMod -->|Ctrl| HorizZoom[handleHorizontalZoom]
    WheelMod -->|Shift| VertZoom[handleVerticalZoom]
    WheelMod -->|None| BothZoom[handleBothAxesZoom]
    
    MouseBtn -->|Right Click| RightClick[Reset View<br/>Recenter to latest bar<br/>30min window]
    MouseBtn -->|Left Click| StartPan[isPanning = true<br/>Save lastMousePos<br/>Set ClosedHand cursor]
    
    IsPan -->|Yes| HandlePan[handlePanning<br/>Calculate delta<br/>Update axes<br/>checkForMissingBars]
    IsPan -->|No| Ignore1[Ignore event]
    
    RelBtn -->|Yes & isPanning| EndPan[isPanning = false<br/>Reset cursor<br/>updateLastPriceLine]
    RelBtn -->|No| Ignore2[Ignore event]
    
    VertPan --> UpdateLabel1[updatePriceLabelPosition]
    HorizPan --> UpdateLabel1
    HorizZoom --> UpdateLabel1
    VertZoom --> UpdateLabel1
    BothZoom --> CheckMissing[checkForMissingBars]
    CheckMissing --> UpdateLabel1
    RightClick --> UpdateBG1[updateAfterHoursBackground<br/>updateLastPriceLineIfNeeded]
    StartPan --> End[Event Handled]
    HandlePan --> End
    EndPan --> End
    UpdateLabel1 --> End
    UpdateBG1 --> End
    Ignore1 --> End
    Ignore2 --> End
```

### Zoom Operations Detail

```mermaid
sequenceDiagram
    participant User
    participant WE as wheelEvent
    participant SPC as StockPriceChart
    participant Axis as Axes (X/Y)
    
    User->>WE: Mouse Wheel + Modifiers
    WE->>WE: Calculate zoomFactor<br/>(0.9 for zoom in, 1.1 for zoom out)
    
    alt Horizontal Zoom Only (Ctrl)
        WE->>SPC: handleHorizontalZoom(event, zoomFactor)
        SPC->>Axis: Get current X range (min, max)
        SPC->>SPC: Calculate time range and center
        SPC->>SPC: newTimeRange = timeRange * zoomFactor
        SPC->>SPC: Calculate new min/max around center
        SPC->>Axis: axisX->setRange(newMinTime, newMaxTime)
        SPC->>SPC: updateLastPriceLineIfNeeded()
    end
    
    alt Vertical Zoom Only (Shift)
        WE->>SPC: handleVerticalZoom(event, zoomFactor)
        SPC->>Axis: Get current Y range (min, max)
        SPC->>SPC: Calculate price range and center
        SPC->>SPC: newRange = range * zoomFactor
        SPC->>SPC: Calculate new min/max around center
        SPC->>Axis: axisY->setRange(newMin, newMax)
        SPC->>SPC: updateLastPriceLineIfNeeded()
    end
    
    alt Both Axes Zoom (No modifiers)
        WE->>SPC: handleBothAxesZoom(event, zoomFactor)
        SPC->>Axis: Get current X and Y ranges
        SPC->>SPC: Calculate new time range (zoom X)
        SPC->>SPC: Calculate new price range (zoom Y)
        SPC->>Axis: axisX->setRange(newMinTime, newMaxTime)
        SPC->>Axis: axisY->setRange(newMinPrice, newMaxPrice)
        SPC->>SPC: checkForMissingBars(newMinTime, newMaxTime)
        SPC->>SPC: updateLastPriceLineIfNeeded()
    end
    
    SPC->>SPC: updatePriceLabelPosition()
```

### Pan Operations Detail

```mermaid
sequenceDiagram
    participant User
    participant ME as MouseEvent
    participant SPC as StockPriceChart
    participant Axis as Axes (X/Y)
    
    User->>ME: Left Click + Drag
    
    alt Mouse Press
        ME->>SPC: MouseButtonPress (Left)
        SPC->>SPC: isPanning = true
        SPC->>SPC: Save lastMousePos
        SPC->>SPC: Set ClosedHandCursor
    end
    
    alt Mouse Move (while panning)
        ME->>SPC: MouseMove event
        SPC->>SPC: handlePanning(mouseEvent)
        SPC->>SPC: Calculate delta = currentPos - lastMousePos
        SPC->>SPC: Update lastMousePos
        
        Note over SPC: Convert pixel movement to units
        SPC->>SPC: timePerPixel = timeRange / chartView->width()
        SPC->>SPC: timeOffset = -delta.x * timePerPixel
        SPC->>SPC: pricePerPixel = priceRange / chartView->height()
        SPC->>SPC: priceOffset = delta.y * pricePerPixel
        
        SPC->>Axis: Calculate new X range with timeOffset
        SPC->>Axis: Calculate new Y range with priceOffset
        SPC->>Axis: axisX->setRange(newMinTime, newMaxTime)
        SPC->>Axis: axisY->setRange(newMinPrice, newMaxPrice)
        
        SPC->>SPC: checkForMissingBars(newMinTime, newMaxTime)
        SPC->>SPC: updatePriceLabelPosition()
        SPC->>SPC: updateLastPriceLine()
    end
    
    alt Mouse Release
        ME->>SPC: MouseButtonRelease (Left)
        SPC->>SPC: isPanning = false
        SPC->>SPC: Reset cursor to ArrowCursor
        SPC->>SPC: updateLastPriceLine()
    end
```

---

## State Management

### Chart State Variables

```mermaid
stateDiagram-v2
    [*] --> Empty: New Chart
    
    Empty --> HasClosedBars: addBar(Closed)
    Empty --> HasOpenBar: addBar(Open)
    HasClosedBars --> HasOpenBar: addBar(Open)
    HasClosedBars --> HasClosedBars: addBar(Closed)
    HasOpenBar --> HasOpenBar: addBar(Open) - Update
    HasOpenBar --> HasClosedBars: addBar(Closed)
    
    note right of Empty
        completedBars: Empty Map
        hasOpenBar: false
        currentOpenBar: Invalid
        lastPrice: 0.0
    end note
    
    note right of HasClosedBars
        completedBars: Contains bars
        hasOpenBar: false
        lastPrice: Last close price
    end note
    
    note right of HasOpenBar
        completedBars: Contains bars
        hasOpenBar: true
        currentOpenBar: Valid Bar
        lastPrice: Current price
    end note
```

**View States:**

```mermaid
stateDiagram-v2
    [*] --> DefaultView: Initial
    
    DefaultView --> UserModifiedView: Zoom/Pan
    UserModifiedView --> UserModifiedView: Zoom/Pan
    UserModifiedView --> DefaultView: Right Click
    
    note right of DefaultView
        axisX: Last 30 minutes
        axisY: Auto-fit to bars
    end note
    
    note right of UserModifiedView
        axisX: User-defined range
        axisY: User-defined range
    end note
```

### Bar Limit Management

```mermaid
graph TD
    AddBar[Add New Bar to completedBars] --> CheckSize{completedBars.size > MAX_BARS?}
    CheckSize -->|No| Done[Done]
    CheckSize -->|Yes| RemoveLoop[maintainBarLimit loop]
    RemoveLoop --> Remove["Remove oldest bar<br/>completedBars.erase(begin)"]
    Remove --> CheckSize2{size still > MAX_BARS?}
    CheckSize2 -->|Yes| Remove
    CheckSize2 -->|No| Done
    
    Note1[MAX_BARS = 1000<br/>Prevents memory growth]
    Note1 -.-> CheckSize
```

---

## Missing Bars Detection

### Detection and Loading Flow

```mermaid
sequenceDiagram
    participant User
    participant SPC as StockPriceChart
    participant GF as GUIFrontend
    participant Cache as BarCache
    
    User->>SPC: Zoom Out / Pan Left
    SPC->>SPC: handleBothAxesZoom() or handlePanning()
    SPC->>SPC: checkForMissingBars(viewStartTime, viewEndTime)
    
    SPC->>SPC: Round viewStartTime (remove seconds/msecs)
    SPC->>SPC: Get firstBarTime from completedBars
    
    alt View extends before first bar
        SPC->>SPC: Check currentGetBarsRequestInProcess flag
        
        alt Request NOT in process
            SPC->>SPC: Set currentGetBarsRequestInProcess = true
            SPC->>GF: emit requestMissingBars(viewStartTimeRounded, firstBarTime)
            GF->>Cache: Fetch bars for time range
            Cache->>GF: Return QVector<Bar>
            GF->>SPC: onRequestedMissingBarsReceived(bars)
            
            Note over SPC: CRASH POINT 1:<br/>Assert currentGetBarsRequestInProcess == true
            
            SPC->>SPC: Set currentGetBarsRequestInProcess = false
            
            loop For each bar
                alt Not Void Bar
                    SPC->>SPC: Insert into completedBars
                else Void Bar
                    SPC->>SPC: Add to voidBarSeries
                end
            end
            
            SPC->>SPC: maintainBarLimit()
            SPC->>SPC: updateChart()
        else Request already in process
            SPC->>SPC: Log warning and return
            Note over SPC: Prevents duplicate requests
        end
    else View within existing bars
        SPC->>SPC: Return (no action needed)
    end
```

### Request State Management

```mermaid
stateDiagram-v2
    [*] --> Idle: Initial State
    
    Idle --> RequestPending: checkForMissingBars()<br/>Set flag = true<br/>Emit signal
    RequestPending --> RequestPending: checkForMissingBars()<br/>Log warning<br/>Return early
    RequestPending --> Idle: onRequestedMissingBarsReceived()<br/>Assert flag == true<br/>Set flag = false
    
    note right of RequestPending
        currentGetBarsRequestInProcess = true
        Prevents duplicate requests
        while waiting for response
    end note
    
    note right of Idle
        currentGetBarsRequestInProcess = false
        Ready to make new requests
    end note
```

---

## View State Preservation

### View State During Chart Updates

```mermaid
graph TD
    Start[updateChart Called] --> SaveState[Save Current View State]
    SaveState --> GetMin[currentMin = axisX->min]
    GetMin --> GetMax[currentMax = axisX->max]
    GetMax --> GetYMin[currentYMin = axisY->min]
    GetYMin --> GetYMax[currentYMax = axisY->max]
    GetYMax --> CheckInit{hadInitialView?<br/>currentMin != currentMax<br/>currentMin != 0}
    
    CheckInit -->|Yes - Preserve View| ClearSeries[Clear candlestickSeries]
    ClearSeries --> RebuildLoop[Loop through completedBars]
    RebuildLoop --> AddCandles[Add candlestick for each bar]
    AddCandles --> AddOpen{hasOpenBar?}
    AddOpen -->|Yes| AddOpenCandle[Add currentOpenBar candlestick]
    AddOpen -->|No| RestoreRange[Restore View State]
    AddOpenCandle --> RestoreRange
    RestoreRange --> SetX[axisX->setRange(currentMin, currentMax)]
    SetX --> SetY[axisY->setRange(currentYMin, currentYMax)]
    SetY --> End[Done]
    
    CheckInit -->|No - First Time| ClearSeries2[Clear candlestickSeries]
    ClearSeries2 --> RebuildLoop2[Loop through completedBars]
    RebuildLoop2 --> AddCandles2[Add candlestick for each bar]
    AddCandles2 --> CalcRange[Calculate default 30-min window]
    CalcRange --> SetInitial[Set initial X and Y ranges]
    SetInitial --> CalcYRange[Calculate Y range from visible bars]
    CalcYRange --> End
```

### State Preservation During Bar Addition

```mermaid
sequenceDiagram
    participant Ext as External Source
    participant SPC as StockPriceChart
    participant Axis as Chart Axes
    
    Ext->>SPC: addBar(bar)
    
    alt handleClosedBar
        SPC->>Axis: Save current view state
        Note over SPC: currentMin = axisX->min()<br/>currentMax = axisX->max()<br/>currentYMin = axisY->min()<br/>currentYMax = axisY->max()
        
        SPC->>SPC: Check hadInitialView flag
        
        SPC->>SPC: Update bar storage
        alt No open bar
            SPC->>SPC: completedBars.insert(bar)
        else Has open bar
            alt Different timestamp
                SPC->>SPC: completedBars.insert(currentOpenBar)
            end
            SPC->>SPC: completedBars.insert(bar)
            SPC->>SPC: hasOpenBar = false
        end
        
        SPC->>SPC: maintainBarLimit()
        
        alt hadInitialView = true
            SPC->>Axis: Restore exact view state
            SPC->>Axis: axisX->setRange(currentMin, currentMax)
            SPC->>Axis: axisY->setRange(currentYMin, currentYMax)
        else hadInitialView = false
            Note over SPC: Let updateChart set default view
        end
    end
    
    SPC->>SPC: updateChart()
```

---

## Potential Crash Points

### Critical Assert and Edge Cases

```mermaid
graph TB
    subgraph "CRASH POINT 1: onRequestedMissingBarsReceived"
        CP1[Q_ASSERT currentGetBarsRequestInProcess == true]
        CP1Cause1[Race Condition:<br/>Multiple rapid zoom/pan operations]
        CP1Cause2[Signal/Slot Connection Issue:<br/>Duplicate connections]
        CP1Cause3[Logic Bug:<br/>Flag not set before emit]
        
        CP1Cause1 -.-> CP1
        CP1Cause2 -.-> CP1
        CP1Cause3 -.-> CP1
    end
    
    subgraph "CRASH POINT 2: Empty Bars Response"
        CP2[bars.isEmpty in onRequestedMissingBarsReceived]
        CP2Cause1[Cache returns no data<br/>for requested range]
        CP2Cause2[Database query fails]
        
        CP2Cause1 -.-> CP2
        CP2Cause2 -.-> CP2
        
        CP2Fix[Currently: Log critical and return<br/>Previously: Q_ASSERT !bars.isEmpty]
    end
    
    subgraph "CRASH POINT 3: Map Access"
        CP3[Accessing completedBars.firstKey<br/>when map is empty]
        CP3Cause1[checkForMissingBars called<br/>before any bars added]
        CP3Cause2[clearSymbol followed by<br/>immediate pan operation]
        
        CP3Cause1 -.-> CP3
        CP3Cause2 -.-> CP3
        
        CP3Fix[Check: if completedBars.isEmpty]
    end
    
    subgraph "CRASH POINT 4: Iterator Issues"
        CP4[updateChart line 329:<br/>it == --completedBars.end]
        CP4Cause1[Decrement on begin iterator<br/>when map has 1 element]
        CP4Cause2[Concurrent modification<br/>during iteration]
        
        CP4Cause1 -.-> CP4
        CP4Cause2 -.-> CP4
    end
    
    subgraph "CRASH POINT 5: Division by Zero"
        CP5[handlePanning calculations]
        CP5Cause1[chartView->width = 0<br/>during resize]
        CP5Cause2[chartView->height = 0<br/>during resize]
        
        CP5Cause1 -.-> CP5
        CP5Cause2 -.-> CP5
    end
    
    style CP1 fill:#ff6666
    style CP2 fill:#ffaa66
    style CP3 fill:#ffaa66
    style CP4 fill:#ff6666
    style CP5 fill:#ffaa66
```

### Zoom Out and Drag Crash Scenario

```mermaid
sequenceDiagram
    participant User
    participant SPC as StockPriceChart
    participant GF as GUIFrontend
    participant Cache as BarCache
    
    Note over User,Cache: SCENARIO: Rapid zoom out + drag operations
    
    User->>SPC: Zoom Out (Wheel Event)
    SPC->>SPC: handleBothAxesZoom()
    SPC->>SPC: Expand time range
    SPC->>SPC: checkForMissingBars()
    
    alt completedBars is EMPTY
        Note over SPC: CRASH: completedBars.firstKey() called
        SPC->>SPC: ❌ Undefined behavior - map is empty
    end
    
    SPC->>SPC: viewStartTime < firstBarTime?
    SPC->>SPC: Check currentGetBarsRequestInProcess
    
    alt Flag is FALSE
        SPC->>SPC: Set currentGetBarsRequestInProcess = true
        SPC->>GF: emit requestMissingBars()
        
        Note over User,SPC: User immediately drags while waiting
        User->>SPC: Mouse Drag (Pan)
        SPC->>SPC: handlePanning()
        SPC->>SPC: checkForMissingBars()
        SPC->>SPC: Check currentGetBarsRequestInProcess
        
        alt Flag is TRUE (request pending)
            SPC->>SPC: Log warning and return
            Note over SPC: ✓ Prevents duplicate request
        end
        
        Note over GF,Cache: Original request completes
        Cache->>GF: Return bars
        GF->>SPC: onRequestedMissingBarsReceived(bars)
        
        alt bars.isEmpty()
            Note over SPC: PREVIOUSLY CRASHED HERE
            SPC->>SPC: Log critical error
            SPC->>SPC: Reset flag = false
            SPC->>SPC: Return early
            Note over SPC: ✓ Now handles gracefully
        end
        
        SPC->>SPC: Assert currentGetBarsRequestInProcess == true
        
        alt Flag is FALSE
            Note over SPC: ❌ CRASH: Assert fails!
            Note over SPC: Possible causes:<br/>1. Duplicate slot connection<br/>2. Race condition<br/>3. Logic error
        end
        
        SPC->>SPC: Set flag = false
        SPC->>SPC: Process bars
    end
```

### Thread Safety Concerns

```mermaid
graph TB
    subgraph "Main GUI Thread"
        MainThread[Event Loop]
        EventHandler[Event Handlers:<br/>wheelEvent, mouseEvent]
        UpdateChart[updateChart]
        Repaint[Chart Repaint]
    end
    
    subgraph "Potential Race Conditions"
        RC1[currentGetBarsRequestInProcess flag]
        RC2[completedBars map modification]
        RC3[Axis range changes]
        RC4[Series data updates]
    end
    
    subgraph "Signal/Slot Connections"
        Signal1[requestMissingBars signal]
        Slot1[onRequestedMissingBarsReceived slot]
        Signal2[rangeChanged signals from axes]
        Slot2[updateAfterHoursBackground slot]
    end
    
    MainThread --> EventHandler
    EventHandler --> RC1
    EventHandler --> RC3
    Signal1 --> Slot1
    Slot1 --> RC1
    Slot1 --> RC2
    UpdateChart --> RC2
    UpdateChart --> RC4
    Signal2 --> Slot2
    RC3 --> Signal2
    
    Warning1[⚠️ Multiple rapid events<br/>can trigger overlapping operations]
    Warning2[⚠️ Axis range changes<br/>trigger signals during updates]
    
    Warning1 -.-> EventHandler
    Warning2 -.-> Signal2
    
    style RC1 fill:#ffcccc
    style RC2 fill:#ffcccc
    style RC3 fill:#ffcccc
    style RC4 fill:#ffcccc
```

---

## Debugging Recommendations

### Key Areas to Investigate for Zoom/Drag Crashes

1. **Assert in onRequestedMissingBarsReceived (Line 144)**
   - Check for duplicate signal/slot connections
   - Verify flag state management in checkForMissingBars
   - Add logging before emit to confirm flag is set
   - Consider using QMutex for thread-safe flag access

2. **Empty completedBars Map Access**
   - Add guards: `if (!completedBars.isEmpty())` before `completedBars.firstKey()`
   - Particularly in checkForMissingBars (line 834)
   - Consider defensive checks after clearSymbol()

3. **Iterator Arithmetic (Line 329, 734)**
   - Review: `it == --completedBars.end()`
   - This is undefined if map has 0 or 1 elements
   - Replace with: `it.key() == completedBars.lastKey()`

4. **Division by Zero in handlePanning**
   - Add checks before: `timePerPixel` and `pricePerPixel` calculations
   - Verify chartView dimensions are valid
   - Handle resize events that may create zero-size views

5. **Rapid Event Handling**
   - Consider debouncing zoom/pan events
   - Use QTimer to coalesce rapid checkForMissingBars calls
   - Implement event throttling for better performance

### Suggested Code Improvements

```cpp
// In checkForMissingBars (line 824):
void StockPriceChart::checkForMissingBars(const QDateTime& viewStartTime, const QDateTime& viewEndTime) {
    // ADD: Early return if no bars
    if (completedBars.isEmpty()) {
        qCDebug(ChartLog) << "No bars available yet";
        return;
    }
    
    // ... rest of function
}

// In onRequestedMissingBarsReceived (line 142):
void StockPriceChart::onRequestedMissingBarsReceived(const QVector<Bar>& bars) {
    // ADD: More detailed logging
    qCDebug(ChartLog) << "Received" << bars.size() << "bars, flag state:" << currentGetBarsRequestInProcess;
    
    if (!currentGetBarsRequestInProcess) {
        qCCritical(ChartLog) << "LOGIC ERROR: Received bars without pending request!";
        qCCritical(ChartLog) << "This indicates duplicate connections or race condition";
        // Consider returning instead of asserting
        return;
    }
    
    // ... rest of function
}

// In updateChart (line 329):
// REPLACE:
if (it == --completedBars.end()) {
    currentPrice = bar.getClose();
}
// WITH:
if (it.key() == completedBars.lastKey()) {
    currentPrice = bar.getClose();
}
```

---

## Summary

The StockPriceChart is a complex component with multiple interacting systems:
- Real-time data updates
- Interactive zoom and pan
- Dynamic bar loading
- View state preservation
- Market hours visualization

The most likely crash scenario during zoom out and drag operations involves:
1. The `currentGetBarsRequestInProcess` flag state management
2. Empty map access in `completedBars.firstKey()`
3. Iterator arithmetic issues
4. Potential race conditions from rapid user interactions

Key defensive programming patterns needed:
- Always check if `completedBars.isEmpty()` before accessing keys
- Validate flag state before and after async operations
- Add bounds checking for iterator arithmetic
- Consider thread-safe access patterns
- Implement event throttling for better UX and stability
