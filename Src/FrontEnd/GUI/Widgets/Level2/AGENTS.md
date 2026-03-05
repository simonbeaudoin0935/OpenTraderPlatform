# Level2/ - Level 2 Display Widget - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/Level2/`
**Purpose**: Display Level 2 Level 2 data in real-time
**Main Classes**: Level2Widget, Level2WidgetView

The Level2 widget displays market data in the main trading view. It has two operating
modes: Level2 (full order book from `Level2` data) and NoData (no data available).

## Files

- **Level2Widget.h/cpp** - Main Level 2 widget
- **Level2WidgetView.h/cpp** - Custom table view for formatting

## Level2Widget

**Purpose**: Container widget managing Level 2 Level 2 display

### Display Modes

```cpp
enum class DisplayMode {
    Level2,   // Full order book from Level2 data
    NoData    // No data available for this symbol
};
```

### Visual Indicator

Between the BID and ASK header labels sits `m_dataSourceLabel` — a small indicator showing
the current display mode:

| Mode | Text | Color |
|------|------|-------|
| Level2 | `L2` | Green (`#00AA00`) |
| NoData | `--` | Grey (`#888888`) |

### Public Interface

```cpp
class Level2Widget : public QWidget {
    Q_OBJECT
public:
    explicit Level2Widget(QWidget* parent = nullptr);
    ~Level2Widget();

public slots:
    // Level 2: Update with full order book (sets DisplayMode::Level2)
    void updateData(const Level2& p_level2,
                    double dwp,
                    double bidTotalVol,
                    double askTotalVol);

    // Reset to empty/NoData state
    void clearData();

    // Legacy alias for clearData()
    void clearMarketDepth();

    // Set top margin for header offset
    void setTopMargin(int margin);
};
```

### Data Flow

```
Level2Receiver
    │
    ▼
MainAlgo::onLevel2Received()
    │  Calculates DWP, bid/ask totals
    ▼
emit displayedStockReceivedNewLevel2()
    │  (cross-thread, QueuedConnection)
    ▼
GUIFrontend::onCurrentHighlightedReceivedNewLevel2()
    │
    └─► Level2Widget::updateData(Level2)
            │  Sets DisplayMode::Level2
            └─► updateDataSourceIndicator()
```

### Level 2 Data Model

The `Level2` type (from `Src/Core/Models/Level2.h`) contains:

```cpp
struct Level2Row {
    double m_price;
    int    m_size;
    int    m_orderCount;
};

struct Level2 {
    std::array<Level2Row, 10> m_bids;
    std::array<Level2Row, 10> m_asks;
};
```

### Level 2 Layout

```
┌──────────────────────────────────────────────────┐
│         BID    [L2]    ASK                        │
├─────────────────────────────────────────────────┤
│ Price   │ Size  │ Count │ Price   │ Size  │ Count │
│ $99.95  │ 1,500 │   5   │ $100.05 │ 2,000 │   8   │ ← bold (best)
│ $99.90  │ 2,200 │   9   │ $100.10 │ 1,800 │   6   │
│ ...     │ ...   │ ...   │ ...     │ ...   │ ...   │
└──────────────────────────────────────────────────┘
     Green                        Red
```

Columns per side: Price (`double`), Size (`int`), Count (`int`) — sourced directly from `Level2Row`.

### Member Variables

```cpp
private:
    // UI components
    Level2WidgetView* m_tableView;
    QStandardItemModel*   m_model;
    QLabel* m_bidLabel;
    QLabel* m_dataSourceLabel;  // L2/-- indicator between headers
    QLabel* m_askLabel;
    QLabel* m_spreadLabel;
    QLabel* m_dwpLabel;

    // Display mode
    DisplayMode m_displayMode = DisplayMode::NoData;

    // State
    QString m_currentSymbol;
    static constexpr int MAX_LEVELS = 10;
```

### Private Methods

```cpp
// Initialize UI components
void setupUI();

// Apply dark theme styling
void setupStyles();

// Update the full table with Level 2 data
void updateTable(const Level2& p_level2);

// Update the L2/-- indicator label
void updateDataSourceIndicator();

// Format helpers
QString formatPrice(double price) const;
QString formatSize(int size) const;
```

## Level2WidgetView

**Purpose**: Custom QTableView with specialized formatting

```cpp
class Level2WidgetView : public QTableView {
    Q_OBJECT
public:
    explicit Level2WidgetView(QWidget* parent = nullptr);
    void setTopMargin(int margin);

private:
    int m_topMargin = 0;
};
```

## DWP (Depth-Weighted Price)

Displayed in Level 2 mode. Calculated in MainAlgo:

```cpp
// Calculated before emitting displayedStockReceivedNewLevel2()
double bidDWP = bidWeightedSum / bidTotalVol;
double askDWP = askWeightedSum / askTotalVol;
double dwp = (bidDWP + askDWP) / 2.0;
```

## Related Components

- **GUIFrontend**: Container; routes Level 2 data to this widget
- **MainAlgo**: Calculates DWP and forwards Level 2 data
- **Level2.h** (`Src/Core/Models/Level2.h`): `Level2` and `Level2Row` data structures used in `updateData()`

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Widgets documentation
- `Doc/FRONTEND.md`: Frontend architecture
