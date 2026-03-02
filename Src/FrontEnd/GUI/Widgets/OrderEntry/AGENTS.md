# OrderEntry/ - Order Placement Widget - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/OrderEntry/`
**Purpose**: Complete order entry interface for placing trades
**Main Class**: OrderEntryWidget

The OrderEntryWidget provides a comprehensive interface for entering and submitting trading orders. It supports multiple order types, trade actions, and features the innovative "sticky price" functionality for automated limit price adjustments.

## Files

- **OrderEntryWidget.h** - Widget class definition
- **OrderEntryWidget.cpp** - Widget implementation

## Features

### Trade Actions

Four trade action types supported:
- **Buy**: Open long position or add to existing long
- **Sell**: Close long position or reduce long
- **Buy to Cover**: Close short position or reduce short
- **Sell Short**: Open short position or add to existing short

Radio button group ensures only one action selected at a time.

### Order Types

Supports all standard order types:
- **Market**: Execute immediately at best available price
- **Limit**: Execute only at specified price or better
- **Stop**: Trigger at stop price, execute as market
- **Stop Limit**: Trigger at stop price, execute as limit

Dropdown combo box for selection.

### Time in Force

Order duration options:
- **Day**: Valid until end of trading day
- **GTC (Good Till Cancelled)**: Valid until explicitly cancelled

### Sticky Price Feature

**Purpose**: Automatically adjust limit price based on real-time market depth

**Modes**:
1. **Aggressive Mode** (crosses spread for guaranteed fill):
   - Buy orders: `limit = best ask + offset`
   - Sell orders: `limit = best bid - offset`

2. **Passive Mode** (joins bid/ask, better price but may not fill):
   - Buy orders: `limit = best bid + offset`
   - Sell orders: `limit = best ask - offset`

**Configuration**:
- Enable/disable checkbox
- Mode selection (Aggressive/Passive) radio buttons
- Offset spinner (0.00 to 10.00, default 0.00)
- Visual feedback (green flash on price update)

**Update Logic**:

The sticky price sources bid/ask from Level 2 data:
- **Level 2 available** (`onMarketDepthUpdate`): uses `Level2` best bid/ask — called from `GUIFrontend::onCurrentHighlightedReceivedNewLevel2`

```cpp
/// Update sticky price from Level 2 market depth
void onMarketDepthUpdate(const QString& symbol, const Level2& level2);

private:
/// Calculate and apply sticky price from m_lastBestBid / m_lastBestAsk
void updateStickyPrice();
```

## Public Interface

### Constructor and Destructor

```cpp
/// Construct order entry widget
/// @param p_parent Parent widget (typically GUIFrontend)
explicit OrderEntryWidget(QWidget* p_parent = nullptr);

/// Destructor - saves widget state
~OrderEntryWidget();
```

### Initialization

```cpp
/// Set reference to parent GUIFrontend
/// Required for accessing account selection
/// @param guiFrontend Pointer to main GUI frontend
void setGUIFrontend(GUIFrontend* guiFrontend);
```

### Configuration Queries

```cpp
/// Check if order result popup is enabled
/// @return true if popup will show on order submission
bool isResultPopupEnabled() const;

/// Check if cancel-all confirmation is enabled
/// @return true if confirmation dialog will show before cancel all
bool isCancelAllConfirmationEnabled() const;
```

## Public Slots

### Account and Symbol Setup

```cpp
/// Update account list (called when accounts are loaded)
/// Populates internal account list for validation
/// @param accounts List of available trading accounts
void setAccounts(const QList<Account>& accounts);

/// Set symbol for order entry
/// Auto-filled when chart symbol changes
/// @param symbol Stock symbol (e.g., "AAPL")
void setSymbol(const QString& symbol);
```

### Quick Trade Actions

Keyboard shortcuts and programmatic order submission:

```cpp
/// Execute buy order with current settings
/// Validates inputs and submits market buy order
void executeBuyOrder();

/// Execute sell order with current settings
/// Validates inputs and submits market sell order
void executeSellOrder();

/// Execute buy-to-cover order with current settings
/// Validates inputs and submits market buy-to-cover order
void executeBuyToCoverOrder();

/// Execute sell-short order with current settings
/// Validates inputs and submits market sell-short order (open short position)
void executeSellToCoverOrder();
```

