# TUI/ - Text User Interface - Agent Instructions

The TUI directory contains the lightweight ncurses terminal interface for L2Trader.

## Overview

**Location**: `Src/FrontEnd/TUI/`
**Framework**: ncurses library with Qt Core event loop
**Main Class**: TUIFrontend (implements FrontEnd abstract base class)
**Purpose**: Read-only monitoring for headless servers or terminal-only environments

**Current State**: Minimal implementation - monitoring only, no order entry

## Architecture

### TUIFrontend (TUIFrontend.h/cpp)

Implements FrontEnd interface with ncurses display:

```cpp
class TUIFrontend : public FrontEnd {
    Q_OBJECT

public:
    explicit TUIFrontend(MainAlgo* p_mainAlgo, QObject* parent = nullptr);
    ~TUIFrontend();

    void initialize();  // MUST be called after constructor

    // FrontEnd interface implementation
    void onNewOrderReceived(const QString& accountId, const Order& order) override;
    void onNewPositionReceived(const QString& accountId, const Position& position) override;
    void onPositionDeleted(const QString& accountId, const QString& positionId) override;
    void onTSClientDataUsageUpdate(qsizetype bytes) override;
    void onMemoryUsageUpdate(qsizetype bytes) override;

    // Not implemented (minimal TUI)
    void onTradeStationAccountsReceived(const QVector<Account>&) override {}
    void onCurrentHighlightedStockBarReceived(const QString&, const Bar&) override {}
    void onCurrentHighlightedReceivedNewLevel2(...) override {}
    void onCurrentHighlightedReceivedNewTrade(...) override {}
    void onBalanceUpdated(const Balance&) override {}

private:
    MainAlgo* m_mainAlgo;

    // ncurses windows
    WINDOW* m_ordersWindow;
    WINDOW* m_positionsWindow;
    WINDOW* m_statusWindow;
    WINDOW* m_helpWindow;

    // Data storage
    QHash<QString, Order> m_orders;           // OrderID → Order
    QHash<QString, Position> m_positions;     // PositionID → Position

    // Status metrics
    qsizetype m_dataUsage;
    qsizetype m_memoryUsage;
    int m_streamCount;

    // Input handling
    QSocketNotifier* m_inputNotifier;

    // Internal methods
    void setupNcurses();
    void cleanupNcurses();
    void createWindows();
    void destroyWindows();
    void refreshDisplay();
    void drawOrders();
    void drawPositions();
    void drawStatus();
    void drawHelp();
    void handleInput();
};
```

## Window Layout

```
┌─────────────────────────────────────────┐
│         ORDERS WINDOW (40% height)      │
│ Order ID  | Symbol | Action | Qty | ... │
│ abc123    | AAPL   | Buy    | 100 | ... │
│ def456    | MSFT   | Sell   |  50 | ... │
├─────────────────────────────────────────┤
│       POSITIONS WINDOW (40% height)     │
│ Symbol | Qty | Avg $ | Last $ | P/L | % │
│  AAPL  | 100 |$150.00|$155.00|+$500|+3.3│
│  MSFT  | -50 |$300.00|$295.00|+$250|+1.7│
├─────────────────────────────────────────┤
│          STATUS BAR (2 lines)           │
│ Data: 1.2MB | Memory: 45MB | Streams: 3 │
│ Auth: ✓ Connected | Last Update: 14:23  │
├─────────────────────────────────────────┤
│          HELP BAR (1 line)              │
│ q:Quit | r:Refresh | ?:Help             │
└─────────────────────────────────────────┘
```

## ncurses Integration

### Initialization

