# TUI (Text User Interface) Implementation

## Overview

The L2Trader application supports two frontend modes:
- **GUI** (Graphical User Interface) - Qt-based desktop application with full features
- **TUI** (Text User Interface) - ncurses-based terminal application with minimal features

This document describes the TUI implementation, which provides a lightweight alternative to the GUI for headless servers or users who prefer terminal interfaces.

## Building

### TUI Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=OFF -DBUILD_TESTS=OFF
cmake --build build -j2
```

### GUI Build (default)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=ON -DBUILD_TESTS=OFF
cmake --build build -j2
```

## Architecture

### Class Structure

The TUI follows the same frontend architecture as the GUI:

- **FrontEnd** (abstract base class): Defines the interface for both GUI and TUI
- **TUIFrontend**: Implements the ncurses-based terminal interface
- **GUIFrontend**: Implements the Qt-based graphical interface

```
FrontEnd (abstract)
    ├── TUIFrontend (ncurses)
    └── GUIFrontend (Qt Widgets)
```

### Window Layout

The TUI uses a multi-window ncurses layout divided into 4 sections:

```
┌─────────────────────────────────────────┐
│         ORDERS WINDOW                   │
│  (Order ID, Symbol, Action, Qty, etc.)  │
│                                         │
├─────────────────────────────────────────┤
│        POSITIONS WINDOW                 │
│  (Symbol, Qty, Avg Price, P/L, etc.)    │
│                                         │
├─────────────────────────────────────────┤
│         STATUS BAR                      │
│  (Data usage, Memory, Streams)          │
├─────────────────────────────────────────┤
│         HELP BAR                        │
│  (Keyboard shortcuts)                   │
└─────────────────────────────────────────┘
```

## Features

### Order Display Window

Displays all orders with the following columns:
- **Order ID**: Unique identifier (truncated to 12 chars)
- **Symbol**: Stock ticker symbol
- **Action**: Buy, Sell, BuyToCover, SellShort, etc.
- **Qty**: Order quantity
- **Type**: Market, Limit, StopMarket, StopLimit
- **Price**: Limit or stop price (or "Market")
- **DateTime**: Order timestamp (MM/dd hh:mm:ss)
- **Status**: ACK, Open, Filled, Canceled, etc.

Orders are displayed in reverse chronological order (most recent first).

### Position Display Window

Displays all open positions with the following columns:
- **Symbol**: Stock ticker symbol
- **Quantity**: Number of shares held
- **Avg Price**: Average purchase price
- **Last**: Last traded price
- **P/L**: Profit/Loss in dollars (color-coded)
- **P/L %**: Profit/Loss percentage
- **Market Val**: Current market value

**Color Coding:**
- Green: Profitable positions (P/L > 0)
- Red: Loss positions (P/L < 0)
- White: Break-even (P/L = 0)

### Status Bar

Displays real-time application metrics:
- **Data**: Total data received from APIs (in KB)
- **Memory**: Application memory usage (in KB)
- **Streams**: Number of active data streams

### Keyboard Shortcuts

The TUI supports the following keyboard shortcuts:

| Key | Action |
|-----|--------|
| `q` or `Q` | Quit application |
| `r` or `R` | Refresh display |
| `?` | Show help (currently just refreshes) |

**Note:** These are similar to the GUI shortcuts but adapted for terminal input. Unlike the GUI which uses Qt's QShortcut system, the TUI uses raw ncurses input handling with QSocketNotifier for Qt event loop integration.

### Terminal Resize Handling

The TUI automatically handles terminal resize events (`KEY_RESIZE`) by:
1. Destroying existing windows
2. Recreating windows with new dimensions
3. Refreshing the display

## Implementation Details

### Dependencies

**TUI-specific:**
- ncurses library (for terminal UI)
- POSIX stdin handling (for keyboard input)

**Conditional Compilation:**
- Code guarded by `#ifdef GUI_ENABLED`
- TUI-specific files excluded from GUI builds
- GUI-specific files excluded from TUI builds

### Data Storage

- **Orders**: `QHash<QString, Order>` - keyed by Order ID
- **Positions**: `QHash<QString, Position>` - keyed by Position ID

**Note:** QHash is used instead of QMap because Order lacks a default constructor, which is required by QMap::operator[].

### Input Handling

The TUI uses Qt's event loop for input handling:

1. **QSocketNotifier** monitors stdin (STDIN_FILENO)
2. When input is available, `handleInput()` is called
3. ncurses `getch()` reads the character
4. Character is processed and appropriate action taken

This integration allows the TUI to work seamlessly with Qt's event loop and signals/slots.

### Terminal Scrolling Prevention

To prevent mouse wheel scrolling from interfering with the ncurses display:
- `scrollok(stdscr, FALSE)` - Disables scrolling on the main screen
- `idlok(stdscr, FALSE)` - Disables hardware scrolling
- `nonl()` - Disables newline translation that can cause scrolling
- Individual windows also have scrolling disabled with `scrollok(window, FALSE)`

This ensures the display remains static and doesn't get misaligned when terminal scroll events occur.

### Color Scheme

If terminal supports colors (checked via `has_colors()`):

