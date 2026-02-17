# MarketDepth/ - Level 2 Market Depth Display Widget - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/MarketDepth/`
**Purpose**: Display Level 2 market depth (bid/ask levels) in real-time
**Main Classes**: MarketDepthTable, MarketDepthTableView

The MarketDepth widget provides a visual display of Level 2 market depth data, showing multiple price levels on both the bid and ask sides with size and count information.

## Files

- **MarketDepthTable.h/cpp** - Main market depth widget
- **MarketDepthTableView.h/cpp** - Custom table view for formatting

## MarketDepthTable

**Purpose**: Container widget managing market depth display

### Features

- Real-time Level 2 market depth display
- Bid side (left) and Ask side (right) columns
- Price, Size, and Count for each level
- Color-coded bid (green) and ask (red) sides
- Bold formatting for best bid/ask
- DWP (Depth-Weighted Price) indicator
- Spread calculation and display
- Update throttling (100ms) to prevent UI lag
- Automatic sorting (best prices at top)

### Public Interface

```cpp
/// Construct market depth table
/// @param parent Parent widget
explicit MarketDepthTable(QWidget* parent = nullptr);

/// Destructor
~MarketDepthTable();
```

### Public Slots

```cpp
/// Update market depth with new quote
/// Throttled to prevent excessive updates
/// @param symbol Stock symbol
/// @param quote Market depth quote with bid/ask levels
/// @param dwp Depth-Weighted Price indicator
/// @param bidTotalVol Total bid volume
/// @param askTotalVol Total ask volume
void updateMarketDepth(const QString& symbol,
                      const MarketDepthQuote& quote,
                      double dwp,
                      double bidTotalVol,
                      double askTotalVol);

/// Clear the market depth display
/// Called when symbol changes or connection lost
void clearMarketDepth();

/// Set top margin for header offset
/// @param margin Margin in pixels
void setTopMargin(int margin);
```

### UI Layout

```
┌──────────────────────────────────────────────────────────┐
│               BID                    ASK                  │
├─────────────────────────────────┬────────────────────────┤
│ Price    │ Size    │ Count      │ Price   │ Size  │ Count│
├─────────────────────────────────┼────────────────────────┤
│ $99.95   │ 1,500   │    5       │ $100.05 │ 2,000 │   8  │ ← Best prices (bold)
│ $99.90   │ 2,200   │    9       │ $100.10 │ 1,800 │   6  │
│ $99.85   │ 1,000   │    3       │ $100.15 │ 3,500 │  12  │
│ $99.80   │  800    │    2       │ $100.20 │ 1,200 │   4  │
│ $99.75   │ 1,500   │    7       │ $100.25 │ 2,500 │  10  │
└─────────────────────────────────┴────────────────────────┘
         ▲                                 ▲
       Green                              Red
```

### Private Methods

```cpp
/// Initialize UI components
void setupUI();

/// Apply dark theme styling
void setupStyles();

/// Update the table with new data
/// @param quote Market depth quote
void updateTable(const MarketDepthQuote& quote);

/// Update bid side of the table
/// @param bids Vector of bid levels
void updateBidSide(const QVector<MarketDepthLevel>& bids);

/// Update ask side of the table
/// @param asks Vector of ask levels
void updateAskSide(const QVector<MarketDepthLevel>& asks);

/// Format price for display
/// @param price Price value
/// @return Formatted string (e.g., "$99.95")
QString formatPrice(double price) const;

/// Format size for display
/// @param size Size value
/// @return Formatted string with commas (e.g., "1,500")
QString formatSize(int size) const;
```

### Member Variables

```cpp
private:
    // UI components
    MarketDepthTableView* tableView;
    QStandardItemModel* model;
    QLabel* bidLabel;
    QLabel* askLabel;
    QLabel* spreadLabel;
    QLabel* dwpLabel;
    
    // Update throttling
    QTimer* m_updateThrottle;
    MarketDepthQuote m_pendingQuote;
    
    // State
    QString m_currentSymbol;
    static constexpr int MAX_LEVELS = 10;  // Show 10 levels each side
```

