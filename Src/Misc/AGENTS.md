# Misc/ Directory - Utilities and Helpers - Agent Instructions

The Misc directory contains utility classes, application settings, logging infrastructure, and shared constants.

## Overview

**Location**: `Src/Misc/`
**Purpose**: Provide common utilities, configuration, and infrastructure services

## Key Files

### CONSTANTS.h

**THE** centralized location for ALL application constants.

**Rules**:
- **NEVER** define constants inline in source files
- **ALL** constants must be defined here in appropriate namespaces
- Use `constexpr` for compile-time constants
- Organize into logical namespaces

**Namespaces**:
```cpp
namespace TradingHours {
    constexpr QTime TRADING_START_TIME(9, 30, 0);      // 9:30 AM ET
    constexpr QTime TRADING_END_TIME(16, 0, 0);        // 4:00 PM ET
    constexpr QTime PRE_MARKET_START(4, 0, 0);         // 4:00 AM ET
    constexpr QTime AFTER_HOURS_END(20, 0, 0);         // 8:00 PM ET
    const QString TIMEZONE = "America/New_York";
}

namespace TSClientEndpoints {
    constexpr const char* BASE_URL = "https://api.tradestation.com/v3";
    constexpr const char* GET_BARS = "/marketdata/barcharts";
    constexpr const char* GET_QUOTES = "/marketdata/quotes";
    constexpr const char* GET_ACCOUNTS = "/brokerage/accounts";
    constexpr const char* POST_ORDER = "/orderexecution/orders";
    constexpr const char* STREAM_BARS = "wss://api.tradestation.com/v3/marketdata/stream/barcharts";
    // ... many more
}

namespace AuthConstants {
    constexpr const char* OAUTH_AUTHORIZE_URL = "https://signin.tradestation.com/authorize";
    constexpr const char* OAUTH_TOKEN_URL = "https://signin.tradestation.com/oauth/token";
    constexpr int TOKEN_EXPIRY_SECONDS = 1200;         // 20 minutes
    constexpr int TOKEN_REFRESH_BUFFER_SECONDS = 5;   // Refresh 5s early
    constexpr const char* REDIRECT_URI = "http://localhost";
}

namespace ChartConstants {
    constexpr int DEFAULT_VISIBLE_BARS = 30;
    constexpr int MIN_VISIBLE_BARS = 10;
    constexpr int MAX_VISIBLE_BARS = 500;
    const QColor PRE_MARKET_COLOR = QColor(255, 165, 0, 180);      // Orange
    const QColor AFTER_HOURS_COLOR = QColor(138, 43, 226, 180);   // Violet
}

namespace BarFlags {
    constexpr quint8 IS_REAL_TIME = 0x01;
    constexpr quint8 IS_END_OF_DAY = 0x02;
    constexpr quint8 IS_PREVIOUS = 0x04;
    // ... bit flags for Bar status
}

namespace FileSystemConstants {
    const QString LOGS_DIR = "logs";
    const QString CACHE_DIR = "bars";
    const QString CONFIG_DIR = ".config/L2Trader";
}

namespace MarketDepthConstants {
    constexpr size_t MAX_CONCURRENT_STREAMS = 10;    // TradeStation API hard limit
    constexpr int QUEUE_PROCESS_DELAY_MS = 1000;     // Delay before opening queued stream (TCP close propagation)
}
```

**Usage**:
```cpp
#include "Misc/CONSTANTS.h"

QTime marketOpen = TradingHours::TRADING_START_TIME;
QString endpoint = TSClientEndpoints::BASE_URL + TSClientEndpoints::GET_BARS;
if (StreamMarketDepthQuote::getNumberOfMarketDepthStreams() >= MarketDepthConstants::MAX_CONCURRENT_STREAMS) {
    // Will be queued
}
```

### Assume.h

**Assertion macros for debug builds** (replaces Q_ASSERT)

