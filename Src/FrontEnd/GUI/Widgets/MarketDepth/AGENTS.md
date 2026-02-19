# MarketDepth/ - Market Depth Display Widget - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/MarketDepth/`
**Purpose**: Display Level 2 market depth or Level 1 best bid/ask in real-time
**Main Classes**: MarketDepthTable, MarketDepthTableView

The MarketDepth widget displays market data in the main trading view. It has three operating
modes: Level 2 (full order book), Level 1 (best bid/ask from Quote stream), and NoData
(no stream open). The widget auto-switches between modes based on available streams.

## Files

- **MarketDepthTable.h/cpp** - Main market depth widget
- **MarketDepthTableView.h/cpp** - Custom table view for formatting

## MarketDepthTable

**Purpose**: Container widget managing market depth/quote display with mode auto-switching

### Display Modes

```cpp
enum class DisplayMode {
    Level2,   // Full order book from StreamMarketDepthQuote
    Level1,   // Best bid/ask only from StreamQuote
    NoData    // No stream open for this symbol
};
```

### Visual Indicator

Between the BID and ASK header labels sits `m_dataSourceLabel` — a small indicator showing
the current display mode:

| Mode | Text | Color |
|------|------|-------|
| Level2 | `L2` | Green (`#00AA00`) |
| Level1 | `L1` | Yellow (`#AAAA00`) |
| NoData | `--` | Grey (`#888888`) |

### Public Interface

```cpp
class MarketDepthTable : public QWidget {
    Q_OBJECT
public:
    explicit MarketDepthTable(QWidget* parent = nullptr);
    ~MarketDepthTable();

public slots:
    // Level 2: Update with full order book (sets DisplayMode::Level2)
    void updateData(const MarketDepthQuote& quote,
                    double dwp,
                    double bidTotalVol,
                    double askTotalVol);

    // Level 1: Show single best bid/ask row from Quote stream
    void updateLevel1Data(const Quote& quote);

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
Level 2 path:
    TSClient (StreamMarketDepthQuote)
        │
        ▼
    MainAlgo::onMarketDepthQuoteReceived()
        │  Calculates DWP, bid/ask totals
        ▼
    emit displayedStockReceivedNewMarketDepthQuote()
        │  (cross-thread, QueuedConnection)
        ▼
    GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote()
        │
        └─► MarketDepthTable::updateData()
                │  Sets DisplayMode::Level2
                └─► updateDataSourceIndicator()

Level 1 path:
    TSClient (StreamQuote)
        │  emit newQuoteReceived(symbol, quote)
        ▼
    MainAlgo::onDisplayedStockReceivedNewQuote()
        │  Filters by currently displayed symbol
        ▼
    emit displayedStockReceivedNewQuote(quote)
        │  (cross-thread, QueuedConnection)
        ▼
    GUIFrontend::onCurrentHighlightedReceivedNewQuote()
        │  Checks: getDisplayMode() != DisplayMode::Level2
        │  (NOT hasOpenMarketDepthStream — that is always true in replay
        │   because a MockNetworkReply is always registered for depth)
        │  If not Level2: call updateLevel1Data()
        └─► MarketDepthTable::updateLevel1Data(quote)
                │  Sets DisplayMode::Level1, shows 1 row
                └─► updateDataSourceIndicator()
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

### Level 1 Layout

```
┌──────────────────────────────────────────────────┐
│         BID    [L1]    ASK                        │
├─────────────────────────────────────────────────┤
│ Price   │ Size  │      │ Price   │ Size  │        │
│ $99.95  │ 10    │      │ $100.05 │ 20    │        │ ← single row
└──────────────────────────────────────────────────┘
     Green   (single best bid/ask from Quote stream)
```

### Member Variables

```cpp
private:
    // UI components
    MarketDepthTableView* m_tableView;
    QStandardItemModel*   m_model;
    QLabel* m_bidLabel;
    QLabel* m_dataSourceLabel;  // L2/L1/-- indicator between headers
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
void updateTable(const MarketDepthQuote& quote);

// Update the L2/L1/-- indicator label
void updateDataSourceIndicator();

// Format helpers
QString formatPrice(double price) const;
QString formatSize(int size) const;
```

## MarketDepthTableView

**Purpose**: Custom QTableView with specialized formatting

```cpp
class MarketDepthTableView : public QTableView {
    Q_OBJECT
public:
    explicit MarketDepthTableView(QWidget* parent = nullptr);
    void setTopMargin(int margin);

private:
    int m_topMargin = 0;
};
```

## DWP (Depth-Weighted Price)

Displayed in Level 2 mode. Calculated in MainAlgo:

```cpp
// Calculated before emitting displayedStockReceivedNewMarketDepthQuote()
double bidDWP = bidWeightedSum / bidTotalVol;
double askDWP = askWeightedSum / askTotalVol;
double dwp = (bidDWP + askDWP) / 2.0;
```

Only available in Level 2 mode. Not shown in Level 1 mode.

## Mode Selection Logic

In `GUIFrontend::onCurrentHighlightedReceivedNewQuote()`:

```cpp
void GUIFrontend::onCurrentHighlightedReceivedNewQuote(const Quote& quote) {
    // Update MarketFlags labels
    m_haltedLabel->setProperty("active", quote.getMarketFlags().isHalted());
    m_delayedLabel->setProperty("active", quote.getMarketFlags().isDelayed());
    m_hardToBorrowLabel->setProperty("active", quote.getMarketFlags().isHardToBorrow());

    // Only use L1 data for depth widget if no L2 stream is open for this symbol
    if (!TSClient::getInstance()->hasOpenMarketDepthStream(quote.getSymbol())) {
        m_marketDepthTable->updateLevel1Data(quote);
    }
}
```

Level 2 data (`updateData()`) always takes priority; L1 data is strictly a fallback.

## Related Components

- **GUIFrontend**: Container; routes both Level 1 and Level 2 data to this widget
- **MainAlgo**: Calculates DWP and forwards market depth; filters quotes by displayed symbol
- **TSClient**: `hasOpenMarketDepthStream(symbol)` used to determine mode
- **Quote.h**: Level 1 data structure used in `updateLevel1Data()`
- **MarketDepthQuote**: Level 2 data structure used in `updateData()`

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Widgets documentation
- `Doc/FRONTEND.md`: Frontend architecture
- `Src/Clients/TSClient/MarketData/GetQuoteSnapshots/AGENTS.md`: Quote data structure
