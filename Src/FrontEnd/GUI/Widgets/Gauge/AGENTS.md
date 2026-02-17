# Gauge/ - Circular Gauge Widget - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Widgets/Gauge/`
**Purpose**: Circular gauge widget for displaying numerical metrics with visual indicators
**Main Class**: Gauge

The Gauge widget provides a circular, analog-style gauge for displaying real-time metrics. It's used throughout the GUI to show trading indicators like BAI, DWP, OBLR, and QRR.

## Files

- **Gauge.h** - Gauge widget class definition
- **Gauge.cpp** - Gauge widget implementation with custom painting

## Features

- Circular gauge visualization
- Configurable value range (min/max)
- Color-coded zones (green/yellow/red)
- Needle indicator showing current value
- Label display (metric name)
- Smooth value transitions
- Custom painting using QPainter
- Dark theme integration

## Public Interface

### Constructors

```cpp
/// Construct gauge with empty label
/// @param parent Parent widget
explicit Gauge(QWidget* parent = nullptr);

/// Construct gauge with specified label
/// @param label Metric name (e.g., "BAI", "DWP")
/// @param parent Parent widget
explicit Gauge(const QString& label, QWidget* parent = nullptr);
```

### Configuration

```cpp
/// Set the value range for the gauge
/// Determines the scale and color zones
/// @param min Minimum value (left edge)
/// @param max Maximum value (right edge)
void setRange(double min, double max);

/// Get the current label
/// @return Label string
QString label() const;

/// Set the gauge label
/// @param label Metric name to display
void setLabel(const QString& label);
```

## Public Slots

```cpp
/// Update gauge value and trigger repaint
/// Value is clamped to [min, max] range
/// Emits valueChanged signal
/// @param value New value to display
void setValue(double value);
```

## Signals

```cpp
/// Emitted when gauge value changes
/// @param value New value
void valueChanged(double value);
```

## Qt Property System

```cpp
Q_PROPERTY(QString label READ label WRITE setLabel)
```

Allows gauge label to be set in Qt Designer:
- Drag Gauge widget onto form
- Set "label" property in property editor
- Label appears on gauge

## Protected Methods

```cpp
/// Custom painting for gauge visualization
/// Draws all gauge components
/// @param event Paint event (contains region to repaint)
void paintEvent(QPaintEvent* event) override;
```

## Private Methods

### Drawing Components

```cpp
/// Draw gauge background (dark circle)
/// @param painter QPainter instance
void drawBackground(QPainter& painter);

/// Draw color-coded bar (green/yellow/red zones)
/// Shows value range with color gradient
/// @param painter QPainter instance
void drawBar(QPainter& painter);

/// Draw needle indicator pointing to current value
/// @param painter QPainter instance
void drawIndicator(QPainter& painter);

/// Draw tick marks around gauge perimeter
/// @param painter QPainter instance
void drawTicks(QPainter& painter);

/// Draw center logo or label
/// @param painter QPainter instance
void drawCenterLogo(QPainter& painter);
```

## Member Variables

```cpp
private:
    double m_value;      // Current value
    double m_minValue;   // Minimum value (default: 0.0)
    double m_maxValue;   // Maximum value (default: 100.0)
    QString m_label;     // Metric name
```

## Gauge Visualization

### Layout

```
        ┌─────────────┐
        │   "BAI"     │  ← Label
        │             │
        │      ╱      │  ← Needle
        │    ●───     │  ← Center point
        │   ╱         │
        │ ╱           │
        └─────────────┘
```

### Color Zones

Default color mapping (can be customized):

- **Green** (0-33%): Good/healthy range
- **Yellow** (33-66%): Warning range
- **Red** (66-100%): Critical range

```cpp
void Gauge::drawBar(QPainter& painter) {
    // Calculate angle for each zone
    double greenAngle = (0.33 * 270);   // First 33%
    double yellowAngle = (0.33 * 270);  // Next 33%
    double redAngle = (0.34 * 270);     // Last 34%
    
    // Draw green zone
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 200, 0));
    painter.drawPie(rect, startAngle, greenAngle);
    
    // Draw yellow zone
    painter.setBrush(QColor(200, 200, 0));
    painter.drawPie(rect, startAngle + greenAngle, yellowAngle);
    
    // Draw red zone
    painter.setBrush(QColor(200, 0, 0));
    painter.drawPie(rect, startAngle + greenAngle + yellowAngle, redAngle);
}
```

### Needle Calculation

Needle position based on current value:

```cpp
void Gauge::drawIndicator(QPainter& painter) {
    // Normalize value to 0-1 range
    double normalized = (m_value - m_minValue) / (m_maxValue - m_minValue);
    normalized = qBound(0.0, normalized, 1.0);  // Clamp to range
    
    // Calculate angle (gauge spans 270 degrees)
    // Start at -225 degrees (bottom-left), end at 45 degrees (bottom-right)
    double angle = -225 + (normalized * 270);
    double radians = qDegreesToRadians(angle);
    
    // Calculate needle endpoint
    QPointF center(width() / 2, height() / 2);
    double radius = qMin(width(), height()) / 2.5;
    QPointF endPoint(
        center.x() + radius * cos(radians),
        center.y() + radius * sin(radians)
    );
    
    // Draw needle
    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(center, endPoint);
    
    // Draw center circle
    painter.setBrush(Qt::white);
    painter.drawEllipse(center, 5, 5);
}
```

## Usage Examples

### In Code

