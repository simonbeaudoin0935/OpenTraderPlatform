# StrategiesTab/ - Strategy Plugin Management UI - Agent Instructions

## Overview

**Location**: `Src/FrontEnd/GUI/Tabs/StrategiesTab/`
**Purpose**: User interface for loading, managing, and monitoring trading strategy plugins
**Pattern**: Component-based UI with cards, grids, and dialogs

The StrategiesTab provides a visual interface for the strategy plugin system, allowing users to load, start, stop, and monitor trading strategies at runtime.

## Architecture

### Component Hierarchy

```
StrategiesTab (Main tab widget)
    │
    ├── StrategyGridWidget (Grid layout of strategy cards)
    │       │
    │       └── StrategyCard[] (Individual strategy displays)
    │               │
    │               ├── StrategyTile (Compact view)
    │               └── StrategyDetailsPanel (Expanded view)
    │
    └── StrategyLoadDialog (Plugin selection dialog)
```

## Components

### StrategiesTab (StrategiesTab.h/cpp)

**Purpose**: Main container for strategy management UI

**Features**:
- Load strategy plugins from disk
- Display loaded strategies in a grid
- Show strategy status (Stopped, Running, Crashed)
- Global controls (Load, Unload All, Start All, Stop All)
- Statistics summary

**Key Methods**:
```cpp
/// Initialize the tab UI and connect signals
explicit StrategiesTab(QWidget* parent = nullptr);

/// Destructor - ensures proper cleanup
~StrategiesTab();

/// Display dialog to load a new strategy plugin
void onLoadStrategyClicked();

/// Unload all strategies
void onUnloadAllClicked();

/// Start all loaded strategies
void onStartAllClicked();

/// Stop all running strategies
void onStopAllClicked();

/// Update summary statistics
void updateSummaryStats();
```

**UI Layout**:
```
┌──────────────────────────────────────────────┐
│ STRATEGIES                                   │
│ ┌──────────────────────────────────────────┐ │
│ │ [Load Strategy] [Unload All]             │ │
│ │ [Start All] [Stop All]                   │ │
│ └──────────────────────────────────────────┘ │
│ ┌──────────────────────────────────────────┐ │
│ │ Summary: 3 loaded, 2 running, 0 crashed  │ │
│ └──────────────────────────────────────────┘ │
│ ┌──────────────────────────────────────────┐ │
│ │  Strategy Grid Widget (scrollable)       │ │
│ │  ┌──────┐ ┌──────┐ ┌──────┐             │ │
│ │  │Card 1│ │Card 2│ │Card 3│             │ │
│ │  └──────┘ └──────┘ └──────┘             │ │
│ └──────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

**Signal Interface**:
```cpp
signals:
    void strategyLoadRequested(const QString& pluginPath);
    void strategyUnloadRequested(const QString& strategyId);
    void strategyStartRequested(const QString& strategyId);
    void strategyStopRequested(const QString& strategyId);
```

---

### StrategyCard (StrategyCard.h/cpp)

**Purpose**: Display individual strategy information and controls

**Features**:
- Strategy name and ID
- Current status (Stopped, Running, Crashed)
- Start/Stop button
- Unload button
- Expandable details panel
- Status color coding (green=running, red=crashed, gray=stopped)
- Strategy metrics display

**Key Methods**:
```cpp
/// Create a strategy card for given strategy
explicit StrategyCard(const QString& strategyId, 
                     const Poland& strategyName,
                     QWidget* parent = nullptr);

/// Update card with new strategy status
void updateStatus(StrategyStatus status);

/// Show/hide the details panel
void toggleDetailsPanel();

/// Update strategy metrics (P&L, trades, etc.)
void updateMetrics(const StrategyMetrics& metrics);
```

**UI Components**:
- Strategy name label
- Status indicator (colored circle)
- Start/Stop button (toggles state)
- Unload button (X icon)
- Expand/collapse button (▼/▲)
- Details panel (hidden by default)

**Status Color Coding**:
```cpp
enum class StrategyStatus {
    Stopped,    // Gray circle
    Running,    // Green circle
    Crashed     // Red circle
};
```

---

### StrategyGridWidget (StrategyGridWidget.h/cpp)

**Purpose**: Grid layout container for strategy cards

**Features**:
- Responsive grid layout (adjusts to window width)
- Scroll support for many strategies
- Dynamic card addition/removal
- Maintains card ordering

**Key Methods**:
```cpp
/// Add a new strategy card to the grid
void addStrategyCard(StrategyCard* card);