- **Color Pair 1** (Blue background, White text): Headers
- **Color Pair 2** (Green): Positive P/L
- **Color Pair 3** (Red): Negative P/L
- **Color Pair 4** (Yellow): Status bar
- **Color Pair 5** (Cyan): Help text

### Display Updates

Updates are triggered by:
- New order received → `onNewOrderReceived()`
- New position received → `onNewPositionReceived()`
- Position deleted → `onPositionDeleted()`
- Data usage changed → `onTSClientDataUsageUpdate()`
- Memory usage changed → `onMemoryUsageUpdate()`
- Stream count changed → `onStreamCountUpdate()`

All updates call `refreshDisplay()` which redraws all windows.

### Logging

To avoid interfering with the ncurses display, the TUI uses a different logging output strategy than the GUI:

- **GUI Mode**: Logs are written to both stdout and a log widget in the UI
- **TUI Mode**: Logs are written to stderr instead of stdout, allowing ncurses to maintain exclusive control of stdout for the terminal UI

This means when running the TUI:
- The ncurses interface appears on stdout (your terminal display)
- Application logs appear on stderr (can be redirected separately)
- Example: `./L2Trader 2>logs.txt` redirects logs to a file while keeping the TUI visible

All logs are also written to a timestamped log file in `~/.local/state/L2Trader/logs/` regardless of the output mode.

## Limitations

The current TUI implementation is minimal and does not support:

- Order entry/modification
- Stock symbol selection
- Real-time charts
- Market depth quotes
- Breaking news display
- Multiple tabs/views
- Configuration dialogs
- Mouse input

These features remain exclusive to the GUI. The TUI is designed for monitoring only.

## Future Enhancements

Potential improvements for the TUI:

1. **Order Entry**: Add keyboard shortcuts to place/cancel orders
2. **Symbol Selection**: Allow entering symbols to monitor specific stocks
3. **Scrolling**: Enable scrolling through long lists of orders/positions
4. **Filtering**: Filter orders by status, positions by P/L, etc.
5. **Detailed Views**: Press Enter on an item to see detailed information
6. **Color Themes**: Support for different color schemes
7. **Configuration**: Read/write TUI-specific settings
8. **Help Screen**: Detailed help overlay (triggered by '?')

## Code Organization

### Source Files

- `Src/FrontEnd/TUI/TUIFrontend.h` - TUI class declaration
- `Src/FrontEnd/TUI/TUIFrontend.cpp` - TUI implementation
- `Src/FrontEnd/FrontEnd.h` - Abstract base class

### Build Configuration

- `CMakeLists.txt` - Root build configuration with `ENABLE_GUI` option
- `Src/CMakeLists.txt` - Source file filtering based on build mode

### Excluded Files (TUI Mode)

The following files are excluded when building for TUI:
- `GUIAuthHandler.cpp/h` - Qt dialog-based authentication
- `ShortcutSettings.cpp/h` - Qt QKeySequence shortcuts
- All files in `FrontEnd/GUI/` directory
- QColor-related code in `MarketHours.cpp/h`

### Conditional Code

Code that differs between GUI and TUI:

```cpp
#ifdef GUI_ENABLED
    // Qt Widgets code
    appFrontend = new GUIFrontend(mainAlgo);
#else
    // ncurses code
    appFrontend = new TUIFrontend(mainAlgo);
    static_cast<TUIFrontend*>(appFrontend)->initialize();
#endif
```

## Testing

Since the TUI requires a terminal and cannot easily be automated, testing should be done manually:

1. Build TUI version
2. Run in terminal: `./build/Src/L2Trader --criterias=Example_Config/selection_criteria.ini`
3. To redirect logs to a file and keep TUI clean: `./build/Src/L2Trader --criterias=Example_Config/selection_criteria.ini 2>app.log`
4. Authenticate with TradeStation
5. Observe orders and positions display
6. Test keyboard shortcuts (q, r, ?)
7. Test terminal resize
8. Verify colors display correctly

**Note**: Logs are written to stderr in TUI mode, so use `2>` to redirect them to a file if needed.

## Troubleshooting

### Mouse wheel scrolling erases display
The TUI has been configured to prevent terminal scrolling that can interfere with the ncurses display. If you still experience issues with mouse wheel events:
- The display is designed to be static (not scrollable)
- All content is shown within the available terminal space
- If you need to see more data, resize your terminal to be larger
- Press 'r' to force a refresh if the display becomes corrupted

### Logs interfere with display
If you see log messages overwriting the TUI, ensure you're running the TUI build (not GUI build). The TUI should automatically write logs to stderr. You can redirect stderr to hide logs: `./L2Trader 2>/dev/null` or save them: `./L2Trader 2>app.log`

### Terminal doesn't support colors
The TUI will still work but without color highlighting. Check `has_colors()` return value.

### Display is garbled
Try resizing the terminal or pressing 'r' to refresh.

### Keyboard input not working
Ensure the terminal is in raw mode and stdin is not being redirected.

### Application crashes on exit
ncurses cleanup is handled in destructor. Ensure proper Qt application shutdown.

### Building fails with ncurses errors
Install ncurses development library: `sudo apt-get install libncurses-dev`