```cpp
// Create gauge
Gauge* baiGauge = new Gauge("BAI", this);
baiGauge->setRange(-100, 100);  // BAI ranges from -100 to 100
baiGauge->setValue(0);           // Start at neutral

// Update value
baiGauge->setValue(42.5);  // Update to 42.5

// Connect to signal
connect(baiGauge, &Gauge::valueChanged, this, [](double value) {
    qDebug() << "Gauge value changed to:" << value;
});
```

### In Qt Designer (GUIFrontend.ui)

```xml
<widget class="Gauge" name="baiGauge" native="true">
    <property name="label">
        <string>BAI</string>
    </property>
</widget>
```

### Common Use Cases

**BAI (Bid-Ask Imbalance)**:
```cpp
Gauge* baiGauge = new Gauge("BAI");
baiGauge->setRange(-100, 100);  // -100 (heavy ask) to 100 (heavy bid)
```

**DWP (Depth-Weighted Price)**:
```cpp
Gauge* dwpGauge = new Gauge("DWP");
dwpGauge->setRange(0, 200);  // Price range
```

**OBLR (Order Book Level Ratio)**:
```cpp
Gauge* oblrGauge = new Gauge("OBLR");
oblrGauge->setRange(0, 10);  // Ratio from 0 to 10
```

**QRR (Quote Refresh Rate)**:
```cpp
Gauge* qrrGauge = new Gauge("QRR");
qrrGauge->setRange(0, 1000);  // Updates per second
```

## Painting Performance

### Anti-Aliasing

Enable smooth rendering:

```cpp
void Gauge::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  // Smooth edges
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // ... draw gauge components
}
```

### Double Buffering

Qt automatically double-buffers widgets, but for complex painting:

```cpp
void Gauge::paintEvent(QPaintEvent* event) {
    // Paint to pixmap first (if needed)
    QPixmap buffer(size());
    QPainter bufferPainter(&buffer);
    
    // ... draw to buffer
    
    bufferPainter.end();
    
    // Draw buffer to widget
    QPainter painter(this);
    painter.drawPixmap(0, 0, buffer);
}
```

### Update Optimization

Only repaint when value changes:

```cpp
void Gauge::setValue(double value) {
    // Clamp to range
    value = qBound(m_minValue, value, m_maxValue);
    
    // Only update if value changed
    if (qFuzzyCompare(m_value, value)) {
        return;  // No change, skip update
    }
    
    m_value = value;
    emit valueChanged(value);
    update();  // Trigger repaint
}
```

## Styling and Theming

### Dark Theme Integration

Gauge colors are designed for dark background:

```cpp
void Gauge::drawBackground(QPainter& painter) {
    // Dark background (matches application theme)
    painter.setBrush(QColor(53, 53, 53));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect());
}
```

### Custom Colors

To customize gauge colors, modify `drawBar()`:

```cpp
// Custom color scheme
QColor lowColor(0, 150, 255);    // Blue for low
QColor midColor(255, 150, 0);    // Orange for mid
QColor highColor(255, 50, 50);   // Red for high
```

## Testing

### Unit Tests

Test value clamping:

```cpp
TEST(Gauge, ClampsValueToRange) {
    Gauge gauge;
    gauge.setRange(0, 100);
    
    gauge.setValue(-50);
    EXPECT_EQ(gauge.getValue(), 0);  // Clamped to min
    
    gauge.setValue(150);
    EXPECT_EQ(gauge.getValue(), 100);  // Clamped to max
}
```

Test signal emission:

```cpp
TEST(Gauge, EmitsValueChangedSignal) {
    Gauge gauge;
    QSignalSpy spy(&gauge, &Gauge::valueChanged);
    
    gauge.setValue(42.5);
    
    EXPECT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.at(0).at(0).toDouble(), 42.5);
}
```

### Visual Testing

Verify rendering:

```cpp
// Create test window with gauge
QWidget window;
Gauge* gauge = new Gauge("TEST", &window);
gauge->setRange(0, 100);

// Test various values
gauge->setValue(0);    // Minimum
gauge->setValue(50);   // Middle
gauge->setValue(100);  // Maximum

window.show();
// Manually verify visual appearance
```

## Common Issues

### Needle Not Visible

**Problem**: Needle doesn't appear on gauge

**Solutions**:
1. Check value is within range
2. Verify paintEvent() is being called (add debug output)
3. Ensure needle color contrasts with background
4. Check needle length calculation

### Value Not Updating

**Problem**: setValue() doesn't update display

**Solutions**:
1. Verify update() is called after value change
2. Check for qFuzzyCompare() blocking updates
3. Ensure value is different from previous

### Performance Issues

**Problem**: Gauge updates cause lag

**Solutions**:
1. Throttle setValue() calls (max 60 FPS)
2. Use double buffering for complex painting
3. Disable anti-aliasing if not needed
4. Cache rendered components

## Future Enhancements

Possible improvements (not yet implemented):

1. **Animated Transitions**: Smooth needle movement
2. **Value Text Display**: Show numeric value on gauge
3. **Min/Max Labels**: Display range endpoints
4. **Custom Zones**: Configurable color zone boundaries
5. **Tick Labels**: Show values at tick marks
6. **Logarithmic Scale**: For non-linear ranges

## Related Components

- **GUIFrontend**: Uses multiple gauges for metrics
- **MainAlgo**: Calculates and provides metric values
- **Market indicators**: BAI, DWP, OBLR, QRR calculations

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Widgets documentation
- `Doc/FRONTEND.md`: Frontend architecture
- Qt Documentation: QPainter, QWidget::paintEvent()