### Update Throttling

To prevent UI lag from high-frequency market data:

```cpp
MarketDepthTable::MarketDepthTable(QWidget* parent) : QWidget(parent) {
    m_updateThrottle = new QTimer(this);
    m_updateThrottle->setInterval(100);  // Max 10 updates/sec
    m_updateThrottle->setSingleShot(true);
    
    connect(m_updateThrottle, &QTimer::timeout, this, [this]() {
        updateTable(m_pendingQuote);
    });
}

void MarketDepthTable::updateMarketDepth(const QString& symbol,
                                         const MarketDepthQuote& quote,
                                         double dwp, double bidVol, double askVol) {
    m_pendingQuote = quote;
    m_currentSymbol = symbol;
    
    if (!m_updateThrottle->isActive()) {
        updateTable(m_pendingQuote);
        m_updateThrottle->start();
    }
}
```

### Color Coding

Bid and ask sides are color-coded for clarity:

```cpp
void MarketDepthTable::setupStyles() {
    // Bid side (left) - green
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int col = 0; col < 3; ++col) {
            QStandardItem* item = model->item(row, col);
            item->setForeground(QColor(0, 200, 0));  // Green
        }
    }
    
    // Ask side (right) - red
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int col = 3; col < 6; ++col) {
            QStandardItem* item = model->item(row, col);
            item->setForeground(QColor(200, 0, 0));  // Red
        }
    }
}
```

### Best Price Highlighting

Best bid and ask are displayed in bold:

```cpp
void MarketDepthTable::updateTable(const MarketDepthQuote& quote) {
    // ... populate table
    
    // Bold best bid (first row, columns 0-2)
    for (int col = 0; col < 3; ++col) {
        QStandardItem* item = model->item(0, col);
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
    }
    
    // Bold best ask (first row, columns 3-5)
    for (int col = 3; col < 6; ++col) {
        QStandardItem* item = model->item(0, col);
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
    }
}
```

### Spread Calculation

Display spread between best bid and ask:

```cpp
void MarketDepthTable::updateSpread(const MarketDepthQuote& quote) {
    double spread = quote.getBestAsk() - quote.getBestBid();
    double spreadPercent = (spread / quote.getBestBid()) * 100.0;
    
    QString spreadText = QString("Spread: $%1 (%2%)")
                            .arg(spread, 0, 'f', 2)
                            .arg(spreadPercent, 0, 'f', 3);
    
    spreadLabel->setText(spreadText);
}
```

## MarketDepthTableView

**Purpose**: Custom QTableView with specialized formatting

### Features

- Custom cell rendering
- Header offset support
- Alignment control
- Font customization

### Public Interface

```cpp
/// Construct custom table view
/// @param parent Parent widget
explicit MarketDepthTableView(QWidget* parent = nullptr);

/// Set top margin to offset header
/// @param margin Margin in pixels
void setTopMargin(int margin);
```

### Custom Rendering

The view provides specialized rendering for market depth data:

```cpp
class MarketDepthTableView : public QTableView {
    Q_OBJECT

public:
    explicit MarketDepthTableView(QWidget* parent = nullptr);
    void setTopMargin(int margin);

protected:
    // Custom painting if needed
    void paintEvent(QPaintEvent* event) override;
    
private:
    int m_topMargin = 0;
};
```

## Data Flow

### Market Depth Update Flow

```
TSClient receives Level 2 data stream
    │
    ▼
MainAlgo::onMarketDepthQuoteReceived()
    │
    ├─ Calculate DWP (Depth-Weighted Price)
    ├─ Calculate total bid/ask volumes
    │
    └─ emit displayedStockReceivedNewMarketDepthQuote()
            │
            ▼ (cross-thread signal)
GUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote()
    │
    └─ Forward to MarketDepthTable::updateMarketDepth()
            │
            ├─ Store pending update
            │
            └─ If not throttling: updateTable()
                    │
                    ├─ Update model data
                    ├─ Apply colors
                    ├─ Bold best prices
                    └─ Replot table
```

