# Visual Guide: Before and After Fix

## Problem Visualization

### Before Fix

```
User Action: Select Stock "AAPL" (trading at ~$180)
    ↓
clearSymbol() called
    ↓
Chart axes reset:
    X-axis: [Current Time - 30min] to [Current Time + 5min]  ✓ CORRECT
    Y-axis: [0] to [100]                                      ✗ WRONG
    ↓
First bars arrive (price ~$180)
    ↓
updateChart() called
    ↓
    hadInitialView = true (because axes are set)
    ↓
    Y-axis RESTORED to [0] to [100]                          ✗ STAYS WRONG
    ↓
Chart Display:
┌────────────────────────────────────────┐
│ Stock Price: AAPL           Price: 100 │  ← Stock invisible
│                                      90 │     up here
│                                      80 │
│                                      70 │
│                                      60 │
│                                      50 │
│                                      40 │
│                                      30 │
│                                      20 │
│                                      10 │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 0 │
│         Time axis (correct) →          │
└────────────────────────────────────────┘
User must RIGHT-CLICK to refocus!
```

### After Fix

```
User Action: Select Stock "AAPL" (trading at ~$180)
    ↓
clearSymbol() called
    ↓
Chart axes reset:
    X-axis: [Current Time - 30min] to [Current Time + 5min]  ✓ CORRECT
    Y-axis: [0] to [100]                                      
    needsInitialPriceFocus = true                            ✓ FLAG SET
    ↓
First bars arrive (price ~$180)
    ↓
updateChart() called
    ↓
    Check needsInitialPriceFocus flag
    ↓
    Flag is TRUE → Skip Y-axis restoration                   ✓ SMART!
    ↓
    Calculate Y-axis from actual bars:
        minPrice = $179.50
        maxPrice = $180.50
        Y-axis: [179.46] to [180.54] (with padding)          ✓ CORRECT!
    ↓
    Clear needsInitialPriceFocus flag                        ✓ ONE-TIME
    ↓
Chart Display:
┌────────────────────────────────────────┐
│ Stock Price: AAPL       Price: 180.54 │
│                              ▃▅▂▄      │  ← Stock visible
│                            ▅█████▃    │     right here!
│                          ▂███████▂   │
│                         ▃█████████   │
│                       ▅███████████▃  │
│                      ▂█████████████▂ │
│ ━━━━━━━━━━━━━━━━━━━ ━ ━ ━ ━ ━ 179.46 │
│         Time axis (correct) →          │
└────────────────────────────────────────┘
Perfect! No user action needed!
```

## Code Flow Comparison

### Before Fix Flow
```
clearSymbol()
    │
    └─> Set Y-axis: [0, 100]
        │
        ▼
addBar() → updateChart()
        │
        ├─> hadInitialView = true
        │   (because axes have range)
        │
        └─> RESTORE Y-axis to [0, 100]  ← Problem!
            │
            ▼
        Wrong display
```

### After Fix Flow
```
clearSymbol()
    │
    ├─> Set Y-axis: [0, 100]
    │
    └─> needsInitialPriceFocus = true  ← New flag!
        │
        ▼
addBar() → updateChart()
        │
        ├─> Check needsInitialPriceFocus
        │   │
        │   └─> TRUE? → SKIP Y-axis restoration  ← Smart!
        │
        ├─> Calculate Y-axis from bars
        │   (minPrice, maxPrice from actual data)
        │
        └─> needsInitialPriceFocus = false  ← Reset
            │
            ▼
        Correct display!
            │
            ▼
More bars arrive...
        │
        └─> needsInitialPriceFocus = false
            │
            └─> Normal behavior
                (preserve user's view)
```

## State Machine Diagram

```
                    ┌─────────────────────────┐
                    │   No Stock Selected     │
                    │  needsInitialPriceFocus │
                    │        = false          │
                    └───────────┬─────────────┘
                                │
                    User selects stock
                                │
                                ▼
                    ┌─────────────────────────┐
                    │  clearSymbol() called   │
                    │  Chart cleared          │
                    │  needsInitialPriceFocus │
                    │        = true           │◄────────┐
                    └───────────┬─────────────┘         │
                                │                       │
                    First bars arrive                   │
                                │                       │
                                ▼                       │
                    ┌─────────────────────────┐        │
                    │  updateChart() called   │        │
                    │  Flag checked: TRUE     │        │
                    │  Y-axis CALCULATED      │        │
                    │  needsInitialPriceFocus │        │
                    │        = false          │        │
                    └───────────┬─────────────┘        │
                                │                       │
                    Chart focused correctly!            │
                                │                       │
                                ▼                       │
                    ┌─────────────────────────┐        │
                    │ Stock Being Displayed   │        │
                    │  needsInitialPriceFocus │        │
                    │        = false          │        │
                    │                         │        │
                    │ • More bars arrive:     │        │
                    │   Normal updates        │        │
                    │ • User pan/zoom:        │        │
                    │   View preserved        │        │
                    └───────────┬─────────────┘        │
                                │                       │
                    User selects different stock        │
                                │                       │
                                └───────────────────────┘
```

## Key Points

### The Flag Lifecycle

1. **Created**: When class is instantiated
   - Initial value: `false`

2. **Set to `true`**: When `clearSymbol()` is called
   - Happens: User selects new stock
   - Purpose: Signal that next update needs auto-focus

3. **Checked**: In `updateChart()`
   - If `true`: Skip Y-axis restoration, force calculation
   - If `false`: Normal behavior (preserve user's view)

4. **Set to `false`**: After first successful focus
   - Happens: In `updateChart()` after Y-axis is set
   - Purpose: Prevent unwanted refocusing on subsequent updates

### Why This Works

✅ **Single responsibility**: One flag, one purpose
✅ **One-time trigger**: Set once, cleared after use
✅ **Thread-safe**: Only accessed in main GUI thread
✅ **No side effects**: Doesn't affect other chart operations
✅ **User-friendly**: Auto-focus on selection, preserve manual operations

### Edge Cases Handled

1. **Rapid stock switching**: Each clearSymbol() resets flag
2. **No bars available**: Condition checks for valid data
3. **User zooms before bars arrive**: Flag still triggers focus
4. **Empty bar list**: Loop handles gracefully
5. **Subsequent updates**: Flag is false, normal behavior resumes

---

## Summary

**What was broken**: Y-axis stuck at 0-100 after stock selection

**What we fixed**: Added smart flag to auto-focus on first bars

**How it works**: Flag triggers one-time Y-axis calculation, then clears

**Result**: Chart displays correctly immediately, no user action needed!
