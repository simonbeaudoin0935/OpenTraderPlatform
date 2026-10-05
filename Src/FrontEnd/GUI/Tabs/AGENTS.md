# Tabs/ - GUI Tab Components - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Tabs/`
**Purpose**: Tab components for the main tab widget in GUIFrontend
**Pattern**: Individual QWidget-based tab implementations

The Tabs folder contains all tab components that appear in the main QTabWidget of the GUIFrontend window. Each tab provides specific functionality for managing different aspects of the application.

**Tab Order** (as displayed in GUI):
1. **Trade** - Main trading interface (not in this directory, embedded in GUIFrontend)
2. **Risk Management** - Account-scoped risk configuration and runtime controls
3. **Records Info** - Explore recorded market data
4. **Logging** - Live log display and filtering
5. **Cache** - Bar cache management
6. **Shortcuts** - Keyboard shortcut configuration
7. **Config** - Application configuration
8. **Credentials** - Connection controls and optional OS Keyring/YubiKey storage selector (applies after restart)

Strategy management no longer lives in a dedicated tab. The relevant GUI pieces
are `../Widgets/StrategyQuickView/`, `../Dialogs/StrategyLoadDialog.*`, and
`../Widgets/StrategyLogWidget/`.

## Tab Components

### RiskTab (RiskTab.h/cpp)

**Purpose**: Configure and inspect account-level risk manager behavior

**Features**:
- Enable/disable risk management per account
- Daily drawdown budget configuration
- Drawdown basis mode selection:
  - `EquityPeak` (trailing)
  - `TodaysProfitLoss` (trailing)
  - `RealizedProfitLoss` (trailing)
  - `TodaysProfitLossFromBaseline` (non-trailing baseline)
- Per-trade planned-loss limit, max shares/notional, max entries/day, max open positions
- Optional cooldown after a losing close
- Runtime controls: `Reset Day` and `Unlock Trading`
- Tooltip help clarifying basis formulas and remaining-headroom interpretation

**Integration**:
- Loads/saves account-scoped settings via `MainAlgo` risk-config APIs
- Refreshes runtime state from `MainAlgo::riskStatusChanged(accountId)`

### CacheTab (CacheTab.h/cpp)

**Purpose**: Manage bar cache and view cache statistics

**Features**:
- View cache status per symbol
- Display cache size and bar count
- Clear cache (selected symbol or all)
- View disk usage
- Database path display
- Memory/disk cache statistics

**Key Methods**:
```cpp
// Update cache display with current statistics
void updateCacheDisplay();

// Clear cache for specific symbol or all
void onClearCacheClicked();

// Refresh cache statistics
void refreshCacheStats();
```

**UI Components**:
- Table view showing cached symbols
- Clear button for cache management
- Statistics labels (total size, entry count)
- Database path display

**Signal Interface**:
```cpp
signals:
    void cacheClearRequested(const QString& symbol);
    void cacheRefreshRequested();
```

---

### ConfigTab (ConfigTab.h/cpp)

**Purpose**: Application configuration and settings

**Features**:
- Trading mode selection (Live/Simulation)
- API endpoint configuration
- Cache settings
- Database paths
- Logging configuration
- Performance tuning options
- **Auto-Timeframe Thresholds** (see below)

**Key Methods**:
```cpp
// Load current configuration
void loadSettings();

// Save configuration changes
void saveSettings();

// Validate configuration before saving
bool validateConfig();
```

**UI Components**:
- Combo boxes for mode selection
- Line edits for paths and URLs
- Checkboxes for feature toggles
- Spin boxes for numeric settings

**Signal Interface**:
```cpp
signals:
    void configChanged();
    void tradingModeChanged(TradingMode mode);
```

#### Auto-Timeframe Thresholds Section

The ConfigTab includes an "Auto-Timeframe Thresholds" group box that allows users to configure when the chart automatically switches timeframes based on visible time range.