### Market Data Updates

```cpp
/// Handle L2 market depth update for sticky price
/// Only updates if sticky price is enabled and symbol matches
/// @param symbol Symbol of the update
/// @param level2 Level 2 data with bid/ask levels
void onMarketDepthUpdate(const QString& symbol, const Level2& level2);
```

## Signals

```cpp
/// Emitted when user submits an order
/// Connected to GUIFrontend which forwards to MainAlgo
/// @param order Complete order request with all parameters
void orderPlaced(const PlaceOrderRequest& order);
```

## Private Slots

### UI Event Handlers

```cpp
/// Handle order type selection change
/// Updates visibility of price input fields
/// @param index Selected order type index
void onOrderTypeChanged(int index);

/// Handle trade action radio button change
/// @param id Button ID from QButtonGroup
void onTradeActionChanged(int id);

/// Handle submit button click
/// Validates inputs and emits orderPlaced signal
void onSubmitClicked();
```

### Settings Persistence

All user preferences are automatically saved:

```cpp
void saveOrderTypeSetting(int index);
void saveDurationSetting(int index);
void saveQuantitySetting(int value);
void saveLimitPriceSetting(double value);
void saveStopPriceSetting(double value);
void saveTradeActionSetting(int id);
```

### Confirmation Options

```cpp
/// Handle confirmation checkbox toggle
/// Enables/disables order submission confirmation dialog
void onConfirmationCheckBoxToggled(bool checked);

/// Handle result popup checkbox toggle
/// Enables/disables order result notification popup
void onResultPopupCheckBoxToggled(bool checked);

/// Handle cancel-all confirmation checkbox toggle
/// Enables/disables confirmation before cancelling all orders
void onCancelAllConfirmationCheckBoxToggled(bool checked);
```

### Sticky Price Controls

```cpp
/// Handle sticky price enable/disable
/// Starts/stops automatic price updates
void onStickyCheckBoxToggled(bool checked);

/// Save sticky price enabled state
void saveStickyPriceSetting(bool checked);

/// Handle sticky mode change (aggressive/passive)
/// @param id Mode button ID
void onStickyModeChanged(int id);

/// Save sticky mode selection
void saveStickyModeSetting(int id);

/// Handle sticky offset value change
/// @param value New offset value (0.00 to 10.00)
void onStickyOffsetChanged(double value);

/// Save sticky offset setting
void saveStickyOffsetSetting(double value);
```

## Private Methods

### UI Setup

```cpp
/// Create and layout all UI components
/// Called from constructor
void setupUI();

/// Apply dark theme styling
/// Consistent with application theme
void setupStyles();
```

### Dynamic UI Updates

```cpp
/// Update visibility of limit/stop price fields
/// Based on selected order type
/// Limit orders: show limit price
/// Stop orders: show stop price
/// Stop Limit orders: show both
void updatePriceFieldsVisibility();
```

### State Management

```cpp
/// Load previously saved settings
/// Called during construction
/// Restores order type, quantity, prices, etc.
void loadSavedSettings();
```

### Order Processing

```cpp
/// Validate all input fields
/// Checks for empty/invalid values
/// @return true if all inputs valid, false otherwise
[[nodiscard]] bool validateInputs();

/// Build order request from current widget state
/// Gathers all input values into PlaceOrderRequest
/// @return Complete order request ready for submission
[[nodiscard]] PlaceOrderRequest buildOrderRequest();
```

### Sticky Price Logic

```cpp
/// Calculate and apply sticky price based on market depth
/// Implements aggressive/passive logic with offset
/// Triggers visual feedback (green flash)
void updateStickyPrice();
```

## UI Components

### Member Variables

