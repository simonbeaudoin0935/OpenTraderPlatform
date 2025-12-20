# Session Background Rectangle Alignment Fix

## Problem Description

The session background rectangles (pre-market and after-hours) were not properly aligning with the correct x-axis indices in the price chart. These colored rectangles are meant to highlight the extended trading hours visually, but they were appearing at incorrect positions or not appearing at all.

## Root Cause Analysis

The issue was a **timezone mismatch** in the timestamp-to-index lookup process:

### How Bars Are Stored

In `Bar.cpp` (line 109), when bars are parsed from JSON:
```cpp
timeStamp = QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate)
            .toTimeZone(QTimeZone("America/New_York"));
```

All bar timestamps are stored with **America/New_York timezone**.

### How Index Mapping Works

The chart uses a bidirectional index mapping system:
- `timestampToIndex`: Maps `QDateTime` → `int` (bar timestamp to chart index)
- `indexToBar`: Maps `int` → `Bar` (chart index to bar data)

The keys in `timestampToIndex` are QDateTime objects with **America/New_York timezone**.

### The Bug

In `StockPriceChart.cpp::updateSessionBackgrounds()` (lines 417-418), before the fix:
```cpp
QDateTime localSessionStart = sessionStart.toLocalTime();
QDateTime localSessionEnd = sessionEnd.toLocalTime();

// Pass local time to drawBackgroundForTimeRange
drawBackgroundForTimeRange(localSessionStart, localSessionEnd, ...);
```

In `drawBackgroundForTimeRange()` (lines 484, 494):
```cpp
// Try to find bar at or after rangeStart
auto startIt = timestampToIndex.lowerBound(rangeStart);  // FAILS!

// Try to find bar at or before rangeEnd  
auto endIt = timestampToIndex.upperBound(rangeEnd);      // FAILS!
```

**The Problem**: QMap uses QDateTime's comparison operators, which are timezone-aware. Even if two QDateTime objects represent the same absolute time:
- `QDateTime::fromString("2024-01-01T09:30:00", Qt::ISODate).toTimeZone(QTimeZone("America/New_York"))` 
- `QDateTime::fromString("2024-01-01T14:30:00", Qt::ISODate).toTimeZone(QTimeZone("UTC"))`  (same instant in time)

They are **NOT equal** for map lookups because they have different timezone representations.

Result: The lookups would fail to find matching bars, causing:
1. `rectStartIndex` and `rectEndIndex` to fall back to defaults
2. Rectangles to span the entire visible range or not appear at all
3. Incorrect alignment with actual pre/after-market bars

## The Fix

**Remove the timezone conversion** in `updateSessionBackgrounds()`:

```cpp
// Keep NY timezone to match bar timestamps stored in timestampToIndex map
// Draw one rectangle for the entire session
if (isPreMarket) {
    drawBackgroundForTimeRange(sessionStart, sessionEnd,  // NY timezone
                              QColor(255, 165, 0, 180), m_preMarketRects);
}
else if (isAfterHours) {
    drawBackgroundForTimeRange(sessionStart, sessionEnd,  // NY timezone
                              QColor(138, 43, 226, 180), m_afterHoursRects);
}
```

Now:
- Session times stay in **America/New_York timezone**
- Map lookups in `drawBackgroundForTimeRange()` succeed
- Rectangles align correctly with the bar indices

## Testing Recommendations

When testing this fix, verify:

1. **Pre-market rectangles** (orange, 180 alpha):
   - Appear from 4:00 AM - 9:30 AM ET
   - Align with bars during those hours
   - Span the correct index range

2. **After-hours rectangles** (violet, 180 alpha):
   - Appear from 4:00 PM - 8:00 PM ET
   - Align with bars during those hours
   - Span the correct index range

3. **Regular hours** (9:30 AM - 4:00 PM ET):
   - No background rectangles
   - Only candlestick bars visible

4. **Edge cases**:
   - Zoom in/out: rectangles should update correctly
   - Pan left/right: rectangles should update correctly
   - Multi-day view: each day should have its own session rectangles
   - DST transitions: rectangles should still align correctly

## Related Files

- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp`
  - `updateSessionBackgrounds()`: Calculates session time ranges
  - `drawBackgroundForTimeRange()`: Converts times to indices and draws rectangles
  
- `Src/Clients/TSClient/MarketData/Bars/Bar.cpp`
  - `Bar::Bar(const QJsonObject&)`: Sets bar timestamps with NY timezone

- `Src/Misc/MarketHours.h/cpp`
  - Defines pre-market, regular, and after-hours periods
  - All operations work in America/New_York timezone

## Key Takeaway

When working with QDateTime in maps or comparisons, **timezone consistency is critical**. Always ensure that all QDateTime objects being compared use the same timezone representation, even if they could represent the same absolute time.
