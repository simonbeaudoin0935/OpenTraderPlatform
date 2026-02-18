# Windows/ - Window-Style Display Components - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Windows/`
**Purpose**: Window-style widgets for displaying trading data (orders, positions, balances)
**Pattern**: QWidget-based components with QTableView for data display

The Windows folder contains dedicated display windows for various trading data types. These are typically shown as dock widgets in the main GUI but can also be used standalone.

## Components

### OrderWindow (OrderWindow.h/cpp)

**Purpose**: Display and manage trading orders

**Features**:
- Real-time order tracking
- Order status display (ACK, Open, Filled, Canceled, Rejected)
- Symbol click to load chart
- Right-click context menu to cancel orders
- Color-coded status indicators
- Sort and filter by any column
- Tracks order history

**Public Interface**:

```cpp
/// Construct order window
/// @param p_parent Parent widget
explicit OrderWindow(QWidget* p_parent = nullptr);

/// Destructor
~OrderWindow();

/// Get all order IDs currently displayed
/// @return List of order IDs
QStringList getAllOrderIds() const;

/// Get IDs of orders that can be cancelled
/// Filters for open orders only
/// @return List of cancellable order IDs
QStringList getCancellableOrderIds() const;
```

**Public Slots**:

```cpp
/// Update or add an order to the display
/// If order exists, updates the row; otherwise adds new row
/// @param account Account ID
/// @param order Order object with all details
void updateOrder(const QString& account, const Order& order);

/// Clear all orders from display
/// Typically called when switching accounts or resetting
void clearAllOrders();
```

**Signals**:

```cpp
/// Emitted when user clicks on a symbol
/// Used to auto-load symbol in chart
/// @param symbol Stock symbol clicked
void symbolClicked(const QString& symbol);

/// Emitted when user requests order cancellation
/// @param orderId Order ID to cancel
void cancelOrderRequested(const QString& orderId);

/// Emitted when user requests cancel all orders
void cancelAllOrdersRequested();
```

**Table Columns**:
1. Order ID
2. Symbol (clickable)
3. Account
4. Action (Buy, Sell, BuyToCover, SellShort)
5. Quantity
6. Type (Market, Limit, Stop, StopLimit)
7. Price (or "Market" for market orders)
8. DateTime (submission time)
9. Status (ACK, Open, Filled, Canceled, Rejected)

**Status Color Coding**:
```cpp
enum class OrderStatus {
    ACK,        // Gray - Acknowledged
    OPN,        // Yellow - Open (working)
    FLL,        // Green - Filled (complete)
    FLP,        // Green - Filled partially
    CAN,        // Red - Cancelled
    REJ         // Red - Rejected
};
```

**Order Tracking**:
```cpp
private:
    // Map order ID to row index for fast updates
    QMap<QString, int> m_orderRowMap;

    // Store complete Order objects for status checking
    QHash<QString, Order> m_orders;
```

---

### PositionWindow (PositionWindow.h/cpp)

**Purpose**: Real-time position tracking and P&L display

**Features**:
- Live position updates
- Real-time P&L calculation
- P&L color coding (green/red)
- Symbol click to load chart
- Right-click context menu (future: quick close)
- Sort by any column
- Market value tracking

**Public Interface**:

```cpp
/// Construct position window
/// @param parent Parent widget
explicit PositionWindow(QWidget* parent = nullptr);

/// Destructor
~PositionWindow();
```

**Public Slots**:

```cpp
/// Update or add a position to the display
/// Calculates P&L and updates row
/// @param account Account ID
/// @param position Position object with details
void updatePosition(const QString& account, const Position& position);

/// Remove a position from display
/// Called when position is closed
/// @param account Account ID
/// @param positionId Position ID to remove
void removePosition(const QString& account, const QString& positionId);

/// Clear all positions from display
void clearAllPositions();

/// Update last price for P&L calculation
/// Called when market data updates
/// @param symbol Stock symbol
/// @param lastPrice Current market price
void updateLastPrice(const QString& symbol, double lastPrice);
```

**Signals**:

```cpp
/// Emitted when user clicks on a symbol
/// @param symbol Stock symbol clicked
void symbolClicked(const QString& symbol);

/// Emitted when user requests position close
/// @param positionId Position ID to close
void closePositionRequested(const QString& positionId);
```

**Table Columns**:
1. Symbol (clickable)
2. Account
3. Quantity (positive=long, negative=short)
4. Average Price (cost basis)
5. Last Price (current market price)
6. P&L (dollars, color-coded)
7. P&L % (percentage, color-coded)
8. Market Value (quantity × last price)

