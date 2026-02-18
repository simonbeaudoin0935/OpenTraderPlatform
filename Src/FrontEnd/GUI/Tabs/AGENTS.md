# Tabs/ - GUI Tab Components - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Tabs/`
**Purpose**: Tab components for the main tab widget in GUIFrontend
**Pattern**: Individual QWidget-based tab implementations

The Tabs folder contains all tab components that appear in the main QTabWidget of the GUIFrontend window. Each tab provides specific functionality for managing different aspects of the application.

**Tab Order** (as displayed in GUI):
1. **Trade** - Main trading interface (not in this directory, embedded in GUIFrontend)
2. **Strategies** - Strategy plugin management
3. **Recorder** - Live market data recording
4. **Records Info** - Explore recorded market data (NEW)
5. **Logging** - Live log display and filtering
6. **Cache** - Bar cache management
7. **Shortcuts** - Keyboard shortcut configuration
8. **Config** - Application configuration

## Tab Components

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

### RecorderTab (RecorderTab.h/cpp)

**Purpose**: Market data recording controls (integrated into main application)

**Features**:
- Start/stop recording within the application
- CSV file input for symbol list
- Select timeframe for recording
- Recording status display
- Output directory selection
- Progress indicator
- Live recording statistics

**Key Methods**:
```cpp
// Start market data recording
void startRecording();

// Stop ongoing recording
void stopRecording();

// Load symbols from CSV file
void loadSymbolsFromCSV(const QString& filePath);

// Update recording statistics
void updateRecordingStats(int symbolsRecorded, qint64 bytesWritten);
```

**UI Components**:
- Symbol list display (QListWidget)
- Browse button for CSV file selection
- Timeframe combo box (1min, 5min, 1day, etc.)
- Start/Stop button
- Output directory selector
- Progress bar
- Statistics labels

**Signal Interface**:
```cpp
signals:
    void recordingStarted(const QStringList& symbols, const QString& timeframe);
    void recordingStopped();

public slots:
    void onRecordingStatusChanged(bool isRecording);
    void onRecordingStatsUpdated(int count, qint64 bytes);
```

**File Format**:
CSV files are stored in `~/.cache/L2Trader/RecordedLiveData/Bars/`

---

### RecordsInfoTab (RecordsInfoTab.h/cpp)

**Purpose**: Explore recorded market data

**Features**:
- Three-column layout for browsing recordings
- List all recorded days with statistics
- View stocks recorded for each day
- Display detailed metrics per stock (bars + market depth)
- Market depth availability indicator (API limited to 10 streams)
- Read-only interface

**UI Layout**:
```
┌─────────────────┬─────────────────┬─────────────────┐
│   Recorded Days │ Stocks in Day   │  Stock Details  │
│                 │                 │                 │
│ Date  Bars Size │ Symbol Bars Dpt │ Symbol: AAPL    │
│ 2026-02-05      │ AAPL   1234 Yes │                 │
│ 2026-02-06      │ MSFT   2345 No  │ Bars Data:      │
│ ...             │ ...             │ - Count: 1234   │
│                 │                 │ - First: ...    │
│                 │                 │ - Last: ...     │
│                 │                 │ - Duration: ... │
│                 │                 │                 │
│                 │                 │ Market Depth:   │
│                 │                 │ - Status: Avail │
│                 │                 │ - Count: 567    │
└─────────────────┴─────────────────┴─────────────────┘
```

**Key Methods**:
```cpp
// Scan RecordedLiveData/Bars directory for .db files
void scanRecordedDays();

// Load stocks from selected day's database
void loadStocksForDay(const QDate& date);

// Load detailed metrics for selected stock
void loadStockDetails(const QDate& date, const QString& symbol);

// Query metrics from both bars and depth databases
StockMetrics queryStockMetrics(const QString& barsDbPath,
                                const QString& depthDbPath,
                                const QString& symbol);

// Check if stock exists in market depth database
bool checkStockInDatabase(const QString& dbPath, const QString& symbol);

// Format helpers
QString formatFileSize(qint64 bytes);
QString formatDuration(qint64 durationMs);
QString formatTimestamp(qint64 epochMs);
```

**Data Sources**:
- Bars: `~/.cache/L2Trader/RecordedLiveData/Bars/{YYYY-MM-DD}.db`
- Market Depth: `~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/{YYYY-MM-DD}.db`

**Database Queries**:
```sql
-- Get all stocks from bars database
SELECT DISTINCT stockTicker FROM bars ORDER BY stockTicker;

-- Get metrics for a stock
SELECT COUNT(*), MIN(epochMs), MAX(epochMs) FROM bars WHERE stockTicker = ?;
SELECT COUNT(*), MIN(epochMs), MAX(epochMs) FROM market_depth_quotes WHERE stockTicker = ?;
```

**Market Depth Limitation**:
Due to TradeStation API limit (10 concurrent market depth streams), not all stocks will have market depth data. The tab shows:
- "Yes" or "No" indicator in stocks table
- "Available" or "Not Available" status in details panel
- Market depth metrics only displayed when available

**StockMetrics Structure**:
```cpp
struct StockMetrics {
    QString symbol;
    qint64 barCount;
    qint64 firstTimestampMs;
    qint64 lastTimestampMs;
    bool hasMarketDepth;
    qint64 depthCount;
    qint64 depthFirstTimestampMs;
    qint64 depthLastTimestampMs;
};
```

**Signal Interface**:
```cpp
// No signals emitted (read-only tab)
```

**Usage**:
1. Tab auto-scans recorded days on creation
2. Click a date → stocks list populates
3. Click a stock → details display on right
4. Click "Refresh" to rescan directory

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

### StrategiesTab/ (Subdirectory)

**Purpose**: Strategy plugin management interface

See `StrategiesTab/AGENTS.md` for detailed documentation.

**Quick Overview**:
- Load strategy plugins (.so files)
- Start/stop strategies
- View strategy status and logs
- Crash isolation per strategy
- Strategy cards with metrics
- Strategy grid layout
- Load dialog for selecting plugins

**Main Components**:
- StrategiesTab.h/cpp - Main tab widget
- StrategyCard.h/cpp - Individual strategy display card
- StrategyGridWidget.h/cpp - Grid layout for strategy cards
- StrategyLoadDialog.h/cpp - Dialog for loading new strategies
- StrategyDetailsPanel.h/cpp - Detailed strategy information panel
- StrategyTile.h/cpp - Compact strategy tile view

**Reference**: See `Doc/STRATEGY.md` for complete strategy system documentation

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
- `StrategiesTab/AGENTS.md`: Strategy tab details
- `Doc/FRONTEND.md`: Complete frontend architecture
- `Doc/STRATEGY.md`: Strategy plugin system
