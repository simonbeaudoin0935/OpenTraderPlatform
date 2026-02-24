# Replay Mode: Dynamic Day/Time Selection - Implementation Plan & Tracking

**Status**: ✅ CORE FEATURE COMPLETE (Phase 5.1 partial)  
**Date Created**: 2026-02-11  
**Last Updated**: 2026-02-11  

---

## Executive Summary

This document tracks the implementation of dynamic day/time selection in replay mode. Users can now change the replay day or start time during paused replay state, and the chart automatically reloads with the selected day/time data.

**What's Done**: ✅ Phases 1-4 + Phase 5.1 (core feature and basic error handling)  
**What's Not Done**: Optional edge case enhancements (Phases 5.2-5.10)

---

## ✅ COMPLETED IMPLEMENTATION

### Phase 1: State Management & Signal Flow
**Status**: ✅ COMPLETE (Commit: a97d2d9)

- [x] Add `ReplayState` enum to ChartToolbar (Inactive, PreloadingPaused, Playing, Paused)
- [x] Add `m_replayState` member variable and state getter/setter
- [x] Implement `updateUIControlStates()` with enable/disable matrix for controls
- [x] Add `m_isPausedBeforePlay` flag to ReplayEngine
- [x] Update ReplayEngine state transitions in `startReplay()`, `startReplayPaused()`, `stopReplay()`

**Files Modified**:
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.h/cpp`
- `Src/Core/Replay/ReplayEngine.h/cpp`

**Key Design Decision**: Use state enum to track UI state, separate from ReplayEngine's PlaybackState

---

### Phase 2: Day Change Handling
**Status**: ✅ COMPLETE (Commit: 8f04bd7)

- [x] Update `ChartToolbar::onReplayDayChanged()` with guard for Playing state
- [x] Add `MainApp::preloadChartForReplay()` method
- [x] Enhance `StockPriceChart::onReplayDayChanged()` to trigger preload
- [x] Maintain existing time range query for toolbar info

**Flow**:
1. User selects different day → ChartToolbar signal (if not Playing)
2. StockPriceChart receives signal, checks `MainApp::isInReplayMode()`
3. Calls `MainApp::preloadChartForReplay(date, currentTime, speed)`
4. Chart reloads with selected day's data

**Files Modified**:
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.cpp`
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp`
- `Src/Core/MainApp.h/cpp`

---

### Phase 3: Time Change Handling
**Status**: ✅ COMPLETE (Commit: 64f4a4e)

- [x] Update `ChartToolbar::onReplayTimeChanged()` with guard for Playing state
- [x] Add `StockPriceChart::onReplayTimeChanged()` slot
- [x] Connect time change signal in StockPriceChart constructor
- [x] Trigger preload when time changes during paused state

**Flow**: Similar to day changes, but keeps current day and updates start time

**Files Modified**:
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.cpp`
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h/cpp`

---

### Phase 4: State Transitions
**Status**: ✅ COMPLETE (Commit: 72d2df6)

- [x] Update `ChartToolbar::onPlayPauseClicked()` to manage state transitions
- [x] Transition PreloadingPaused → Playing when Play pressed
- [x] Transition Playing → Paused when Pause pressed
- [x] Set state to PreloadingPaused in `GUIFrontend::onReplayModeEntered()`
- [x] Set state to Inactive in `GUIFrontend::onReplayModeExited()`
- [x] Initialize state to Inactive in ChartToolbar constructor

**State Transition Matrix**:
```
Inactive
    ↓ (enterReplayMode)
PreloadingPaused (day/time enabled)
    ↓ (play pressed)
Playing (day/time disabled)
    ↓ (pause pressed)
Paused (day/time re-enabled)
    ↓ (play pressed)
Playing
    ↓ (exitReplayMode)
Inactive
```

**Files Modified**:
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.cpp`
- `Src/FrontEnd/GUI/GUIFrontend.cpp`

---

### Phase 5.1: Database File Error Handling
**Status**: ✅ COMPLETE (Commit: 9142efd)

- [x] Add `replayDataLoadFailed` signal to ReplayEngine
- [x] Emit signal with error message when `initLoaders()` fails
- [x] Add `getReplayEngine()` getter to MainAlgo and MainApp
- [x] Add `onReplayDataLoadFailed()` public slot to StockPriceChart
- [x] Connect ReplayEngine error signal in `GUIFrontend::onReplayModeEntered()`
- [x] Display error in toolbar info label when preload fails

**Error Handling**:
1. ReplayEngine detects DB file not found/corrupted
2. Emits `replayDataLoadFailed(errorMessage)`
3. StockPriceChart clears toolbar info to show failure
4. User can retry with different day or debug

