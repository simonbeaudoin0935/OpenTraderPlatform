# Volume Chart Zoom Behavior

## Overview
The stock price chart widget now has different zoom behavior when using the mouse wheel over the volume chart area.

## Implementation
When the mouse cursor is positioned over the volume chart (the bottom chart showing trading volume), mouse wheel scrolling will zoom only the horizontal (time) axis, not the vertical (volume) axis.

## Behavior Details

### When Mouse is Over Volume Chart:
- **Default Scroll (no modifier keys)**: Zooms only horizontally (time axis)
  - Previously: Zoomed both horizontal and vertical axes
  - Now: Zooms only horizontal axis
- **Shift + Scroll**: Zooms only horizontally (time axis)
  - Previously: Zoomed only vertical axis
  - Now: Zooms only horizontal axis

### When Mouse is Over Price Chart (unchanged):
- **Default Scroll**: Zooms both axes
- **Shift + Scroll**: Zooms only vertical (price) axis
- **Ctrl + Scroll**: Zooms only horizontal (time) axis
- **Alt + Scroll**: Pans horizontally
- **Shift + Ctrl + Scroll**: Pans vertically

## Rationale
This change prevents the volume bars from becoming disproportionately large or small when scrolling over the volume chart. Since the volume chart shares the time axis with the price chart, horizontal zoom is still useful for zooming in/out on time periods. However, vertical zoom of volume bars is typically less useful and can distort the visual representation.

## Technical Details
- Uses QCustomPlot's `axisRectAt()` method to detect which axis rect the mouse is over
- Only affects zoom behavior when mouse is over the visible volume chart
- If volume chart is hidden, normal zoom behavior applies everywhere
- Debug logging added to help verify behavior: "Mouse wheel over volume chart - using horizontal-only zoom"

## Testing
To test this feature:
1. Run the L2Trader application with GUI enabled
2. Open a stock chart with volume chart visible
3. Position mouse over the volume chart area (bottom chart)
4. Scroll with mouse wheel - should see only horizontal zoom
5. Position mouse over the price chart area (top chart)
6. Scroll with mouse wheel - should see both horizontal and vertical zoom
7. Check debug logs for "Mouse wheel over volume chart" messages when scrolling over volume area

## Code Location
- File: `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp`
- Function: `StockPriceChart::wheelEvent(QWheelEvent* event)`