**Philosophy**: ASSERT-First Strategy
- Assert expected state upfront
- Don't use defensive null checks (hides bugs)
- Only use conditionals when null/empty is valid runtime possibility

**Macros**:
```cpp
// Object pointer assertions
OBJ_ASSUME_DIFF(ptr, nullptr)  // Assert ptr != nullptr
OBJ_ASSUME_EQ(a, b)            // Assert a == b
OBJ_ASSUME_NEQ(a, b)           // Assert a != b

// Container assertions
CONTAINER_ASSUME_NOT_EMPTY(container)  // Assert !container.isEmpty()
CONTAINER_ASSUME_SIZE(container, size) // Assert container.size() == size

// Value range assertions
VALUE_ASSUME_GT(value, threshold)   // Assert value > threshold
VALUE_ASSUME_GE(value, threshold)   // Assert value >= threshold
VALUE_ASSUME_LT(value, threshold)   // Assert value < threshold
VALUE_ASSUME_LE(value, threshold)   // Assert value <= threshold
```

**Behavior**:
- **Debug builds**: Triggers assertion with file/line info
- **Release builds**: Compiles to no-op (zero runtime cost)

**Example**:
```cpp
// ✗ WRONG - Defensive null check hides bugs
if (m_replayEngine != nullptr) {
    m_replayEngine->pauseReplay();
}

// ✓ CORRECT - Assert expectation upfront
OBJ_ASSUME_DIFF(m_replayEngine, nullptr);
m_replayEngine->pauseReplay();
```

### Logging.h/cpp

**Centralized logging system with categories**

**Architecture**:
- **LogBroadcaster**: Singleton that distributes log messages
- **LoggingConfig**: Singleton for runtime configuration
- Category-based filtering
- Multiple outputs (file, console, GUI)

**Logging Categories**:
Auto-generated from source code using `generate_logging_categories.py`:
```cpp
Q_LOGGING_CATEGORY(TSClientLog, "TSClient")
Q_LOGGING_CATEGORY(TSClientAuthLog, "TSClient.auth")
Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")
Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")
// ... many more
```

**Usage**:
```cpp
#include "Misc/Logging.h"

qCDebug(TSClientLog) << "Debug message";
qCInfo(TSClientLog) << "Info message";
qCWarning(TSClientLog) << "Warning message";
qCCritical(TSClientLog) << "Critical error";
```

**Runtime Control**:
```cpp
// Enable specific categories
QLoggingCategory::setFilterRules("TSClient*=true\nMainAlgo*=false");

// Change log level
LoggingConfig::getInstance().setLogLevel(QtWarningMsg);

// Enable/disable file logging
LoggingConfig::getInstance().setFileLoggingEnabled(true);
```

**Log File Location**:
- Linux: `~/.local/share/L2Trader/logs/L2Trader_YYYY-MM-DD.log`
- Rotated daily
- Old logs kept for 30 days (configurable)

**Custom Message Handler**:
```cpp
void initLogging() {
    qInstallMessageHandler(customMessageHandler);
}

void customMessageHandler(QtMsgType type,
                         const QMessageLogContext& context,
                         const QString& msg) {
    // Format message
    QString formatted = formatLogMessage(type, context, msg);

    // Broadcast to all consumers
    LogBroadcaster::getInstance().broadcast(formatted);

    // Write to file if enabled
    if (LoggingConfig::getInstance().isFileLoggingEnabled()) {
        writeToFile(formatted);
    }
}
```

### Settings.h/cpp

**Application settings management** (wrapper around QSettings)

**Storage**:
- Linux: `~/.config/L2Trader/L2Trader.conf`
- INI format
- Automatic sync on setValue

**Usage**:
```cpp
#include "Misc/Settings.h"

// Save setting
Settings::setValue("GUI/WindowGeometry", geometry);
Settings::setValue("MainAlgo/BalancePollingInterval", 5000);

// Load setting with default
QByteArray geometry = Settings::getValue("GUI/WindowGeometry", QByteArray());
int interval = Settings::getValue("MainAlgo/BalancePollingInterval", 5000);

// Check if exists
if (Settings::contains("LastSymbol")) {
    QString symbol = Settings::getValue("LastSymbol");
}

// Remove setting
Settings::remove("OldSetting");

// Clear all settings
Settings::clear();
```

