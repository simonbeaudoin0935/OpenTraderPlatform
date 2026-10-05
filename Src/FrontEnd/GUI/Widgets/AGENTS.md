# Widgets/ - Reusable GUI Widget Components - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/`
**Purpose**: Reusable, self-contained widget components for the GUI
**Pattern**: Composition - widgets can be embedded in any parent container

The Widgets folder contains specialized UI components that can be reused throughout the application. Each widget is designed to be self-contained with its own data handling and visual presentation.

## Widget Components

### OrderEntry/ (Subdirectory)

**Purpose**: Order placement interface widget

See `OrderEntry/AGENTS.md` for detailed documentation.

**Quick Overview**:
- Trade action selection (Buy/Sell/BuyToCover/SellShort)
- Order type selection (Market/Limit/Stop/StopLimit)
- Quantity and price inputs
- Time in force selection (Day/GTC)
- Sticky price feature (aggressive/passive modes)
- Account selection integration
- Real-time validation

---

### RiskStatus/ (Subdirectory)

**Purpose**: Compact risk headroom and lock-state display

See `RiskStatus/RiskStatusWidget.h` and `RiskStatus/RiskStatusWidget.cpp`.

**Quick Overview**:
- Horizontal budget bar for drawdown headroom (`remaining / limit`)
- Remaining budget in USD plus drawdown basis reference/current metrics
- Status text for `ready`, `cooldown`, or `locked` states
- Amber/red threshold coloring based on configured usage percentages
- Embedded above Time & Sales in the Trade tab right column

---

### MarketDepth/ (Subdirectory)

**Purpose**: Level 2 market depth visualization

See `MarketDepth/AGENTS.md` for detailed documentation.

**Quick Overview**:
- Bid/ask price levels display
- Size and count per level
- Color-coded bid (green) and ask (red)
- DWP (Depth-Weighted Price) indicator
- Spread calculation
- Real-time updates (throttled to 100ms)
- Custom table view with formatting

---

### Gauge/ (Subdirectory)

**Purpose**: Circular gauge widgets for metrics visualization

See `Gauge/AGENTS.md` for detailed documentation.

**Quick Overview**:
- Circular gauge with needle indicator
- Min/max/current value display
- Color-coded ranges (green/yellow/red)
- Smooth animations
- Used for BAI, DWP, OBLR, QRR metrics
- Custom painting with QPainter

---

## Widget Design Principles

### Self-Contained Components

Each widget should be self-contained:

```cpp
class MyWidget : public QWidget {
    Q_OBJECT

public:
    explicit MyWidget(QWidget* parent = nullptr);

    // Public interface for setting data
    void setData(const Data& data);

    // Public interface for getting widget state
    Data getData() const;

signals:
    // Emit signals for user interactions
    void dataChanged(const Data& newData);
    void actionTriggered();

private:
    void setupUI();       // Create internal widgets
    void setupStyles();   // Apply styling
    void updateDisplay(); // Update visual presentation

    // Internal state
    Data m_currentData;

    // Internal UI components
    QLabel* m_label;
    QPushButton* m_button;
};
```

### Reusability

Widgets should be reusable in different contexts:

```cpp
// Can be used in main window
OrderEntryWidget* orderEntry = new OrderEntryWidget(mainWindow);
mainLayout->addWidget(orderEntry);

// Can be used in a dialog
QDialog* dialog = new QDialog();
OrderEntryWidget* orderEntry = new OrderEntryWidget(dialog);
dialogLayout->addWidget(orderEntry);

// Can be used in another widget
class TradingPanel : public QWidget {
    OrderEntryWidget* m_orderEntry;
    MarketDepthTable* m_marketDepth;
};
```

### Signal-Based Communication

Widgets should communicate via signals/slots, not direct coupling:

```cpp
// ✓ CORRECT: Emit signals for actions
class MyWidget : public QWidget {
signals:
    void actionRequested(const Action& action);
};

// In parent:
connect(widget, &MyWidget::actionRequested,
        this, &Parent::handleAction);

// ✗ WRONG: Direct coupling to other components
class MyWidget : public QWidget {
    void onButtonClicked() {
        MainAlgo::getInstance()->performAction();  // ❌ Tight coupling
    }
};
```