/// Remove a strategy card from the grid
void removeStrategyCard(const QString& strategyId);

/// Clear all strategy cards
void clearAllCards();

/// Get all strategy cards
QVector<StrategyCard*> getAllCards() const;
```

**Layout Algorithm**:
```cpp
// Calculate columns based on available width
int columns = qMax(1, width() / CARD_MIN_WIDTH);

// Arrange cards in grid
for (int i = 0; i < cards.size(); ++i) {
    int row = i / columns;
    int col = i % columns;
    layout->addWidget(cards[i], row, col);
}
```

---

### StrategyLoadDialog (StrategyLoadDialog.h/cpp)

**Purpose**: Dialog for selecting and loading strategy plugins

**Features**:
- Browse file system for .so files
- Display plugin information before loading
- Validate plugin compatibility
- Recent plugins list
- Default strategy directory shortcut

**Key Methods**:
```cpp
/// Show dialog and return selected plugin path
/// @return Empty string if cancelled, file path if selected
QString getSelectedPluginPath();

/// Validate that the selected file is a valid strategy plugin
bool validatePlugin(const QString& path);

/// Load list of recently loaded plugins
void loadRecentPlugins();
```

**UI Components**:
- File browser tree view
- Plugin info display (name, version, description)
- Recent plugins combo box
- Browse button
- Load/Cancel buttons

**Validation**:
```cpp
bool validatePlugin(const QString& path) {
    // Check file exists
    if (!QFile::exists(path)) return false;
    
    // Check file extension
    if (!path.endsWith(".so")) return false;
    
    // Try to load as QLibrary
    QLibrary lib(path);
    if (!lib.load()) return false;
    
    // Check for required symbols
    if (!lib.resolve("getStrategyName")) return false;
    if (!lib.resolve("createStrategy")) return false;
    
    lib.unload();
    return true;
}
```

---

### StrategyDetailsPanel (StrategyDetailsPanel.h/cpp)

**Purpose**: Expanded view showing detailed strategy information

**Features**:
- Strategy description
- Current configuration parameters
- Performance metrics (P&L, trades, win rate)
- Recent trades log
- Error messages (if crashed)
- Strategy-specific visualizations

**Key Methods**:
```cpp
/// Update the details panel with new information
void updateDetails(const StrategyInfo& info);

/// Show error message in the panel
void showError(const QString& errorMessage);

/// Clear and hide the panel
void clearAndHide();
```

**UI Components**:
- Description label
- Metrics table (P&L, Trades, Win%, Sharpe, etc.)
- Recent trades list
- Error message display
- Strategy logs scrollable text area

**Metrics Display**:
```
┌─────────────────────────────────────┐
│ Performance Metrics                 │
├─────────────────────────────────────┤
│ Total P&L:        +$1,234.56        │
│ Total Trades:     42                │
│ Win Rate:         64.3%             │
│ Sharpe Ratio:     1.82              │
│ Max Drawdown:     -$156.78          │
└─────────────────────────────────────┘
```

---

### StrategyTile (StrategyTile.h/cpp)

**Purpose**: Compact strategy display (alternative to card view)

**Features**:
- Minimal single-row display
- Status indicator
- Name and ID
- Quick start/stop button
- Suitable for many strategies

**Key Methods**:
```cpp
/// Create a compact strategy tile
explicit StrategyTile(const QString& strategyId,
                     const QString& strategyName,
                     QWidget* parent = nullptr);

/// Update tile status
void updateStatus(StrategyStatus status);
```

**UI Layout** (single row):
```
[●] ExampleStrategy (ID: abc123) [Start] [X]
```

---

## Data Flow

### Loading a Strategy

```
User clicks "Load Strategy" button
    │
    ▼
StrategiesTab::onLoadStrategyClicked()
    │
    ├─ Show StrategyLoadDialog
    │       │
    │       └─ User selects plugin file
    │               │
    │               ▼
    │       Dialog validates plugin
    │               │
    │               └─ Returns plugin path
    │
    ▼
emit strategyLoadRequested(pluginPath)
    │
    ▼
GUIFrontend forwards to StrategyManager
    │
    ▼
StrategyManager::loadStrategy(pluginPath)
    │
    ├─ Load plugin from disk
    ├─ Resolve strategy symbols
    ├─ Create strategy instance
    ├─ Assign unique ID
    │
    └─ emit strategyLoaded(strategyId, name)
            │
            ▼
StrategiesTab receives signal
    │
    ├─ Create StrategyCard
    └─ Add to StrategyGridWidget