**Organization**:
Settings organized by component:
- `GUI/*` - GUI-specific settings
- `MainAlgo/*` - Algorithm settings
- `BarCache/*` - Cache configuration
- `Auth/*` - Authentication preferences
- `Logging/*` - Log configuration

### SecureStorage.h/cpp

**Encrypted credential storage via QKeychain**

**Purpose**: Store sensitive data (tokens, secrets) with OS-level encryption

**Platform Support**:
- **Linux**: GNOME Keyring / KWallet (libsecret)
- **macOS**: macOS Keychain
- **Windows**: Credential Manager (DPAPI)

**Fallback**: XOR obfuscation (development only, NOT secure)

**API**:
```cpp
#include "Misc/SecureStorage.h"

// Store values
QMap<QString, QString> values;
values["access_token"] = "eyJhbGc...";
values["refresh_token"] = "abc123...";
bool success = SecureStorage::storeValuesSync("L2Trader", values, 5000);

// Retrieve values
QStringList keys = {"access_token", "refresh_token"};
QMap<QString, QString> retrieved =
    SecureStorage::retrieveValuesSync("L2Trader", keys, 5000);

// Delete values
SecureStorage::deleteValuesSync("L2Trader", keys, 5000);

// Check availability
if (!SecureStorage::isSecureStorageAvailable()) {
    qCWarning() << "QKeychain not available, using fallback";
}
```

**Security Notes**:
- **ALWAYS** compile with QKeychain for production
- Never log retrieved values
- Clear values from memory after use
- Fallback is NOT cryptographically secure

### TimeFrame.h/cpp

**Trading timeframe definitions and utilities**

**Enum**:
```cpp
enum class TimeFrame {
    OneMinute,
    FiveMinute,
    FifteenMinute,
    ThirtyMinute,
    OneHour,
    Daily,
    Weekly,
    Monthly
};
```

**Utilities**:
```cpp
// Convert to string for API
QString TimeFrame::toString(TimeFrame tf);  // "1min", "5min", etc.

// Convert from string
TimeFrame TimeFrame::fromString(const QString& str);

// Get duration in seconds
int TimeFrame::toSeconds(TimeFrame tf);

// Human-readable name
QString TimeFrame::toDisplayName(TimeFrame tf);  // "1 Minute", "5 Minutes", etc.
```

### ShortcutSettings.h/cpp

**Keyboard shortcut management** (singleton)

**Purpose**: Centralize and configure keyboard shortcuts

**API**:
```cpp
#include "Misc/ShortcutSettings.h"

// Get shortcut (with default)
QKeySequence quit = ShortcutSettings::getShortcut("quit", QKeySequence::Quit);

// Set custom shortcut
ShortcutSettings::setShortcut("quit", QKeySequence(Qt::CTRL | Qt::Key_Q));

// Reset to default
ShortcutSettings::resetShortcut("quit");

// Reset all
ShortcutSettings::resetAll();
```

**Default Shortcuts**:
- `quit`: Ctrl+Q
- `refresh`: F5
- `close`: Ctrl+W
- `newOrder`: Ctrl+N
- `cancelOrder`: Ctrl+X
- `help`: F1

### ArgumentParser.h/cpp

**CLI argument parsing**

**Supported Arguments**:
```bash
./L2Trader --help                   # Show help
./L2Trader --version                # Show version
./L2Trader --gui                    # Force GUI mode
./L2Trader --tui                    # Force TUI mode
./L2Trader --log-level debug        # Set log level
./L2Trader --config /path/to/config # Custom config file
```

**API**:
```cpp
ArgumentParser parser(argc, argv);

if (parser.hasHelp()) {
    parser.printHelp();
    return 0;
}

bool guiMode = parser.isGUIMode();
QString logLevel = parser.getLogLevel();
QString configPath = parser.getConfigPath();
```