```cpp
void TUIFrontend::initialize() {
    setupNcurses();
    createWindows();
    refreshDisplay();
}

void TUIFrontend::setupNcurses() {
    // Initialize ncurses
    initscr();              // Initialize screen
    cbreak();               // Disable line buffering
    noecho();               // Don't echo typed characters
    keypad(stdscr, TRUE);   // Enable function keys
    nodelay(stdscr, TRUE);  // Non-blocking getch()
    curs_set(0);            // Hide cursor

    // Initialize colors if supported
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Headers
        init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Positive P/L
        init_pair(3, COLOR_RED, COLOR_BLACK);     // Negative P/L
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Status
        init_pair(5, COLOR_CYAN, COLOR_BLACK);    // Help
    }

    // Disable scrolling
    scrollok(stdscr, FALSE);
    idlok(stdscr, FALSE);
    nonl();
}
```

### Cleanup

```cpp
void TUIFrontend::~TUIFrontend() {
    cleanupNcurses();
}

void TUIFrontend::cleanupNcurses() {
    destroyWindows();
    endwin();  // Restore terminal
}
```

### Window Creation

```cpp
void TUIFrontend::createWindows() {
    int height, width;
    getmaxyx(stdscr, height, width);

    // Calculate heights
    int ordersHeight = height * 0.40;
    int positionsHeight = height * 0.40;
    int statusHeight = 2;
    int helpHeight = 1;

    // Create windows (y, x, height, width)
    m_ordersWindow = newwin(ordersHeight, width, 0, 0);
    m_positionsWindow = newwin(positionsHeight, width, ordersHeight, 0);
    m_statusWindow = newwin(statusHeight, width,
                           ordersHeight + positionsHeight, 0);
    m_helpWindow = newwin(helpHeight, width,
                         ordersHeight + positionsHeight + statusHeight, 0);

    // Disable scrolling per window
    scrollok(m_ordersWindow, FALSE);
    scrollok(m_positionsWindow, FALSE);
    scrollok(m_statusWindow, FALSE);
    scrollok(m_helpWindow, FALSE);
}
```

## Input Handling

### Qt Integration with QSocketNotifier

Integrate ncurses with Qt event loop:

```cpp
void TUIFrontend::initialize() {
    setupNcurses();
    createWindows();

    // Monitor stdin for keyboard input
    m_inputNotifier = new QSocketNotifier(STDIN_FILENO,
                                         QSocketNotifier::Read, this);
    connect(m_inputNotifier, &QSocketNotifier::activated,
            this, &TUIFrontend::handleInput);
}

void TUIFrontend::handleInput() {
    int ch = getch();  // Non-blocking

    if (ch == ERR) return;  // No input available

    switch (ch) {
        case 'q':
        case 'Q':
            QCoreApplication::quit();
            break;

        case 'r':
        case 'R':
            refreshDisplay();
            break;

        case KEY_RESIZE:
            destroyWindows();
            createWindows();
            refreshDisplay();
            break;

        case '?':
            // Show help (future)
            break;

        default:
            // Ignore unknown keys
            break;
    }
}
```

### Terminal Resize Handling

Handle terminal resize gracefully:
```cpp
case KEY_RESIZE:
    destroyWindows();      // Delete old windows
    createWindows();       // Create with new dimensions
    refreshDisplay();      // Repaint everything
    break;
```

## Display Methods

### Draw Orders

```cpp
void TUIFrontend::drawOrders() {
    werase(m_ordersWindow);

    // Draw header
    wattron(m_ordersWindow, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(m_ordersWindow, 0, 0, "%-10s %-8s %-8s %-6s %-8s %-12s",
              "Order ID", "Symbol", "Action", "Qty", "Type", "Status");
    wattroff(m_ordersWindow, COLOR_PAIR(1) | A_BOLD);

    // Draw orders
    int row = 1;
    for (const Order& order : m_orders) {
        // Color-code by status
        if (order.status() == OrderStatus::Filled) {
            wattron(m_ordersWindow, COLOR_PAIR(2));  // Green
        } else if (order.status() == OrderStatus::Canceled ||
                   order.status() == OrderStatus::Rejected) {
            wattron(m_ordersWindow, COLOR_PAIR(3));  // Red
        }

        mvwprintw(m_ordersWindow, row, 0, "%-10s %-8s %-8s %-6d %-8s %-12s",
                  order.orderId().left(10).toStdString().c_str(),
                  order.symbol().toStdString().c_str(),
                  tradeActionToString(order.tradeAction()).toStdString().c_str(),
                  order.quantity(),
                  orderTypeToString(order.orderType()).toStdString().c_str(),
                  orderStatusToString(order.status()).toStdString().c_str());

        wattroff(m_ordersWindow, COLOR_PAIR(2) | COLOR_PAIR(3));
        row++;
    }

    box(m_ordersWindow, 0, 0);
    wrefresh(m_ordersWindow);
}
```