### Responsive Layout

Widgets should handle resizing gracefully:

```cpp
void MyWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // Adjust layout based on new size
    if (width() < COMPACT_THRESHOLD) {
        setCompactLayout();
    } else {
        setNormalLayout();
    }
}
```

## Common Widget Patterns

### Header with Title

Many widgets have a header section:

```cpp
void setupHeaderUI() {
    QLabel* headerLabel = new QLabel("WIDGET TITLE", this);
    QFont headerFont;
    headerFont.setPointSize(12);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(headerLabel);
}
```

### Dark Theme Styling

Apply consistent dark theme:

```cpp
void setupStyles() {
    setStyleSheet(R"(
        QWidget {
            background-color: #353535;
            color: white;
        }
        QLabel {
            color: white;
        }
        QPushButton {
            background-color: #454545;
            color: white;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 5px;
        }
        QPushButton:hover {
            background-color: #555555;
        }
        QPushButton:pressed {
            background-color: #656565;
        }
    )");
}
```

### Input Validation

Validate user input before accepting:

```cpp
bool MyWidget::validate() {
    if (m_input->text().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
                           tr("Input cannot be empty"));
        m_input->setFocus();
        return false;
    }

    if (m_quantityInput->value() <= 0) {
        QMessageBox::warning(this, tr("Validation Error"),
                           tr("Quantity must be greater than 0"));
        m_quantityInput->setFocus();
        return false;
    }

    return true;
}
```

### Update Throttling

Throttle high-frequency updates:

```cpp
class MyWidget : public QWidget {
private:
    QTimer* m_updateThrottle;
    Data m_pendingData;

    void setupThrottling() {
        m_updateThrottle = new QTimer(this);
        m_updateThrottle->setInterval(100);  // 100ms throttle
        m_updateThrottle->setSingleShot(true);

        connect(m_updateThrottle, &QTimer::timeout,
                this, &MyWidget::updateDisplay);
    }

public slots:
    void onDataReceived(const Data& data) {
        m_pendingData = data;
        if (!m_updateThrottle->isActive()) {
            updateDisplay();
            m_updateThrottle->start();
        }
    }
};
```

## Widget Lifecycle

### Construction

```cpp
MyWidget::MyWidget(QWidget* parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_button(new QPushButton(this))
{
    setupUI();
    setupStyles();
    setupConnections();

    // Load saved state if applicable
    restoreState();
}
```

### Destruction

```cpp
MyWidget::~MyWidget() {
    // Save state if applicable
    saveState();

    // Qt handles child widget deletion automatically
    // Manual cleanup only needed for non-QObject resources
}
```

### State Persistence

Save/restore widget state:

```cpp
void MyWidget::saveState() {
    Settings::setValue("MyWidget/Geometry", saveGeometry());
    Settings::setValue("MyWidget/LastValue", m_input->text());
}

void MyWidget::restoreState() {
    restoreGeometry(Settings::getValue("MyWidget/Geometry"));
    m_input->setText(Settings::getValue("MyWidget/LastValue").toString());
}
```

## Custom Painting

For widgets with custom graphics (like Gauge):

```cpp
void MyWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter.fillRect(rect(), QColor(53, 53, 53));

    // Draw custom content
    painter.setPen(Qt::white);
    painter.drawLine(0, height()/2, width(), height()/2);

    // ... more custom painting
}
```

## Accessibility

### Keyboard Navigation

Support keyboard navigation:

```cpp
void setupKeyboardNavigation() {
    // Set tab order
    setTabOrder(m_firstInput, m_secondInput);
    setTabOrder(m_secondInput, m_submitButton);

    // Set focus policy
    m_firstInput->setFocusPolicy(Qt::StrongFocus);

    // Add keyboard shortcuts
    QShortcut* submitShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(submitShortcut, &QShortcut::activated,
            this, &MyWidget::onSubmitClicked);
}
```

### Screen Reader Support

Provide accessible names:

```cpp
void setupAccessibility() {
    m_input->setAccessibleName("Order Quantity Input");
    m_button->setAccessibleDescription("Submit order for execution");
}
```

## Testing Widgets

### Unit Testing

Test widget logic independently:

```cpp
TEST(OrderEntryWidget, ValidatesRequiredFields) {
    OrderEntryWidget widget;

    // Test empty symbol validation
    widget.setSymbol("");
    EXPECT_FALSE(widget.validate());

    // Test valid input
    widget.setSymbol("AAPL");
    widget.setQuantity(100);
    EXPECT_TRUE(widget.validate());
}
```

### Integration Testing

Test widget in application context:

```cpp
TEST(OrderEntryWidget, EmitsCorrectSignal) {
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

## Performance Considerations

### Minimize Redraws

Only redraw when necessary:

```cpp
void MyWidget::updateValue(double value) {
    if (qFuzzyCompare(m_value, value)) {
        return;  // No change, skip update
    }

    m_value = value;
    update();  // Trigger repaint
}
```

### Use Double Buffering

For complex custom painting:

```cpp
void MyWidget::paintEvent(QPaintEvent* event) {
    // Paint to pixmap first
    QPixmap buffer(size());
    buffer.fill(Qt::transparent);

    QPainter bufferPainter(&buffer);
    // ... draw to buffer
    bufferPainter.end();

    // Then paint buffer to widget
    QPainter painter(this);
    painter.drawPixmap(0, 0, buffer);
}
```

### Cache Expensive Calculations

Cache computed values:

```cpp
class MyWidget : public QWidget {
private:
    mutable QCache<int, QPixmap> m_renderCache;

    const QPixmap& getCachedRender(int state) {
        if (QPixmap* cached = m_renderCache.object(state)) {
            return *cached;
        }

        QPixmap* rendered = new QPixmap(renderState(state));
        m_renderCache.insert(state, rendered);
        return *rendered;
    }
};
```

## Common Issues and Solutions

### Widget Not Updating

**Problem**: Widget doesn't reflect data changes

**Solution**: Ensure update() or repaint() is called:
```cpp
void setData(const Data& data) {
    m_data = data;
    update();  // Trigger repaint
}
```

### Signals Not Connecting

**Problem**: Signal/slot connections fail silently

**Solution**: Use Qt::UniqueConnection and check return value:
```cpp
bool connected = connect(sender, &Sender::signal,
                        this, &MyWidget::slot,
                        Qt::UniqueConnection);
if (!connected) {
    qWarning() << "Failed to connect signal";
}
```

### Memory Leaks

**Problem**: Widgets not cleaned up

**Solution**: Ensure proper parent-child relationships:
```cpp
// ✓ CORRECT: Child automatically deleted when parent is deleted
QLabel* label = new QLabel(this);  // 'this' is parent

// ✗ WRONG: No parent, manual deletion required
QLabel* label = new QLabel();  // Memory leak if not deleted
```

## Adding a New Widget

To add a new widget to the application:

1. **Create Widget Class Files**:
```bash
touch Src/FrontEnd/GUI/Widgets/MyWidget/MyWidget.h
touch Src/FrontEnd/GUI/Widgets/MyWidget/MyWidget.cpp
```

2. **Implement Widget**:
```cpp
// MyWidget.h
class MyWidget : public QWidget {
    Q_OBJECT
public:
    explicit MyWidget(QWidget* parent = nullptr);
signals:
    void dataChanged();
private:
    void setupUI();
};
```

3. **Add to Parent (e.g., GUIFrontend.ui)** if used in main window:
```xml
<customwidget>
 <class>MyWidget</class>
 <extends>QWidget</extends>
 <header>GUI/Widgets/MyWidget/MyWidget.h</header>
</customwidget>
```

4. **Build System** (automatic with GLOB_RECURSE):
CMake will automatically detect the new files.

## Related Components

- **GUIFrontend**: Main window containing widgets
- **Widgets/**: Window-type components (OrderWidget, PositionWidget, etc.)
- **Tabs/**: Tab components for the main tab widget
- **StockPriceChart/**: Specialized chart widget

## Related Documentation

- `../AGENTS.md`: Main GUI documentation
- `OrderEntry/AGENTS.md`: Order entry widget details
- `MarketDepth/AGENTS.md`: Market depth widget details
- `Gauge/AGENTS.md`: Gauge widget details
- `Doc/FRONTEND.md`: Complete frontend architecture
