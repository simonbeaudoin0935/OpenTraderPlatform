# Background Rendering Debouncing Optimization

## Problem
When panning or zooming the chart, the `updateAfterHoursBackground()` method was called on **every single axis range change event**, causing severe sluggishness even when viewing already-loaded data.

## Root Cause
The background rendering system was directly connected to axis `rangeChanged` signals:

```cpp
// Old code - called on EVERY range change
connect(axisX, &QValueAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::updateAfterHoursBackground);
```

During a pan operation, the axis range changes **dozens or hundreds of times**, and each change triggered:
1. Clearing all background rectangles - O(r) where r = number of rectangles
2. Iterating through each day in visible range
3. For each day, iterating through 24 hours
4. For each hour, checking market status and creating rectangles
5. Total: **O(h × d)** where h = hours, d = days in visible range

**Example**: Panning over a 5-day view with 100 range change events = 100 × 5 days × 24 hours = **12,000 iterations** just for panning!

## Solution: Debouncing with QTimer

Implemented a debouncing mechanism using `QTimer::singleShot()`:

### Changes Made

1. **Added debounce timers** (header):
```cpp
QTimer* backgroundUpdateTimer;   // Debounce background updates
QTimer* priceLineUpdateTimer;    // Debounce price line updates
```

2. **Created debounced handler** (implementation):
```cpp
void StockPriceChart::onAxisRangeChanged() {
    // Restart timers - they only fire when changes stop
    backgroundUpdateTimer->start();
    priceLineUpdateTimer->start();
}
```

3. **Configured timers in constructor**:
```cpp
backgroundUpdateTimer = new QTimer(this);
backgroundUpdateTimer->setSingleShot(true);
backgroundUpdateTimer->setInterval(100); // 100ms debounce

priceLineUpdateTimer = new QTimer(this);
priceLineUpdateTimer->setSingleShot(true);
priceLineUpdateTimer->setInterval(50);  // 50ms debounce
```

4. **Rewired connections**:
```cpp
// New code - debounced
connect(axisX, &QValueAxis::rangeChanged, this, &StockPriceChart::onAxisRangeChanged);
connect(axisY, &QValueAxis::rangeChanged, this, &StockPriceChart::onAxisRangeChanged);

// Timer connections
connect(backgroundUpdateTimer, &QTimer::timeout, this, &StockPriceChart::updateAfterHoursBackground);
connect(priceLineUpdateTimer, &QTimer::timeout, this, &StockPriceChart::updateLastPriceLineIfNeeded);
```

### How Debouncing Works

```
User pans continuously:
rangeChanged → start timer (100ms)
rangeChanged → restart timer (100ms)  ← Timer resets, doesn't fire
rangeChanged → restart timer (100ms)  ← Timer resets, doesn't fire
rangeChanged → restart timer (100ms)  ← Timer resets, doesn't fire
... [user stops panning] ...
[wait 100ms]
Timer fires → updateAfterHoursBackground() executes ONCE
```

Instead of executing on every change, the expensive operations only execute **once** after the user stops interacting.

## Performance Impact

### Before
- Pan over 5 days with 100 range changes
- Executions: 100 (one per change)
- Total iterations: 100 × 5 × 24 = 12,000
- User experience: Laggy, stuttering

### After
- Pan over 5 days with 100 range changes
- Executions: 1 (only after stopping)
- Total iterations: 1 × 5 × 24 = 120
- User experience: Smooth, responsive

**Speedup: 100x reduction in background rendering during panning**

## Debounce Intervals

- **Background updates: 100ms** - Acceptable delay for visual elements that don't affect immediate interaction
- **Price line updates: 50ms** - Shorter delay for more responsive visual feedback

These intervals provide a good balance between:
- Responsiveness (user doesn't notice the delay)
- Performance (operations batched efficiently)

## Edge Cases Handled

1. **Immediate updates still work**: When loading historical bars (`onRequestedMissingBarsReceived()`), the method is called directly, bypassing debouncing
2. **Multiple axes**: Both X and Y axis changes trigger the same debounced handler
3. **Single-shot timers**: Using `setSingleShot(true)` ensures timers only fire once per restart
4. **Timer ownership**: Timers are owned by the widget (passed `this` as parent) for automatic cleanup

## Trade-offs

**Pros:**
- Massive performance improvement during pan/zoom
- Smooth user experience
- Simple, maintainable solution
- No loss of functionality

**Cons:**
- Slight delay (50-100ms) before visual updates appear after user stops interacting
- This delay is imperceptible to users and is standard UX practice

## Testing Recommendations

1. **Rapid panning**: Pan quickly across large time ranges - should be smooth
2. **Rapid zooming**: Zoom in/out rapidly - should be smooth
3. **Visual accuracy**: After panning stops, backgrounds should render correctly
4. **Historical loads**: Bulk bar loads should still trigger immediate updates

## Comparison to Other Approaches

### Alternative 1: Caching backgrounds
- Would still require O(h×d) initially
- Complex to implement correctly
- Invalidation logic adds complexity

### Alternative 2: Simplify background rendering
- Would reduce h×d iteration count
- Still executes on every change
- Less effective for continuous operations

### Debouncing (chosen approach)
- ✅ Simple to implement
- ✅ Eliminates redundant work
- ✅ Standard UX pattern
- ✅ Minimal code changes
- ✅ No functional impact