**UI Layout**:
```
┌─ Auto-Timeframe Thresholds ─────────────────────────────┐
│ Timeframe  Lower (min)  Upper (min)                      │
│ 1m         [30     ▲▼]  [150    ▲▼]                     │
│ 5m         [120    ▲▼]  [480    ▲▼]                     │
│ 15m        [240    ▲▼]  [960    ▲▼]                     │
│ 30m        [480    ▲▼]  [1440   ▲▼]                     │
│ 1h         [720    ▲▼]  [2880   ▲▼]                     │
│ 4h         [1440   ▲▼]  [10080  ▲▼]                     │
└─────────────────────────────────────────────────────────┘
```

**Behavior**:
- When visible minutes < lower threshold → switch to smaller timeframe
- When visible minutes > upper threshold → switch to larger timeframe
- Overlapping thresholds provide hysteresis to prevent oscillation
- Settings stored in `AppState.ini` under `Config/AutoTF/<TF>/Lower` and `Upper`

**Default Values**:
| Timeframe | Lower (min) | Upper (min) |
|-----------|-------------|-------------|
| 1m        | 30          | 150         |
| 5m        | 120         | 480         |
| 15m       | 240         | 960         |
| 30m       | 480         | 1440        |
| 1h        | 720         | 2880        |
| 4h        | 1440        | 10080       |

**Implementation**:
```cpp
// Storage structure in ConfigTab.h
struct TfThresholdWidgets {
    QSpinBox* lowerSpinBox;
    QSpinBox* upperSpinBox;
};
QMap<TimeFrame, TfThresholdWidgets> m_tfThresholds;

// Values read by ZoomAndPanning::checkAutoTimeFrame()
const int lowerThreshold = settings.value(
    QString("Config/AutoTF/%1/Lower").arg(tfKey), defaultLower).toInt();
```

---

### LoggingTab (LoggingTab.h/cpp)

**Purpose**: Live log display and filtering

**Features**:
- Real-time log message display (QTextEdit)
- Category filter checkboxes (dynamically generated)
- Log level dropdown (Debug, Info, Warning, Critical)
- Auto-scroll toggle
- Clear logs button
- Save logs to file
- Color-coded log levels
- Timestamp display

**Key Methods**:
```cpp
// Append new log message
void appendLog(const QString& category, const QString& message,
               QtMsgType level);

// Update category filters
void updateCategoryFilters();

// Clear log display
void clearLogs();

// Save logs to file
void saveLogsToFile();
```

**Log Categories** (auto-generated from code):
- TSClient.*
- MainAlgo.*
- BarCache.*
- GUI.*
- Strategy.*
- ReplayEngine.*
- ... and more

**Color Coding**:
- Debug: Gray
- Info: White
- Warning: Yellow
- Critical: Red

**UI Components**:
- QTextEdit for log display
- Category filter checkbox group (dynamically created)
- Log level combo box
- Auto-scroll checkbox
- Clear and Save buttons

**Signal Interface**:
```cpp
public slots:
    void onLogMessageReceived(const QString& category,
                             const QString& message,
                             QtMsgType level);
```

**Connection**:
```cpp
// Connect to LogBroadcaster
connect(&LogBroadcaster::getInstance(), &LogBroadcaster::logMessage,
        loggingTab, &LoggingTab::onLogMessageReceived);
```

---

### RecorderTab — REMOVED

The RecorderTab was removed in Phase 3 of the Databento migration. Recording is no longer done
via live TradeStation streams. Instead, replay data is downloaded as `.dbn.zst` files via
`DBClient::downloadReplayData()`, triggered from the DownloadsTab download section.

---

### DownloadsTab (DownloadsTab.h/cpp)

**Purpose**: Download and browse recorded Databento market data (`.dbn.zst` files)

**Features**:
- **Download section** at top: date picker, CSV file input, manual symbols input, download button with progress
- **Three-column browser** below: recorded days → symbols → file details (Mbp10/Trades)
- Sequential download (one symbol at a time via `DBClient::downloadReplayData`)
- CSV parser for stock lists (e.g., `Example_Config/NBI.csv`)
- Skips already-downloaded symbols (`DBClient::hasReplayData`)
- Persists last CSV path and manual symbols in `AppState.ini`