### ThreadStats.h/cpp

**Thread performance monitoring**

**Purpose**: Measure thread CPU usage and event processing

**API**:
```cpp
ThreadStats stats;
stats.startMonitoring(&m_thread);

// Later...
double cpuUsage = stats.getCPUUsage();      // Percentage
qint64 events = stats.getEventCount();       // Events processed
qint64 avgEventTime = stats.getAvgEventTime(); // Microseconds

stats.stopMonitoring();
```

**Usage**:
- Debug performance issues
- Monitor thread responsiveness
- Detect event loop starvation

### generate_logging_categories.py

**Python script to generate logging categories**

Scans source code for Q_LOGGING_CATEGORY declarations and generates:
- Category list
- Filter rule helpers
- Documentation

**Usage**:
```bash
python3 Src/Misc/generate_logging_categories.py
```

## Design Patterns

### Singleton Pattern

Used by:
- **LogBroadcaster**: Single log distribution point
- **LoggingConfig**: Single configuration source
- **ShortcutSettings**: Single shortcut registry

**Implementation**:
```cpp
class Singleton {
public:
    static Singleton& getInstance() {
        static Singleton instance;
        return instance;
    }

    Q_DISABLE_COPY_MOVE(Singleton)

private:
    Singleton() = default;
    ~Singleton() = default;
};
```

### Static Utility Classes

Used by:
- **Settings**: Static methods wrapping QSettings
- **SecureStorage**: Static methods wrapping QKeychain

No instance needed, all methods static.

## Common Patterns

### Using Constants

```cpp
#include "Misc/CONSTANTS.h"

// ✓ CORRECT - Use centralized constant
QTime start = TradingHours::TRADING_START_TIME;

// ✗ WRONG - Don't define inline
const QTime TRADING_START(9, 30, 0);  // NO!
```

### Using Assertions

```cpp
#include "Misc/Assume.h"

// ✓ CORRECT - Assert expected state
OBJ_ASSUME_DIFF(m_stream, nullptr);
m_stream->start();

// ✗ WRONG - Defensive check
if (m_stream != nullptr) {  // Hides bug!
    m_stream->start();
}
```

### Using Logging

```cpp
#include "Misc/Logging.h"

Q_LOGGING_CATEGORY(MyClassLog, "MyClass")

void MyClass::someMethod() {
    qCDebug(MyClassLog) << "Method called";

    if (errorCondition) {
        qCWarning(MyClassLog) << "Warning condition";
    }
}
```

### Using Settings

```cpp
#include "Misc/Settings.h"

// Save on close
void saveState() {
    Settings::setValue("LastSymbol", m_currentSymbol);
}

// Restore on startup
void restoreState() {
    QString symbol = Settings::getValue("LastSymbol", "AAPL");
    loadSymbol(symbol);
}
```

## Testing Considerations

### Unit Tests

Test utilities independently:
```cpp
TEST(TimeFrame, ConvertsToString) {
    EXPECT_EQ(TimeFrame::toString(TimeFrame::OneMinute), "1min");
    EXPECT_EQ(TimeFrame::toString(TimeFrame::FiveMinute), "5min");
}

TEST(Settings, SavesAndLoadsValues) {
    Settings::setValue("Test/Key", 42);
    EXPECT_EQ(Settings::getValue("Test/Key", 0), 42);
    Settings::remove("Test/Key");
}
```

### Mock Singletons

For testing components that use singletons:
- Provide test interface
- Reset singleton state between tests
- Use dependency injection where possible

## Related Agent Instructions

- `../AGENTS.md`: Source directory overview
- `../SQL/AGENTS.md`: SQL query management
- `../Core/AGENTS.md`: Core application components

## Related Documentation

- `Doc/DEVELOPMENT.md`: Development guidelines
- `Doc/ARCHITECTURE.md`: System architecture
