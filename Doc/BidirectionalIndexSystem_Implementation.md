# Bidirectional Index System Implementation

## Overview

This document describes the implementation of the bidirectional index system for the StockPriceChart widget, which resolves performance issues when loading historical data.

## Problem Statement

Previously, when users panned left to view historical data, the chart would request missing bars from the cache. Upon receiving these bars, the system would call `rebuildIndexMapping()`, which:

1. Cleared all existing index mappings
2. Rebuilt the entire mapping from scratch starting at index 0
3. Shifted all existing bar indices to the right to accommodate new historical bars

This resulted in **O(n) time complexity** for inserting m historical bars, where n is the number of existing bars.

## Solution: Bidirectional Index System

### Concept

The bidirectional index system treats the first bar received (when the app starts or a symbol is loaded) as the **origin** at **index 0**. From this origin:

- **Future/Live bars** extend in the **positive direction**: 1, 2, 3, 4...
- **Historical bars** extend in the **negative direction**: -1, -2, -3, -4...

### Visual Representation

```
Historical ← | Origin | → Future/Live
       -3  -2  -1   0   1   2   3   4
       ↑   ↑   ↑    ↑   ↑   ↑   ↑   ↑
     9:27 9:28 9:29 9:30 9:31 9:32 9:33 9:34
                     ↑
              First bar received
```

## Key Implementation Changes

### 1. New Method: `addHistoricalBarsToIndexMapping()`

```cpp
void StockPriceChart::addHistoricalBarsToIndexMapping(const QVector<Bar>& bars);
```

- Processes historical bars in reverse order (newest to oldest)
- Assigns negative indices going backwards from the current minimum index
- **Time complexity**: O(m) where m = number of new bars
- **No recomputation** of existing indices

### 2. Modified: `onRequestedMissingBarsReceived()`

- **Before**: Called `updateChart()` which triggered full rebuild via `rebuildIndexMapping()`
- **After**: Calls `addHistoricalBarsToIndexMapping()` for incremental updates
- Adds candlesticks and void bars directly to series (no full rebuild)
- Preserves view state across the operation

### 3. Updated: `rebuildIndexMapping()`

- Now preserves the existing index origin (index 0) when rebuilding
- If the timestamp at index 0 still exists, maintains its position
- Only performs full rebuild when absolutely necessary (initialization, out-of-order bars)

### 4. Removed Index Constraints

Removed all occurrences of `qMax(0, index)` that prevented negative indices:
- `handleMouseButtonPress()` (right-click reset)
- `handlePanning()`
- `handleHorizontalPanning()`
- `handleHorizontalZoom()`
- `handleBothAxesZoom()`
- `handleClosedBar()` (initial view setup)
- `handleOpenBar()` (initial view setup)
- `updateChart()` (initial view setup)

### 5. Updated Missing Bar Detection

Changed from:
```cpp
if (newMin < 0 && !completedBars.isEmpty()) {
    // Request missing bars
}
```

To:
```cpp
if (!completedBars.isEmpty() && !indexToTimestamp.isEmpty()) {
    int firstAvailableIndex = indexToTimestamp.firstKey();
    if (newMin < firstAvailableIndex) {
        // Request missing bars
    }
}
```

This properly detects when panning beyond the first available bar, regardless of whether it has a positive or negative index.

### 6. Fixed: `maintainBarLimit()`

Now cleans up index mappings when removing old bars:
```cpp
// Clean up the index mapping for the removed bar
if (oldestTime.isValid() && timestampToIndex.contains(oldestTime)) {
    int removedIndex = timestampToIndex[oldestTime];
    timestampToIndex.remove(oldestTime);
    indexToTimestamp.remove(removedIndex);
}
```

## Performance Improvements

| Operation | Before | After |
|-----------|--------|-------|
| Insert m historical bars (with n existing bars) | O(n log n) | O(m) |
| Full chart rebuild | Always on historical data | Only on initialization |
| Index computation | Recompute all n indices | Only compute m new indices |

## Files Modified

1. `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h`
   - Added declaration for `addHistoricalBarsToIndexMapping()`

2. `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp`
   - Implemented `addHistoricalBarsToIndexMapping()`
   - Modified `onRequestedMissingBarsReceived()`
   - Updated `rebuildIndexMapping()`
   - Updated `getIndexForTimestamp()`
   - Updated `maintainBarLimit()`
   - Removed index constraints in multiple methods

3. `Src/FrontEnd/GUI/StockPriceChart/Panning.cpp`
   - Updated `handleMouseButtonPress()` (right-click reset)
   - Updated `handlePanning()`

4. `Src/FrontEnd/GUI/StockPriceChart/WheelEvent.cpp`
   - Updated `handleHorizontalPanning()`
   - Updated `handleHorizontalZoom()`
   - Updated `handleBothAxesZoom()`

5. `Doc/StockPriceChart_Architecture.md`
   - Updated with bidirectional index system documentation

## Testing Recommendations

### Manual Testing

1. **Initial Load**
   - Start the application and load a symbol
   - Verify the first bar received becomes index 0
   - Check that subsequent live bars get positive indices (1, 2, 3...)

2. **Historical Bar Loading**
   - Pan left to view earlier time periods
   - Verify that missing bars are requested
   - Confirm that historical bars receive negative indices (-1, -2, -3...)
   - Ensure no visual glitches or jumps in the chart

3. **Mixed Navigation**
   - Pan left to load historical bars (negative indices)
   - Pan right back to live data (positive indices)
   - Zoom in and out at various positions
   - Verify smooth transitions and correct bar display

4. **Right-Click Reset**
   - Load historical bars (creating negative indices)
   - Right-click to reset view to last 30 bars
   - Verify correct centering even with negative indices present

5. **Bar Limit**
   - Load enough bars to exceed MAX_BARS (1000)
   - Verify oldest bars are removed
   - Check that index mappings are cleaned up properly

### Performance Testing

1. **Baseline Measurement**
   - Measure time to load 100 historical bars with 500 existing bars
   - Compare against expected O(m) complexity

2. **Scalability Test**
   - Test with varying numbers of existing bars (100, 500, 1000)
   - Verify that historical bar insertion time remains constant (O(m))

3. **Memory Usage**
   - Monitor memory usage during extensive panning
   - Verify that index mappings don't accumulate orphaned entries

## Potential Future Enhancements

1. **Index Compaction**
   - If the index range becomes very large (e.g., -10000 to +10000), consider periodic compaction to normalize around 0

2. **Lazy Loading Optimization**
   - Could further optimize by loading larger chunks of historical data in a single request

3. **Index Persistence**
   - Consider caching index mappings to disk for faster symbol switching

## Known Limitations

1. **Index Range Growth**
   - Indices can grow indefinitely in both directions
   - QMap can handle large index ranges, but extreme cases (>100k bars) haven't been tested

2. **Bar Limit Interaction**
   - If MAX_BARS is set very low relative to the panning range, users might see gaps when panning far back then returning to live data

## Migration Notes

This change is **backward compatible** as it doesn't affect:
- Bar data structures
- Cache format
- API interfaces
- User-visible behavior (except improved performance)

The only behavioral change is the internal index assignment, which is transparent to users.