**Files Modified**:
- `Src/Core/Replay/ReplayEngine.h/cpp`
- `Src/Algo/MainAlgo.h/cpp`
- `Src/Core/MainApp.h/cpp`
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h/cpp`
- `Src/FrontEnd/GUI/GUIFrontend.cpp`

---

## ⏸️ OPTIONAL ENHANCEMENTS (Not Implemented)

These edge cases are tracked but deemed lower priority. Can be tackled if needed.

### HIGH PRIORITY (Recommended for future)

**Phase 5.2: Cancel Pending Preloads on Replay Exit**
- **Issue**: If user exits replay mode while preload async operation pending, callback fires after exit
- **Fix**: Track async operations, cancel on mode exit
- **Complexity**: Medium
- **Files Affected**: MainApp, ReplayEngine, ReplayDataLoader
- **Implementation**: Use QFutureWatcher or explicit cancellation flag

**Phase 5.3: Time Validation - Within Trading Range**
- **Issue**: User selects time before trading opens (e.g., 3:00 AM) or after close
- **Fix**: Validate time is within data available range before preload
- **Complexity**: Low
- **Files Affected**: StockPriceChart, ChartToolbar
- **Implementation**: Check against toolbar's time range info, add validation before preload
- **Note**: Toolbar already queries time range via `queryStockTimeRangeForDate()`

**Phase 5.4: Symbol Validation - Data Exists on Day**
- **Issue**: User switches symbol, then changes day - might not have data for that symbol on that day
- **Fix**: Check symbol has data on selected day before triggering preload
- **Complexity**: Medium
- **Files Affected**: StockPriceChart, ReplayDataLoader
- **Implementation**: Add query to check bar count for symbol on day before preload

---

### MEDIUM PRIORITY (Nice to have)

**Phase 5.5: Rapid Day/Time Change Race Condition**
- **Issue**: User rapidly clicks different days → multiple preloads fire, later one might complete first
- **Fix**: Cancel previous preload when new one selected, or use timestamps to ignore stale results
- **Complexity**: Medium
- **Implementation**: Either cancel async operations or tag results with timestamp

**Phase 5.6: Time Validation - Outside Trading Hours**
- **Issue**: Selected time is outside normal trading hours (pre-market, after-hours)
- **Fix**: Warn user or auto-adjust to nearest trading time
- **Complexity**: Low
- **Files Affected**: ChartToolbar, StockPriceChart
- **Implementation**: Check against `TradingHours::` constants, show warning or auto-adjust

**Phase 5.7: UI State Synchronization**
- **Issue**: Play button state vs actual replay state might get out of sync
- **Fix**: Bind button checked state to actual ReplayEngine state, not local checkbox
- **Complexity**: Low
- **Files Affected**: ChartToolbar, GUIFrontend
- **Implementation**: Query ReplayEngine state instead of trusting button checkbox

---

### LOWER PRIORITY (Polish)

**Phase 5.8: Auto-Adjust Invalid Times**
- **Issue**: User picks invalid time, chart doesn't load
- **Fix**: Automatically jump to nearest valid trading time
- **Complexity**: Low
- **Files Affected**: StockPriceChart, ReplayDataLoader
- **Implementation**: On error, query first available time and preload with that

**Phase 5.9: Better Error Messages**
- **Issue**: Generic error messages don't help user understand what went wrong
- **Fix**: Distinguish between: file not found, corrupted, no data for symbol, etc.
- **Complexity**: Low
- **Files Affected**: ReplayEngine, StockPriceChart
- **Implementation**: Add detailed error codes/messages in error signal

**Phase 5.10: Logging Improvements**
- **Issue**: Debugging edge cases requires enabling all logging, noisy output
- **Fix**: Add more specific debug logging for replay preload flow
- **Complexity**: Low
- **Files Affected**: ReplayEngine, ReplayDataLoader, MainApp, StockPriceChart
- **Implementation**: Add debug log statements at key points in preload flow

---

## Control Enable/Disable Matrix

This is the key feature that prevents user confusion:

| State | Day Combo | Time Edit | Speed Combo | Play/Pause Button |
|-------|-----------|-----------|-------------|-------------------|
| **Inactive** | ❌ Disabled | ❌ Disabled | ❌ Disabled | ❌ Disabled |
| **PreloadingPaused** | ✅ Enabled | ✅ Enabled | ✅ Enabled | ✅ Enabled |
| **Playing** | ❌ Disabled | ❌ Disabled | ✅ Enabled | ✅ Enabled |
| **Paused** | ✅ Enabled | ✅ Enabled | ✅ Enabled | ✅ Enabled |

**Rationale**:
- Day/Time disabled during Playing to prevent confusing state where user selects new values but they don't take effect
- Speed always enabled because it applies immediately during playback
- Day/Time re-enabled when Paused to allow new preloads

---

## Signal Flow Diagram

```
User selects day while Paused
    │
    ▼
ChartToolbar::onReplayDayChanged()
    │
    ├─ Guard: if Playing state, return (ignore)
    │
    └─ Emit replayDayChanged(date)
            │
            ▼
    StockPriceChart::onReplayDayChanged()
            │
            ├─ Check: MainApp::isInReplayMode()?
            │
            └─ Yes: MainApp::preloadChartForReplay(date, time, speed)
                    │
                    ▼
            MainApp::preloadChartForReplay()
                    │
                    ├─ Get displayed symbol
                    │
                    └─ MainAlgo::enterReplayModePaused(date, time, speed)
                            │
                            ▼
                    ReplayEngine::startReplayPaused()
                            │
                            ├─ Call initLoaders(date, time)
                            │
                            ├─ If error:
                            │   emit replayDataLoadFailed(message)
                            │       │
                            │       ▼
                            │   StockPriceChart::onReplayDataLoadFailed()
                            │       └─ Clear toolbar info, log error
                            │
                            └─ If success:
                                emit first bar, pause
                                │
                                ▼
                            Chart updates with new data
