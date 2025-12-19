# Migration from Qt Charts to QCustomPlot

## Overview

The StockPriceChart widget has been migrated from Qt Charts (QChart, QChartView, QCandlestickSeries) to the qcustomplot library (QCustomPlot, QCPFinancial).

## Changes Made

### 1. CMake Build System Updates

**Root CMakeLists.txt:**
- Removed `Qt6::Charts` dependency
- Added `Qt6::PrintSupport` dependency (required by qcustomplot)

**Src/CMakeLists.txt:**
- Added qcustomplot source files to the build (qcustomplot.cpp, qcustomplot.h)
- Added qcustomplot directory to include paths

### 2. StockPriceChart Implementation

**Removed Files:**
- `WheelEvent.cpp` - Zoom/pan logic consolidated into main implementation
- `Panning.cpp` - Panning logic consolidated into main implementation

**Modified Files:**
- `StockPriceChart.h` - Updated to use qcustomplot classes
- `StockPriceChart.cpp` - Complete rewrite using qcustomplot API

**Preserved Files:**
- `TimeFrameSelector.h/cpp` - Unchanged, still works with new implementation

### 3. Key Differences

#### Qt Charts → QCustomPlot Mapping

| Qt Charts | QCustomPlot | Notes |
|-----------|-------------|-------|
| QChart | QCustomPlot | Main plotting widget |
| QChartView | QCustomPlot | Replaces chart view |
| QCandlestickSeries | QCPFinancial | Financial/candlestick plotting |
| QLineSeries | QCPItemLine | For last price line |
| QGraphicsTextItem | QCPItemText | For price label |
| QValueAxis | QCPAxis | Axis management |

#### Preserved Features

✅ **Index-based bar positioning** - Still uses `indexToBar` and `timestampToIndex` maps  
✅ **Panning and zooming** - All modifier key combinations preserved:
  - Ctrl+Shift: Vertical panning
  - Alt: Horizontal panning
  - Ctrl: Horizontal zoom
  - Shift: Vertical zoom
  - No modifiers: Both axes zoom

✅ **Missing bars detection** - Automatic request when view extends beyond available data  
✅ **Signal/slot interface** - `requestMissingBars` signal maintained  
✅ **Dark theme styling** - Preserved with qcustomplot styling API  
✅ **TimeFrameSelector integration** - Still integrated at the top of the widget  

#### API Differences

**Data Updates:**
- Old: Individual QCandlestickSet objects added to series
- New: QCPFinancialData container updated in bulk via `updateCandlestickData()`

**Rendering:**
- Old: Automatic via Qt Charts
- New: Explicit `m_customPlot->replot()` calls after data changes

**Axis Ranges:**
- Old: `axisX->setRange(min, max)`
- New: `m_customPlot->xAxis->setRange(min, max)`

**Item Positioning:**
- Old: Scene-based coordinates with QGraphicsItems
- New: QCPItemPosition with axis-relative coordinates

## Benefits of QCustomPlot

1. **More control** - Direct access to rendering pipeline
2. **Better performance** - Optimized for large datasets
3. **Simpler API** - Less abstraction layers than Qt Charts
4. **Active development** - Well-maintained open source library
5. **Financial chart support** - Native OHLC/Candlestick support

## Testing Notes

The implementation preserves all external interfaces, so existing code that uses StockPriceChart should work without modifications. The chart behavior should be identical to the previous implementation.

## Future Enhancements

Potential improvements enabled by qcustomplot:
- Volume bars below the price chart (see example code)
- Multiple axis rects for different indicators
- Better tooltip support
- Export to various image formats
- OpenGL acceleration support