```

### Starting a Strategy

```
User clicks "Start" button on card
    │
    ▼
StrategyCard::onStartClicked()
    │
    └─ emit startRequested(strategyId)
            │
            ▼
StrategiesTab::onStrategyStartRequested()
    │
    └─ emit strategyStartRequested(strategyId)
            │
            ▼
GUIFrontend forwards to StrategyManager
    │
    ▼
StrategyManager::startStrategy(strategyId)
    │
    ├─ Move strategy to worker thread
    ├─ Call strategy->onStart()
    ├─ Connect to data feeds
    │
    └─ emit strategyStarted(strategyId)
            │
            ▼
StrategyCard::updateStatus(Running)
```

### Handling Strategy Crash

```
Strategy crashes in worker thread
    │
    ▼
StrategyManager detects crash (QThread::finished signal)
    │
    └─ emit strategyCrashed(strategyId, errorMessage)
            │
            ▼
StrategiesTab::onStrategyCrashed()
    │
    ├─ Find corresponding StrategyCard
    ├─ Update status to Crashed (red)
    ├─ Show error in DetailsPanel
    └─ Log crash to LoggingTab
```

## Crash Isolation

Each strategy runs in its own process/thread to prevent crashes from affecting the main application:

```cpp
// Strategy runs in isolated worker thread
QThread* strategyThread = new QThread(this);
strategy->moveToThread(strategyThread);

// Handle crashes gracefully
connect(strategyThread, &QThread::finished, this, [this, strategyId]() {
    if (!isExpectedShutdown(strategyId)) {
        emit strategyCrashed(strategyId, "Unexpected thread termination");
    }
});
```

## Configuration

Strategy plugins can be configured via:

1. **Configuration Dialog**: Edit parameters before/after loading
2. **Config Files**: Persistent TOML/JSON config per strategy
3. **Environment Variables**: System-level settings

Example configuration:
```toml
[strategy.example]
symbol = "AAPL"
timeframe = "1min"
max_position_size = 100
risk_per_trade = 0.02
```

## Threading Model

- **StrategiesTab**: Runs on main (GUI) thread
- **StrategyCards**: Runs on main thread (UI updates)
- **Strategy Instances**: Run on separate worker threads
- **Communication**: Signal/slot (queued connections)

## Performance Considerations

### Lazy Updates

Only update visible cards:
```cpp
void StrategyGridWidget::updateVisibleCards() {
    for (auto* card : m_cards) {
        if (card->isVisible()) {
            card->updateMetrics();
        }
    }
}
```

### Throttled Metrics Updates

Avoid overwhelming UI with updates:
```cpp
QTimer* m_metricsThrottle = new QTimer(this);
m_metricsThrottle->setInterval(500);  // Update every 500ms
m_metricsThrottle->setSingleShot(true);

connect(m_metricsThrottle, &QTimer::timeout, this, &StrategyCard::updateMetrics);
```

## Error Handling

### Plugin Load Failure

```cpp
void onPluginLoadFailed(const QString& path, const QString& error) {
    QMessageBox::critical(this, "Plugin Load Failed",
        QString("Failed to load strategy plugin:\n%1\n\nError: %2")
            .arg(path).arg(error));
}
```

### Strategy Crash

```cpp
void onStrategyCrashed(const QString& strategyId, const QString& error) {
    // Update UI to show crashed state
    StrategyCard* card = findCard(strategyId);
    if (card) {
        card->updateStatus(StrategyStatus::Crashed);
        card->showError(error);
    }
    
    // Log the crash
    qCCritical(StrategyLog) << "Strategy crashed:" << strategyId << error;
}
```

## Related Components

- **StrategyManager**: Backend strategy management (Src/Strategy/)
- **Strategy SDK**: Base classes and interfaces for strategy plugins
- **StrategyBase**: Abstract base class all strategies inherit from
- **LogBroadcaster**: Logging system for strategy output

## Related Documentation

- `../../AGENTS.md`: Main GUI documentation
- `../AGENTS.md`: Tabs documentation
- `Doc/STRATEGY.md`: Complete strategy plugin system
- `Src/Strategy/AGENTS.md`: Backend strategy management

## Plugin Development

To develop a strategy plugin, see `Strategies/` directory for examples:
- `ExampleStrategy/`: Template strategy implementation
- `CrashTestStrategy/`: Test crash isolation
- `HistoricalBarsStrategy/`: Example using historical data

Each plugin must:
1. Inherit from `StrategyBase`
2. Implement required virtual methods
3. Export required symbols for dynamic loading
4. Be compiled as a shared library (.so)
