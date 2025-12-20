# Session Background Rectangle Alignment Fix

## Problem Description

The session background rectangles (pre-market and after-hours) were not properly aligning with the correct x-axis indices in the price chart. Two distinct issues were identified:

1. **Timezone Mismatch**: Rectangles not appearing or appearing at wrong positions due to timestamp lookup failures
2. **Incorrect Clipping Logic**: Rectangles drawing across the entire screen when sessions extended beyond visible range

## Root Cause Analysis

### Issue 1: Timezone Mismatch

The first issue was a **timezone mismatch** in the timestamp-to-index lookup process:

#### How Bars Are Stored

In `Bar.cpp` (line 109), when bars are parsed from JSON:
```cpp
timeStamp = QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate)
            .toTimeZone(QTimeZone("America/New_York"));
```

All bar timestamps are stored with **America/New_York timezone**.

#### How Index Mapping Works

The chart uses a bidirectional index mapping system:
- `timestampToIndex`: Maps `QDateTime` → `int` (bar timestamp to chart index)
- `indexToBar`: Maps `int` → `Bar` (chart index to bar data)

The keys in `timestampToIndex` are QDateTime objects with **America/New_York timezone**.

#### The Bug (Issue 1)

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

### Issue 2: Incorrect Clipping Logic

After fixing the timezone issue, a second problem became apparent when panning and zooming.

#### The Bug (Issue 2)

In the original `drawBackgroundForTimeRange()` logic:
```cpp
// Original buggy code
qreal rectStartIndex = visibleMinIndex;  // Default to visible range
qreal rectEndIndex = visibleMaxIndex;

auto startIt = timestampToIndex.lowerBound(rangeStart);
if (startIt != timestampToIndex.end()) {
    qreal barIndex = static_cast<qreal>(startIt.value());
    if (barIndex > visibleMinIndex) {  // BUG: Wrong condition!
        rectStartIndex = barIndex;
    }
}
// Similar logic for endIndex
```

**The Problem**: The condition `if (barIndex > visibleMinIndex)` is backwards!

**Example scenario causing full-screen rectangle:**
- Visible range: indices 100-200
- Pre-market session: 4:00 AM - 9:30 AM (bar indices 50-380)
- `lowerBound(4:00 AM)` correctly returns bar at index 50
- Check: `50 > 100`? **NO** → keeps `rectStartIndex = 100` (visible min)
- `upperBound(9:30 AM)` correctly returns bar at index 380
- Check: `380 < 200`? **NO** → keeps `rectEndIndex = 200` (visible max)
- **Result**: Rectangle draws from 100 to 200 (entire visible screen!)

The logic was trying to be "smart" by only using found indices if they were within the visible range, but this backfired. When a session extended beyond the visible range, it would fall back to the defaults (entire visible range), causing full-screen overlays.

## The Fixes

### Fix 1: Timezone Consistency (Commit b26cafc)

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
- Rectangles can now be looked up correctly (but see Fix 2 for complete solution)

### Fix 2: Correct Clipping Logic (Commit e5dfcab)

**Always use the actual session bar indices, then clip to visible range:**

```cpp
// Find first bar at or after rangeStart
auto startIt = timestampToIndex.lowerBound(rangeStart);
if (startIt == timestampToIndex.end()) {
    // Session entirely before available data - don't draw
    return;
}

// Find last bar at or before rangeEnd
auto endIt = timestampToIndex.upperBound(rangeEnd);
if (endIt == timestampToIndex.begin()) {
    // Session entirely after available data - don't draw
    return;
}
--endIt;

// Get the actual bar indices for this session
qreal sessionStartIndex = static_cast<qreal>(startIt.value());
qreal sessionEndIndex = static_cast<qreal>(endIt.value()) + 1.0;

// Only draw if session intersects with visible area
if (sessionEndIndex <= visibleMinIndex || sessionStartIndex >= visibleMaxIndex) {
    return;  // No intersection
}

// Clip to visible range
qreal clippedStart = qMax(sessionStartIndex, visibleMinIndex);
qreal clippedEnd = qMin(sessionEndIndex, visibleMaxIndex);

// Draw rectangle from clippedStart to clippedEnd
```

**Key changes:**
1. Always get the actual session indices from the map lookups
2. Check for intersection with visible range
3. Only then clip to visible boundaries
4. No fallback to visible range defaults

**Example with new logic:**
- Visible range: indices 100-200
- Pre-market session: 4:00 AM - 9:30 AM (bar indices 50-380)
- `sessionStartIndex = 50`, `sessionEndIndex = 381`
- Check intersection: `381 <= 100`? NO, `50 >= 200`? NO → session intersects
- Clip: `clippedStart = max(50, 100) = 100`, `clippedEnd = min(381, 200) = 200`
- **Result**: Rectangle draws from 100 to 200, but **only if this is a valid session range**

Now when multiple sessions are present (e.g., pre-market from 50-330, after-hours from 390-480), each gets its own correctly clipped rectangle instead of overlapping full-screen rectangles.

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