```

---

## Testing Checklist

**Manual Testing** (already works):
- [x] Select day → verify chart preloads
- [x] Select new day → verify chart reloads
- [x] Select new time → verify chart reloads
- [x] Press play → verify day combo disables
- [x] Change day during playback → verify no effect
- [x] Press pause → verify day combo re-enables
- [x] Select new day while paused → verify chart updates

**Optional Testing** (for future edge cases):
- [ ] Delete .db file during preload → verify error handling
- [ ] Corrupt .db file → verify error message
- [ ] Select time before first bar → verify handling
- [ ] Rapidly click different days → verify no crash or wrong data
- [ ] Change symbol during paused replay → verify day change still works

---

## Files Modified Summary

**Phases 1-4 Complete**:
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.h` (100 lines added)
- `Src/FrontEnd/GUI/StockPriceChart/ChartToolbar.cpp` (125 lines added/modified)
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h` (8 lines added)
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.cpp` (55 lines added)
- `Src/Core/MainApp.h/cpp` (35 lines added)
- `Src/Core/Replay/ReplayEngine.h/cpp` (25 lines added/modified)
- `Src/FrontEnd/GUI/GUIFrontend.cpp` (30 lines added/modified)

**Phase 5.1 Complete**:
- `Src/Core/Replay/ReplayEngine.h/cpp` (10 lines signal/error handling)
- `Src/Algo/MainAlgo.h/cpp` (8 lines added getter)
- `Src/Core/MainApp.h/cpp` (10 lines added getter)
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h/cpp` (20 lines added)
- `Src/FrontEnd/GUI/GUIFrontend.cpp` (8 lines connection)

**Total Lines Added**: ~400 lines across 10 files

---

## Commits Summary

1. **a97d2d9**: Phase 1 - Add replay state management and UI control state tracking
2. **8f04bd7**: Phase 2 - Implement day change handling with dynamic chart preload
3. **9d521e8**: Phase 3a - Add time change handling (header and toolbar updates)
4. **64f4a4e**: Phase 3 - Complete time change handling with dynamic chart preload
5. **72d2df6**: Phase 4 - Implement state transitions for play/pause controls
6. **9142efd**: Phase 5.1 - Handle database file not found/corrupted errors

---

## Key Design Decisions

1. **State Machine Approach**: Use explicit ReplayState enum instead of implicit state tracking
   - **Rationale**: Clear visibility into UI state, easier to debug, explicit transitions

2. **Silent Preload**: No loading indicator, happens immediately behind scenes
   - **Rationale**: Database operations are fast, user feedback via toolbar info update

3. **Guard at Signal Emit**: Check state in ChartToolbar before emitting, not after
   - **Rationale**: Prevents unnecessary signal processing downstream

4. **Separate ChartToolbar State**: ChartToolbar has its own ReplayState, separate from ReplayEngine's PlaybackState
   - **Rationale**: UI controls state is different from engine state, cleaner separation of concerns

5. **Error Signal from Engine**: ReplayEngine emits replayDataLoadFailed to UI
   - **Rationale**: Proper separation of concerns, engine reports errors, UI displays them

---

## Future Maintenance Notes

1. **State Transitions**: If adding new replay states, update:
   - `ReplayState` enum
   - `updateUIControlStates()` switch statement
   - `GUIFrontend` enter/exit methods
   - Guard logic in day/time change slots

2. **New Replay Control**: If adding new control (e.g., filter selector):
   - Add to ChartToolbar UI
   - Add getter/setter methods
   - Add state-aware enable/disable in `updateUIControlStates()`
   - Add signal in header
   - Connect in StockPriceChart
   - Implement handler slot

3. **Error Handling**: If adding new error type:
   - Add error code/message in ReplayEngine
   - Update replayDataLoadFailed signal
   - Handle in StockPriceChart::onReplayDataLoadFailed()
   - Update user-facing error messages

---

## References

- **Implementation**: See `Src/FrontEnd/GUI/StockPriceChart/AGENTS.md` for detailed technical documentation
- **Replay Architecture**: See `Src/Core/Replay/AGENTS.md` for replay system details
- **Custom Instructions**: See `.L2Trader/.copilot/instructions.md` for coding guidelines

---

## Status Update Log

| Date | Update | Status |
|------|--------|--------|
| 2026-02-11 | Completed Phases 1-4 and Phase 5.1 | Core feature working, error handling in place |
| 2026-02-11 | Documented edge cases for future | Phases 5.2-5.10 ready to implement if needed |
| 2026-02-11 | Created AGENTS.md | Technical documentation complete |

---

**Last Updated**: 2026-02-11  
**By**: AI Assistant  
**Status**: ✅ PRODUCTION READY (Core Feature Complete)