**P&L Calculation**:
```cpp
// Long position
double pl = (lastPrice - avgPrice) * quantity;
double plPercent = ((lastPrice - avgPrice) / avgPrice) * 100.0;

// Short position (quantity is negative)
double pl = (avgPrice - lastPrice) * abs(quantity);
double plPercent = ((avgPrice - lastPrice) / avgPrice) * 100.0;
```

**Color Coding**:
- **Green**: P&L > 0 (profitable)
- **Red**: P&L < 0 (loss)
- **White**: P&L = 0 (break-even)

**Position Tracking**:
```cpp
private:
    // Map position ID to row index
    QMap<QString, int> m_positionRowMap;

    // Store positions for P&L updates
    QHash<QString, Position> m_positions;

    // Cache last prices for P&L calculation
    QHash<QString, double> m_lastPrices;
```

---

### BalanceWindow (BalanceWindow.h/cpp)

**Purpose**: Account balance and buying power display

**Features**:
- Real-time balance updates
- Cash available display
- Buying power calculation
- Equity tracking
- Margin used display
- Account value
- Day P&L tracking

**Public Interface**:

```cpp
/// Construct balance window
/// @param parent Parent widget
explicit BalanceWindow(QWidget* parent = nullptr);

/// Destructor
~BalanceWindow();
```

**Public Slots**:

```cpp
/// Update balance display with new data
/// @param account Account ID
/// @param balance Balance object with all metrics
void updateBalance(const QString& account, const Balance& balance);

/// Clear balance display
/// Called when switching accounts
void clearBalance();
```

**Display Fields**:
1. **Cash Available**: Liquid cash for trading
2. **Buying Power**: Leveraged buying capacity
3. **Equity**: Total account value (cash + positions)
4. **Margin Used**: Amount of margin currently in use
5. **Account Value**: Total account value
6. **Day P&L**: Today's profit/loss
7. **Day P&L %**: Today's P&L as percentage

**Color Coding**:
- **Day P&L**: Green if positive, red if negative
- **Margin Used**: Red if near limit, yellow if moderate, green if low

**UI Layout**:
```
┌──────────────────────────────────┐
│ BALANCE                          │
├──────────────────────────────────┤
│ Cash Available:    $25,000.00    │
│ Buying Power:      $100,000.00   │
│ Equity:            $26,234.56    │
│ Margin Used:       $5,000.00     │
│ Account Value:     $26,234.56    │
│ Day P&L:           +$1,234.56    │
│ Day P&L %:         +4.94%        │
└──────────────────────────────────┘
```

---

## Common Patterns

### Window Setup

All windows follow a similar initialization:

```cpp
class MyWindow : public QWidget {
public:
    explicit MyWindow(QWidget* parent = nullptr);
    ~MyWindow();

private:
    void setupUI();       // Create widgets and layout
    void setupStyles();   // Apply dark theme

    QTableView* m_tableView;
    QStandardItemModel* m_model;
    QLabel* m_headerLabel;
};

MyWindow::MyWindow(QWidget* parent) : QWidget(parent) {
    setupUI();
    setupStyles();
}
```

### Header Label

All windows have a prominent header:

```cpp
void setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Header
    m_headerLabel = new QLabel("WINDOW TITLE", this);
    QFont headerFont;
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_headerLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_tableView);
}
```

### Dark Theme Styling

Consistent styling across all windows:

```cpp
void setupStyles() {
    setStyleSheet(R"(
        QWidget {
            background-color: #353535;
            color: white;
        }
        QHeaderView::section {
            background-color: #454545;
            color: white;
            border: 1px solid #555;
            padding: 5px;
        }
        QTableView {
            gridline-color: #555;
            background-color: #353535;
            color: white;
        }
        QLabel {
            color: white;
        }
    )");
}
```

### Row Updates

Efficient row updates (update existing or add new):

```cpp
void updateRow(const QString& id, const Data& data) {
    // Check if row exists
    if (m_rowMap.contains(id)) {
        // Update existing row
        int row = m_rowMap[id];
        updateRowItems(row, data);
    } else {
        // Add new row
        int row = m_model->rowCount();
        m_model->appendRow(createRowItems(data));
        m_rowMap[id] = row;
    }
}
```

### Symbol Click Handling

Allow clicking symbols to load in chart:

