# Testing Guide for Stock Price Chart Focus Fix

## Issue Description
When a stock is selected (either by entering the ticker in the input box or clicking on a ticker's name in the position widget), the stock price chart would:
- Correctly display the time axis (defaulting to current time)
- Incorrectly display the price range as 0-100 (default range)
- Require a manual right-click to refocus on the actual price

## Fix Implementation

### Changes Made

1. **Added `needsInitialPriceFocus` flag** in `StockPriceChart.h`:
   - Boolean flag to track when initial price focus is needed
   - Initialized to `false`

2. **Modified `clearSymbol()`** in `StockPriceChart.cpp`:
   - Sets `needsInitialPriceFocus = true` when clearing for a new stock selection
   - This signals that the next chart update should auto-focus on the price

3. **Modified `updateChart()`** in `StockPriceChart.cpp`:
   - When restoring view state, checks `needsInitialPriceFocus` flag
   - If flag is set, skips restoring the Y-axis (price) range
   - Adds condition to force Y-axis calculation when flag is set
   - Clears the flag after initial focus is completed

### How It Works

1. User selects a stock (via input box or position widget)
2. `clearSymbol()` is called, which sets `needsInitialPriceFocus = true`
3. First bar(s) arrive via `addBar()` → `updateChart()` is called
4. `updateChart()` detects `needsInitialPriceFocus` is true
5. Instead of restoring the 0-100 range, it calculates actual price range from bars
6. Chart displays with correct price focus
7. Flag is reset to `false`, preventing unwanted refocusing on subsequent updates

## Testing Instructions

### Prerequisites
- Build the application with Qt6 and GUI enabled
- Have access to market data (TradeStation API configured)
- Have some stocks available to select

### Test Case 1: Input Box Selection

1. Start the application
2. Wait for it to fully load
3. Press 'i' to focus the stock symbol input box
4. Type a valid stock ticker (e.g., "AAPL")
5. Press Enter

**Expected Result:**
- Chart should immediately display with correct price range
- No need to right-click to see the stock price properly
- Time axis should show last 30 minutes
- Price axis should be centered on actual stock price

**Before Fix:**
- Chart would show 0-100 price range
- Stock bars would not be visible or barely visible
- Required right-click to fix

### Test Case 2: Position Widget Selection

1. Start the application
2. Wait for positions to load in the position widget
3. Click on a ticker name in the position widget

**Expected Result:**
- Same as Test Case 1
- Chart should auto-focus on the correct price range

### Test Case 3: Subsequent Updates Don't Refocus

1. Select a stock using either method above
2. Wait for chart to display with correct focus
3. Manually pan or zoom the chart to view historical data
4. Wait for new bars to arrive (real-time updates)

**Expected Result:**
- Chart should NOT refocus to current time/price
- Your manual pan/zoom position should be preserved
- This confirms the flag only triggers once per stock selection

### Test Case 4: Switching Between Stocks

1. Select Stock A
2. Wait for it to display correctly
3. Select Stock B
4. Wait for it to display correctly
5. Select Stock A again

**Expected Result:**
- Each stock selection should auto-focus correctly
- No residual behavior from previous selections

## Verification Points

- [ ] Chart auto-focuses on price when first selecting a stock
- [ ] No right-click needed to see stock price properly
- [ ] Subsequent bar updates don't cause unwanted refocusing
- [ ] User pan/zoom operations are not interfered with
- [ ] Switching between stocks works correctly
- [ ] No crashes or errors in the log

## Code Review Points

1. **Flag Management**: The flag is set in exactly one place (`clearSymbol()`) and cleared in exactly one place (after initial focus in `updateChart()`)

2. **Race Conditions**: The flag is only accessed in the main GUI thread, no threading issues

3. **Edge Cases Handled**:
   - Empty bars check: `minPrice != std::numeric_limits<double>::max()`
   - No bars available: Logic only executes when bars are present
   - Multiple rapid selections: Each `clearSymbol()` resets the flag

4. **Minimal Changes**: 
   - Only 3 lines added to header
   - Only ~15 lines changed in implementation
   - No changes to existing behavior except the specific bug fix

## Rollback Plan

If issues are discovered:
1. Remove the `needsInitialPriceFocus` flag from header
2. Revert the three changes in `StockPriceChart.cpp`
3. Chart will return to previous behavior (requiring right-click to focus)

## Related Files

- `src/GUI/StockPriceChart.h` - Header file with flag declaration
- `src/GUI/StockPriceChart.cpp` - Implementation with fix
- `src/GUI/GuiFrontend.cpp` - Where stock selection triggers clearSymbol()
- `Doc/StockPriceChart_Architecture.md` - Architecture documentation
