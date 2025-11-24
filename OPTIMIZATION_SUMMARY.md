# StockPriceChart Performance Optimization Summary

## Problem
The StockPriceChart became sluggish with large numbers of bars loaded due to O(n) operations on every bar update.

## Root Cause
The original implementation used a "rebuild everything" approach:
- Every bar addition (open or closed) called `updateChart()`
- `updateChart()` performed a complete rebuild:
  1. Rebuilt index mapping from scratch - O(n log n)
  2. Cleared entire candlestick series - O(n)
  3. Recreated all candlestick sets - O(n)

This meant **every single bar update** (which happens frequently for real-time data) triggered O(n) operations on all bars.

## Solution: Incremental Updates

### Key Changes

#### 1. Incremental Index Mapping (`updateIndexMappingIncremental`)
- Instead of rebuilding the entire index↔timestamp mapping on every bar
- Add only the new timestamp to the mapping - O(log n)
- Falls back to full rebuild only when bars arrive out of order (rare for historical data)

#### 2. Incremental Candlestick Updates
- **For open bar updates**: Update existing candlestick set in place - O(1)
  - `updateOpenBarCandlestick()` finds and modifies the existing set
  - No series rebuild needed
  
- **For new bars**: Append single candlestick to series - O(1)
  - `addNewCandlestick()` creates and appends one set
  - No series rebuild needed

- **For bar transitions**: Update existing or append new - O(1)
  - When open bar closes, update its candlestick in place
  - When new bar starts, append it

#### 3. Removed `updateChart()` from Bar Addition Path
- `addBar()` no longer calls `updateChart()`
- Each handler (`handleOpenBar`, `handleClosedBar`) does incremental updates
- `updateChart()` still exists and is used when loading historical bars in bulk

### Performance Improvement

**Before:**
- Add bar: O(n log n) + O(n) + O(n) = **O(n log n)** per bar
- With 1000 bars loaded, each new bar update touches ~10,000 operations

**After:**
- Add bar (update existing): O(log n) + O(1) = **O(log n)** per bar  
- Add bar (new): O(log n) + O(1) = **O(log n)** per bar
- With 1000 bars loaded, each new bar update touches ~10 operations

**Speedup: ~1000x improvement for bar updates with 1000 bars loaded**

### What Still Uses Full Rebuild

`updateChart()` is still called (and needed) when:
1. Loading historical bars via `onRequestedMissingBarsReceived()`
2. Symbol changes via `clearSymbol()`
3. Any operation that loads many bars at once

This is acceptable because these operations happen less frequently than real-time bar updates.

## Code Structure

### New Helper Methods

```cpp
void updateIndexMappingIncremental(const QDateTime& timestamp);
void updateOpenBarCandlestick();
void addNewCandlestick(const QDateTime& timestamp, const Bar& bar);
QCandlestickSet* findCandlestickSetByTimestamp(const QDateTime& timestamp) const;
```

### Modified Flow

**Before:**
```
addBar() → handleOpenBar/handleClosedBar() → updateChart() → [full rebuild]
```

**After:**
```
addBar() → handleOpenBar() → updateIndexMappingIncremental() + updateOpenBarCandlestick()
        → handleClosedBar() → updateIndexMappingIncremental() + addNewCandlestick()
```

## Testing Recommendations

1. **Real-time updates**: Verify that open bar updates are smooth with 1000 bars loaded
2. **Bar transitions**: Test that open→closed transitions work correctly
3. **New bars**: Verify new bars appear correctly in sequence
4. **Historical loads**: Ensure bulk historical bar loads still work
5. **Edge cases**: Test first bar, empty chart, symbol changes

## Future Optimizations

This addresses Issue #2 (highest priority). Remaining issues to address:

- **Issue #3**: Background re-rendering on all axis changes (debouncing/throttling)
- **Issue #4**: Inefficient visible bar price range calculation (cache visible range)
- **Issue #1**: Index mapping rebuild (further optimize or eliminate for incremental case)