**UI Layout**:
```
┌─ Download Replay Data ──────────────────────────────────────────────┐
│ Date:         [2026-02-28 ▼]                                        │
│ CSV File:     [/path/to/NBI.csv                ] [Browse…]          │
│ Extra Symbols:[AAPL, NVDA, MSFT                ]                    │
│ [Download]    Downloading 5/260: ABUS…                              │
│ ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  5/260               │
└─────────────────────────────────────────────────────────────────────┘
┌─────────────────┬──────────────────────────┬──────────────────────┐
│  Recorded Days  │  Symbols                 │  File Details        │
│ Date  Files Size│ Symbol  Mbp10  Trades    │ Symbol: AAPL         │
│ 2026-02-28      │ AAPL    ✓      ✓         │ Mbp10: Available     │
│ 2026-02-27      │ MSFT    ✓      ✓         │   Size: 12.3 MB     │
│ ...             │ ...                      │ Trades: Available    │
│                 │                          │   Size: 2.1 MB       │
└─────────────────┴──────────────────────────┴──────────────────────┘
```

**Key Methods**:
```cpp
// CSV parsing — static, extracts first-column tickers, skips header
static QStringList parseSymbolCsv(const QString& filePath);

// Manual symbols — parses comma-separated text input
QStringList parseManualSymbols() const;

// Build download queue — merge CSV + manual, deduplicate, skip existing
QStringList buildDownloadQueue(const QDate& date) const;

// Sequential download — starts next symbol on replayDownloadFinished
void startNextDownload();

// Persistence
void saveCsvPath();       // AppState: RecordsInfo/LastCsvPath
void saveManualSymbols(); // AppState: RecordsInfo/ManualSymbols
```

**Download Flow**:
1. User clicks "Download" → `onDownloadClicked()`
2. Validates: API key present, date valid, symbols non-empty
3. Builds queue via `buildDownloadQueue()` (CSV + manual, deduplicated, minus already-downloaded)
4. Disables button, shows progress bar
5. Calls `DBClient::downloadReplayData(symbol, date)` for first symbol
6. `onDownloadFinished()` fires → increments progress → calls `startNextDownload()`
7. On all complete: re-enables button, refreshes browser, shows summary

**Data Sources**:
- Mbp10: `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_mbp10.dbn.zst`
- Trades: `~/.local/share/OpenTraderPlatform/ReplayData/{YYYY-MM-DD}/{SYMBOL}_trades.dbn.zst`

**AppState.ini Keys**:
- `RecordsInfo/LastCsvPath` — last CSV file path used
- `RecordsInfo/ManualSymbols` — last comma-separated manual symbols

**Signal Interface**:
```cpp
// No signals emitted (self-contained tab)
// Connects to DBClient::replayDownloadFinished for download tracking
```

---

### ShortcutsTab (ShortcutsTab.h/cpp)

**Purpose**: Keyboard shortcut configuration

**Features**:
- View all keyboard shortcuts
- Customize keyboard shortcuts
- Reset to defaults
- Conflict detection
- Export/import shortcut configurations

**Key Methods**:
```cpp
// Load current shortcuts
void loadShortcuts();

// Update shortcut for action
void updateShortcut(const QString& action, const QKeySequence& sequence);

// Check for conflicts
bool hasConflict(const QKeySequence& sequence);

// Reset all shortcuts to defaults
void resetToDefaults();
```

**UI Components**:
- Table view showing action-shortcut pairs
- Edit shortcut dialog
- Reset button
- Conflict warning labels

**Signal Interface**:
```cpp
signals:
    void shortcutChanged(const QString& action, const QKeySequence& sequence);
```

**Default Shortcuts**:
- Ctrl+Q: Quit
- F5: Refresh
- Ctrl+W: Close window
- Ctrl+N: New order
- Esc: Cancel/close dialog

---

### Strategy Management Moved Out of Tabs/

`Tabs/StrategiesTab/` no longer exists.

Current strategy-management surfaces are:

- `../Widgets/StrategyQuickView/` — compact tree view embedded in the Trade tab
- `../Dialogs/StrategyLoadDialog.*` — executable loader and schema-driven parameter editor
- `../Widgets/StrategyLogWidget/` — per-strategy log panel