```cpp
void setupConnections() {
    connect(m_tableView, &QTableView::clicked,
            this, &MyWindow::onCellClicked);
}

void onCellClicked(const QModelIndex& index) {
    // Check if symbol column
    if (index.column() == SYMBOL_COLUMN) {
        QString symbol = m_model->data(index).toString();
        emit symbolClicked(symbol);
    }
}
```

### Context Menu

Right-click menus for actions:

```cpp
void setupTableView() {
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &MyWindow::showContextMenu);
}

void showContextMenu(const QPoint& pos) {
    QMenu menu(this);

    QModelIndex index = m_tableView->indexAt(pos);
    if (!index.isValid()) return;

    QString id = getIdForRow(index.row());

    QAction* action1 = menu.addAction("Action 1");
    connect(action1, &QAction::triggered, [this, id]() {
        handleAction(id);
    });

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}
```

## Data Model Management

### Using QStandardItemModel

All windows use QStandardItemModel:

```cpp
void setupModel() {
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({"Col1", "Col2", "Col3"});
    m_tableView->setModel(m_model);
}
```

### Creating Row Items

```cpp
QList<QStandardItem*> createRowItems(const Data& data) {
    QList<QStandardItem*> items;

    auto* item1 = new QStandardItem(data.field1);
    item1->setEditable(false);  // Make read-only
    items.append(item1);

    auto* item2 = new QStandardItem(data.field2);
    item2->setEditable(false);
    items.append(item2);

    return items;
}
```

### Color Coding Items

```cpp
void setItemColor(QStandardItem* item, double value) {
    if (value > 0) {
        item->setForeground(QColor(0, 200, 0));  // Green
    } else if (value < 0) {
        item->setForeground(QColor(200, 0, 0));  // Red
    } else {
        item->setForeground(Qt::white);          // White
    }
}
```

## Performance Optimization

### Batch Updates

Reduce model updates:

```cpp
void updateMultipleRows(const QVector<Data>& dataList) {
    // Block signals during batch update
    m_model->blockSignals(true);

    for (const auto& data : dataList) {
        updateRow(data.id, data);
    }

    m_model->blockSignals(false);

    // Emit single update signal
    emit m_model->dataChanged(m_model->index(0, 0),
                             m_model->index(m_model->rowCount()-1,
                                          m_model->columnCount()-1));
}
```

### Lazy P&L Updates

Only update visible P&L values:

```cpp
void updateLastPrice(const QString& symbol, double price) {
    m_lastPrices[symbol] = price;

    // Only update if window is visible
    if (!isVisible()) {
        m_pendingUpdates.insert(symbol);
        return;
    }

    updatePLForSymbol(symbol);
}
```

## Testing

### Unit Tests

Test row management:

```cpp
TEST(OrderWindow, AddsNewOrder) {
    OrderWindow window;
    Order order;
    order.setOrderId("ORD123");
    order.setSymbol("AAPL");

    window.updateOrder("ACC1", order);

    EXPECT_EQ(window.getRowCount(), 1);
    EXPECT_TRUE(window.hasOrder("ORD123"));
}

TEST(OrderWindow, UpdatesExistingOrder) {
    OrderWindow window;
    Order order;
    order.setOrderId("ORD123");
    order.setStatus(Order::Status::OPN);

    window.updateOrder("ACC1", order);
    EXPECT_EQ(window.getRowCount(), 1);

    order.setStatus(Order::Status::FLL);
    window.updateOrder("ACC1", order);

    EXPECT_EQ(window.getRowCount(), 1);  // Same row updated
}
```

## Common Issues

### Rows Not Updating

**Problem**: Updates don't reflect in display

**Solutions**:
1. Check m_rowMap is updated correctly
2. Verify model's dataChanged signal is emitted
3. Ensure row/column indices are correct

### Color Coding Not Applied

**Problem**: Items show default color

**Solutions**:
1. Verify setForeground() is called
2. Check color values (0-255 range)
3. Ensure stylesheet doesn't override item colors

### Memory Leaks

**Problem**: Memory grows over time

**Solutions**:
1. Use Qt parent-child for QStandardItem
2. Clear maps when clearing display
3. Remove entries from tracking maps on deletion

## Related Components

- **GUIFrontend**: Container for these windows as dock widgets
- **MainAlgo**: Provides data updates
- **TSClient**: Source of order/position/balance data
- **Order/Position/Balance**: Data structures

## Related Documentation

- `../AGENTS.md`: Main GUI documentation
- `Doc/FRONTEND.md`: Frontend architecture
- `Doc/ARCHITECTURE.md`: System architecture