```cpp
private:
    // Header
    QLabel* m_headerLabel;

    // Trade action selection
    QRadioButton* m_buyRadio;
    QRadioButton* m_buyToCoverRadio;
    QRadioButton* m_sellRadio;
    QRadioButton* m_sellToCoverRadio;
    QButtonGroup* m_tradeActionGroup;

    // Order parameters
    QComboBox* m_orderTypeCombo;
    QSpinBox* m_quantityInput;
    QDoubleSpinBox* m_limitPriceInput;
    QDoubleSpinBox* m_stopPriceInput;
    QComboBox* m_durationCombo;

    // Submission
    QPushButton* m_submitButton;

    // Sticky price controls
    QCheckBox* m_stickyCheckBox;
    QRadioButton* m_aggressiveRadio;
    QRadioButton* m_passiveRadio;
    QButtonGroup* m_stickyModeGroup;
    QDoubleSpinBox* m_stickyOffsetInput;

    // Options
    QCheckBox* m_confirmationCheckBox;
    QCheckBox* m_resultPopupCheckBox;
    QCheckBox* m_cancelAllConfirmationCheckBox;

    // State
    GUIFrontend* m_guiFrontend;
    QList<Account> m_accounts;
    QString m_currentSymbol;
    Level2 m_latestLevel2;
    bool m_resultPopupEnabled;
    bool m_cancelAllConfirmationEnabled;
```

## Validation Rules

### Required Fields

All fields must be filled:
- Symbol (auto-filled from chart)
- Trade action (radio button default selected)
- Order type (combo box default selected)
- Quantity (must be > 0)
- Account (selected from GUIFrontend dropdown)

### Conditional Requirements

- **Limit orders**: Limit price required (must be > 0.01)
- **Stop orders**: Stop price required (must be > 0.01)
- **Stop Limit orders**: Both limit and stop prices required

### Validation Error Handling

```cpp
bool OrderEntryWidget::validateInputs() {
    if (m_currentSymbol.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
                           tr("Please enter a symbol"));
        return false;
    }

    if (m_quantityInput->value() <= 0) {
        QMessageBox::warning(this, tr("Validation Error"),
                           tr("Quantity must be greater than 0"));
        m_quantityInput->setFocus();
        return false;
    }

    // Check price fields based on order type
    OrderType type = getSelectedOrderType();
    if (type == OrderType::Limit || type == OrderType::StopLimit) {
        if (m_limitPriceInput->value() < 0.01) {
            QMessageBox::warning(this, tr("Validation Error"),
                               tr("Limit price must be at least $0.01"));
            m_limitPriceInput->setFocus();
            return false;
        }
    }

    // ... more validation

    return true;
}
```

## Sticky Price Algorithm

### Aggressive Mode (Crosses Spread)

Ensures order fills immediately by crossing the spread:

```cpp
if (isAggressive) {
    if (isBuyOrder) {
        // Cross to ask side for guaranteed fill
        newPrice = quote.getBestAsk() + offset;
    } else {
        // Cross to bid side for guaranteed fill
        newPrice = quote.getBestBid() - offset;
    }
}
```

**Use Case**: When execution certainty is more important than price

### Passive Mode (Joins Bid/Ask)

Joins the bid or ask to get better price, but may not fill:

```cpp
if (isPassive) {
    if (isBuyOrder) {
        // Join bid side (better price, less certain fill)
        newPrice = quote.getBestBid() + offset;
    } else {
        // Join ask side (better price, less certain fill)
        newPrice = quote.getBestAsk() - offset;
    }
}
```

**Use Case**: When price is more important than execution certainty

### Visual Feedback

Price updates trigger a green flash animation:

```cpp
void updateStickyPrice() {
    // Calculate new price
    double newPrice = calculateStickyPrice();

    // Update price input
    m_limitPriceInput->setValue(newPrice);

    // Visual feedback (green flash)
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(m_limitPriceInput);
    m_limitPriceInput->setGraphicsEffect(effect);

    QPropertyAnimation* animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(0.3);
    animation->setEndValue(1.0);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}
```

## Order Submission Flow

```
User clicks "Submit Order" button
    │
    ▼
onSubmitClicked()
    │
    ├─ Validate all inputs
    │       │
    │       └─ If invalid: Show error, return
    │
    ├─ Build order request
    │       │
    │       ├─ Get selected account (from GUIFrontend)
    │       ├─ Get trade action (Buy/Sell/etc.)
    │       ├─ Get order type (Market/Limit/etc.)
    │       ├─ Get quantity
    │       ├─ Get prices (if applicable)
    │       └─ Get duration (Day/GTC)
    │
    ├─ Show confirmation dialog (if enabled)
    │       │
    │       └─ If cancelled: return
    │
    └─ emit orderPlaced(request)
            │
            ▼
    GUIFrontend receives signal
            │
            ├─ Log order submission
            │
            └─ Forward to MainAlgo
                    │
                    ▼
            MainAlgo::onOrderPlaced()
                    │
                    └─ TSClient::placeOrder()
```