Reference: see `Doc/STRATEGY.md` for the current runtime and GUI integration model.

---

## Common Patterns

### Tab Initialization

All tabs follow a similar initialization pattern:

```cpp
class MyTab : public QWidget {
    Q_OBJECT

public:
    explicit MyTab(QWidget* parent = nullptr);
    ~MyTab();

private:
    void setupUI();       // Create and layout widgets
    void setupStyles();   // Apply dark theme styling
    void setupConnections(); // Connect signals/slots

    // UI components
    QLabel* m_headerLabel;
    // ... other widgets
};
```

### Dark Theme Styling

Tabs inherit the dark theme from GUIFrontend:

```cpp
void setupStyles() {
    // Background colors match dark theme
    setStyleSheet(R"(
        QWidget {
            background-color: #353535;
            color: white;
        }
        QLabel {
            color: white;
        }
        QPushButton {
            background-color: #454545;
            color: white;
            border: 1px solid #555;
        }
    )");
}
```

### Signal/Slot Connections

Tabs communicate with backend via signals:

```cpp
// In GUIFrontend constructor
connect(m_loggingTab, &LoggingTab::logLevelChanged,
        &LogBroadcaster::getInstance(), &LogBroadcaster::setMinimumLevel);

connect(m_cacheTab, &CacheTab::cacheClearRequested,
        &MainAlgo::getInstance(), &MainAlgo::clearBarCache);
```

## Adding a New Tab

To add a new tab to GUIFrontend:

1. **Create Tab Class**:
```cpp
// NewTab.h
class NewTab : public QWidget {
    Q_OBJECT
public:
    explicit NewTab(QWidget* parent = nullptr);
    ~NewTab();
private:
    void setupUI();
    void setupStyles();
    void setupConnections();
};
```

2. **Implement in NewTab.cpp**:
```cpp
#include "NewTab.h"
NewTab::NewTab(QWidget* parent) : QWidget(parent) {
    setupUI();
    setupStyles();
    setupConnections();
}
```

3. **Add to GUIFrontend**:
```cpp
// In GUIFrontend constructor
NewTab* newTab = new NewTab(this);
ui->tabWidget->addTab(newTab, "New Tab Name");
```

4. **Add to CMakeLists.txt** (if using explicit file lists):
```cmake
set(GUI_SOURCES
    ...
    Tabs/NewTab.cpp
    Tabs/NewTab.h
)
```
Note: With GLOB_RECURSE, this step is automatic.

## Threading Considerations

All tab components run on the **main (GUI) thread**:
- Receive signals from worker threads via Qt::QueuedConnection
- Update UI components directly (no thread synchronization needed)
- Emit signals to worker threads (automatically queued by Qt)

## Performance Tips

### Update Throttling

For high-frequency updates (e.g., logging, recording stats):

```cpp
QTimer* m_updateThrottle = new QTimer(this);
m_updateThrottle->setInterval(100);  // Max 10 updates/sec
m_updateThrottle->setSingleShot(true);

connect(m_updateThrottle, &QTimer::timeout, this, [this]() {
    updateDisplay(m_pendingData);
});

void onDataReceived(const Data& data) {
    m_pendingData = data;
    if (!m_updateThrottle->isActive()) {
        updateDisplay(m_pendingData);
        m_updateThrottle->start();
    }
}
```

### Lazy Loading

Load data only when tab becomes visible:

```cpp
// In GUIFrontend
connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
    QWidget* tab = ui->tabWidget->widget(index);
    if (auto* myTab = qobject_cast<MyTab*>(tab)) {
        myTab->onTabActivated();  // Load data when activated
    }
});
```

## Related Components

- **GUIFrontend**: Main window containing the tab widget
- **MainAlgo**: Backend providing data for tabs
- **Settings**: Persistent configuration storage
- **LogBroadcaster**: Centralized logging system

## Related Documentation

- `../AGENTS.md`: Main GUI documentation
- `Doc/FRONTEND.md`: Complete frontend architecture
- `Doc/STRATEGY.md`: Strategy runtime and strategy-management UI