### Draw Positions

```cpp
void TUIFrontend::drawPositions() {
    werase(m_positionsWindow);

    // Draw header
    wattron(m_positionsWindow, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(m_positionsWindow, 0, 0, "%-8s %-6s %-10s %-10s %-10s %-8s",
              "Symbol", "Qty", "Avg Price", "Last Price", "P/L", "P/L %");
    wattroff(m_positionsWindow, COLOR_PAIR(1) | A_BOLD);

    // Draw positions
    int row = 1;
    for (const Position& position : m_positions) {
        double pnl = (position.lastPrice() - position.averagePrice()) *
                     position.quantity();
        double pnlPercent = (pnl / (position.averagePrice() *
                                    position.quantity())) * 100.0;

        // Color-code by P/L
        if (pnl > 0) {
            wattron(m_positionsWindow, COLOR_PAIR(2));  // Green
        } else if (pnl < 0) {
            wattron(m_positionsWindow, COLOR_PAIR(3));  // Red
        }

        mvwprintw(m_positionsWindow, row, 0, "%-8s %6d $%9.2f $%9.2f $%9.2f %7.2f%%",
                  position.symbol().toStdString().c_str(),
                  position.quantity(),
                  position.averagePrice(),
                  position.lastPrice(),
                  pnl,
                  pnlPercent);

        wattroff(m_positionsWindow, COLOR_PAIR(2) | COLOR_PAIR(3));
        row++;
    }

    box(m_positionsWindow, 0, 0);
    wrefresh(m_positionsWindow);
}
```

### Draw Status

```cpp
void TUIFrontend::drawStatus() {
    werase(m_statusWindow);

    wattron(m_statusWindow, COLOR_PAIR(4));

    // Line 1: Resource metrics
    mvwprintw(m_statusWindow, 0, 0,
              "Data: %.2f MB | Memory: %.2f MB | Streams: %d",
              m_dataUsage / (1024.0 * 1024.0),
              m_memoryUsage / (1024.0 * 1024.0),
              m_streamCount);

    // Line 2: Connection status
    bool authenticated = TSClient::getInstance().isAuthenticated();
    mvwprintw(m_statusWindow, 1, 0,
              "Auth: %s | Last Update: %s",
              authenticated ? "✓ Connected" : "✗ Disconnected",
              QDateTime::currentDateTime().toString("hh:mm:ss").toStdString().c_str());

    wattroff(m_statusWindow, COLOR_PAIR(4));
    wrefresh(m_statusWindow);
}
```

### Draw Help

```cpp
void TUIFrontend::drawHelp() {
    werase(m_helpWindow);

    wattron(m_helpWindow, COLOR_PAIR(5));
    mvwprintw(m_helpWindow, 0, 0,
              "q:Quit | r:Refresh | ?:Help");
    wattroff(m_helpWindow, COLOR_PAIR(5));

    wrefresh(m_helpWindow);
}
```

### Refresh All

```cpp
void TUIFrontend::refreshDisplay() {
    drawOrders();
    drawPositions();
    drawStatus();
    drawHelp();
    doupdate();  // Single screen update
}
```

## Logging in TUI Mode

### Avoid stdout Conflicts