## Settings Persistence

All widget settings are saved to `Settings` (QSettings wrapper):

```cpp
// Save settings
Settings::setValue("OrderEntry/OrderType", m_orderTypeCombo->currentIndex());
Settings::setValue("OrderEntry/Quantity", m_quantityInput->value());
Settings::setValue("OrderEntry/Duration", m_durationCombo->currentIndex());
Settings::setValue("OrderEntry/TradeAction", m_tradeActionGroup->checkedId());
Settings::setValue("OrderEntry/StickyEnabled", m_stickyCheckBox->isChecked());
Settings::setValue("OrderEntry/StickyMode", m_stickyModeGroup->checkedId());
Settings::setValue("OrderEntry/StickyOffset", m_stickyOffsetInput->value());

// Load settings
m_orderTypeCombo->setCurrentIndex(Settings::getValue("OrderEntry/OrderType", 0).toInt());
m_quantityInput->setValue(Settings::getValue("OrderEntry/Quantity", 100).toInt());
// ... etc
```

## Testing

### Unit Tests

Test validation logic:

```cpp
TEST(OrderEntryWidget, ValidatesQuantity) {
    OrderEntryWidget widget;
    widget.setSymbol("AAPL");
    widget.setQuantity(0);
    EXPECT_FALSE(widget.validate());

    widget.setQuantity(100);
    EXPECT_TRUE(widget.validate());
}

TEST(OrderEntryWidget, RequiresLimitPriceForLimitOrders) {
    OrderEntryWidget widget;
    widget.setOrderType(OrderType::Limit);
    widget.setLimitPrice(0.0);
    EXPECT_FALSE(widget.validate());

    widget.setLimitPrice(100.50);
    EXPECT_TRUE(widget.validate());
}
```

### Integration Tests

Test signal emission:

```cpp
TEST(OrderEntryWidget, EmitsOrderPlacedSignal) {
    OrderEntryWidget widget;
    QSignalSpy spy(&widget, &OrderEntryWidget::orderPlaced);

    widget.setSymbol("AAPL");
    widget.setQuantity(100);
    widget.submitOrder();

    EXPECT_EQ(spy.count(), 1);
    PlaceOrderRequest request = spy.at(0).at(0).value<PlaceOrderRequest>();
    EXPECT_EQ(request.symbol, "AAPL");
    EXPECT_EQ(request.quantity, 100);
}
```

## Common Issues

### Sticky Price Not Updating

**Problem**: Price doesn't update despite market data changes

**Solutions**:
1. Check sticky checkbox is enabled
2. Verify symbol matches current widget symbol
3. Ensure market data is valid (bid/ask > 0)
4. Check order type is Limit or Stop Limit
5. If in replay mode with L1-only data (no L2): bid/ask comes from `Level1.m_bid`/`Level1.m_ask` — to be wired in Phase 6

### Validation Failing

**Problem**: Submit button doesn't work

**Solutions**:
1. Check all required fields are filled
2. Verify quantity > 0
3. For limit orders, ensure limit price >= 0.01
4. Check account is selected in GUIFrontend

### Settings Not Persisting

**Problem**: Widget resets to defaults on restart

**Solutions**:
1. Verify Settings singleton is initialized
2. Check save signals are connected
3. Ensure destructor saves state

## Related Components

- **GUIFrontend**: Parent container, provides account selection
- **MainAlgo**: Receives order placement signals
- **TSClient**: Executes orders via TradeStation API (brokerage only)
- **MarketDepthTable**: Displays Level 2 market depth
- **Level2** (`Src/Core/Models/Level2.h`): Market depth data type for sticky price
- **PlaceOrder.h**: Order request data structures

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Widgets documentation
- `Doc/FRONTEND.md`: Frontend architecture
- `Doc/ARCHITECTURE.md`: System architecture