## DWP (Depth-Weighted Price)

**Purpose**: Indicator of where most liquidity is concentrated

**Calculation** (performed in MainAlgo):
```cpp
double calculateDWP(const MarketDepthQuote& quote) {
    double bidWeightedSum = 0.0;
    double askWeightedSum = 0.0;
    double bidTotalVol = 0.0;
    double askTotalVol = 0.0;
    
    for (const auto& bid : quote.getBids()) {
        bidWeightedSum += bid.price * bid.size;
        bidTotalVol += bid.size;
    }
    
    for (const auto& ask : quote.getAsks()) {
        askWeightedSum += ask.price * ask.size;
        askTotalVol += ask.size;
    }
    
    double bidDWP = bidWeightedSum / bidTotalVol;
    double askDWP = askWeightedSum / askTotalVol;
    
    return (bidDWP + askDWP) / 2.0;
}
```

## Performance Optimizations

### Update Throttling

Limit updates to 10 per second to prevent UI lag:
- Buffer incoming quotes
- Apply most recent on timer expiry
- Smooth visual experience

### Model Updates

Efficient model updates:
```cpp
// ✓ CORRECT: Update individual cells
model->item(row, col)->setText(newText);

// ✗ WRONG: Full model reset (slow)
model->clear();
model->setRowCount(rows);
// ... rebuild entire model
```

### Lazy Calculation

Only calculate displayed values:
```cpp
void updateTable(const MarketDepthQuote& quote) {
    // Only show top 10 levels
    int levelsToShow = qMin(MAX_LEVELS, quote.getBids().size());
    
    for (int i = 0; i < levelsToShow; ++i) {
        // Update only visible rows
        updateRow(i, quote.getBids()[i]);
    }
}
```

## Testing

### Unit Tests

Test data formatting:

```cpp
TEST(MarketDepthTable, FormatsPrice) {
    MarketDepthTable table;
    QString formatted = table.formatPrice(99.95);
    EXPECT_EQ(formatted, "$99.95");
}

TEST(MarketDepthTable, FormatsSize) {
    MarketDepthTable table;
    QString formatted = table.formatSize(1500);
    EXPECT_EQ(formatted, "1,500");
}
```

### Integration Tests

Test market depth updates:

```cpp
TEST(MarketDepthTable, UpdatesOnQuoteReceived) {
    MarketDepthTable table;
    
    MarketDepthQuote quote;
    quote.addBid(99.95, 1500, 5);
    quote.addAsk(100.05, 2000, 8);
    
    table.updateMarketDepth("AAPL", quote, 100.0, 1500, 2000);
    
    // Verify table updated
    EXPECT_EQ(table.getRowCount(), 1);
    EXPECT_EQ(table.getBestBid(), 99.95);
    EXPECT_EQ(table.getBestAsk(), 100.05);
}
```

## Common Issues

### Table Not Updating

**Problem**: Market depth doesn't refresh

**Solutions**:
1. Check symbol matches current displayed stock
2. Verify throttle timer is running
3. Ensure model is not null
4. Check signal connections

### Incorrect Colors

**Problem**: Bid/ask colors swapped or missing

**Solutions**:
1. Verify column indices (0-2 bid, 3-5 ask)
2. Check setupStyles() called after model population
3. Ensure QStandardItem foreground set correctly

### Performance Issues

**Problem**: UI lags with frequent updates

**Solutions**:
1. Verify throttling is enabled (100ms interval)
2. Check only visible rows are updated
3. Ensure no full model resets
4. Profile with high-frequency test data

## Related Components

- **GUIFrontend**: Container for market depth display
- **MainAlgo**: Calculates DWP and forwards market depth
- **TSClient**: Receives Level 2 data stream from TradeStation
- **MarketDepthQuote**: Data structure for market depth
- **OrderEntryWidget**: Uses market depth for sticky price

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Widgets documentation
- `Doc/FRONTEND.md`: Frontend architecture
- `Doc/ARCHITECTURE.md`: System architecture