TUI uses stdout for ncurses display. Logs must go to stderr:

```cpp
void customMessageHandler(QtMsgType type,
                         const QMessageLogContext& context,
                         const QString& msg) {
    QTextStream err(stderr);  // Use stderr, NOT stdout
    err << formatLogMessage(type, msg) << Qt::endl;

    // Also write to log file
    writeToFile(msg);
}

// Install handler
qInstallMessageHandler(customMessageHandler);
```

### Usage Examples

```bash
# Run TUI with logs to file
./L2Trader 2>app.log

# Run TUI with logs hidden
./L2Trader 2>/dev/null

# Run TUI with logs to separate terminal
./L2Trader 2> >(tee app.log >&2)
```

## Limitations

### Current TUI Limitations

TUI is **monitoring only** and does NOT support:
- Order entry/modification
- Stock symbol selection (uses last selected from settings)
- Real-time charts
- Market depth quotes display
- Configuration dialogs
- Mouse input
- Interactive symbol search

### Future Enhancements

Planned features:
- Simple order entry form
- Symbol selection menu
- Text-based candlestick chart (ASCII art)
- Compressed market depth display
- Configuration via text menus
- Mouse support (ncurses mouse events)

## Performance Considerations

### Update Frequency

Avoid overwhelming terminal with updates:
```cpp
QTimer* m_refreshTimer = new QTimer(this);
m_refreshTimer->setInterval(250);  // 4 updates/sec max

connect(m_refreshTimer, &QTimer::timeout, this, &TUIFrontend::refreshDisplay);
m_refreshTimer->start();
```

### Lazy Rendering

Only redraw changed windows:
```cpp
void TUIFrontend::onNewOrderReceived(const QString& accountId, const Order& order) {
    m_orders[order.orderId()] = order;
    drawOrders();  // Only redraw orders window
    // Don't redraw positions/status/help
}
```

## Error Handling

### Terminal Size

Handle terminals too small:
```cpp
void TUIFrontend::createWindows() {
    int height, width;
    getmaxyx(stdscr, height, width);

    if (height < 20 || width < 80) {
        cleanupNcurses();
        fprintf(stderr, "Error: Terminal too small (min 80x20)\n");
        QCoreApplication::exit(1);
        return;
    }

    // Create windows...
}
```

### ncurses Errors

Check ncurses return values:
```cpp
if (initscr() == nullptr) {
    fprintf(stderr, "Error: Failed to initialize ncurses\n");
    return;
}
```

## Testing TUI

### Unit Tests

Test data structures and logic:
```cpp
TEST(TUIFrontend, StoresOrders) {
    TUIFrontend tui(nullptr);
    Order order;
    order.setOrderId("test-123");

    tui.onNewOrderReceived("account", order);

    EXPECT_TRUE(tui.hasOrder("test-123"));
}
```

### Manual Testing

Test in various terminals:
- xterm
- gnome-terminal
- tmux/screen
- kitty
- alacritty
- SSH sessions

Test with various sizes:
- 80x24 (minimum)
- 120x40 (recommended)
- 200x60 (large)

## Build Configuration

TUI is built when GUI is disabled:

```cmake
if(NOT ENABLE_GUI)
    find_package(Curses REQUIRED)
    target_link_libraries(L2Trader PRIVATE ${CURSES_LIBRARIES})
    target_include_directories(L2Trader PRIVATE ${CURSES_INCLUDE_DIR})
endif()
```

## Related Agent Instructions

- `../AGENTS.md`: FrontEnd abstract interface
- `../GUI/AGENTS.md`: GUI implementation (comparison)
- `../../Algo/AGENTS.md`: Backend coordination
- `../../Core/AGENTS.md`: Core components

## Related Documentation

- `Doc/FRONTEND.md`: Complete frontend documentation
- `Doc/ARCHITECTURE.md`: System architecture
- ncurses documentation: https://invisible-island.net/ncurses/
