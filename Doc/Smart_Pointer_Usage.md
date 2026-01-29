# Smart Pointer Usage in L2Trader

## Overview

This document describes the smart pointer usage patterns adopted throughout the L2Trader codebase to ensure proper memory management and prevent memory leaks.

## Composition Over Pointers

**PREFER composition (direct member objects) over pointers** when designing classes. This is a fundamental design principle in the L2Trader codebase.

### When to Use Composition

Use direct member objects instead of pointers when:
- The object has a clear owner (the containing class)
- Polymorphism is not needed
- The object's lifetime matches the containing class
- The object is not optional

**Good example**:
```cpp
class StockInstruments : public QObject
{
    QString symbol;
    BarCache barCache;                      // Composition - direct member
    MarketDepthQuoteReceiver marketDepthQuoteReceiver; // Composition - direct member
};
```

**Avoid**:
```cpp
class StockInstruments : public QObject
{
    QString symbol;
    BarCache* barCache;                     // Pointer - unnecessary indirection
    MarketDepthQuoteReceiver* marketDepthQuoteReceiver; // Pointer - unnecessary indirection
};
```

### Benefits of Composition

1. **No null checks needed** - The object always exists
2. **Automatic lifetime management** - No manual cleanup required
3. **Clear ownership** - Compiler enforces ownership at compile-time
4. **Better encapsulation** - Members are initialized in constructor initializer list
5. **More efficient** - No heap allocation overhead
6. **Simpler code** - No pointer dereferences needed

### When Pointers Are Necessary

Use pointers (raw or smart) only when:
- **Polymorphism is required** - Base class pointers to derived objects
- **Object lifetime extends beyond container** - Object needs to outlive the containing class
- **Object is optional** - May or may not exist (use `std::optional` as alternative)
- **Object is very large** - Copying would be expensive
- **Forward declaration needed** - To break circular dependencies
- **Qt parent-child ownership** - Following Qt's memory management model
- **Dynamic collections** - Objects managed in containers like maps

## Smart Pointer Types and Usage

### Qt Parent-Child Ownership (No Smart Pointers Needed)

**When to use**: Qt objects (QWidget, QTimer, QLayout, etc.) that have a parent specified in their constructor.

**Why**: Qt's parent-child ownership system automatically manages memory. When a parent is deleted, it deletes all its children.

**Examples**:
```cpp
// Correct - Qt manages the timer
QTimer* m_timer = new QTimer(this);

// Correct - layout is owned by parent widget
QVBoxLayout* layout = new QVBoxLayout(parentWidget);

// Correct - button is owned by parent
QPushButton* button = new QPushButton("Text", this);
```

**Pattern**: Pass `this` or another QObject parent as the last parameter to the constructor.

### QPointer for Qt Objects That May Be Deleted

**When to use**: Qt objects that may be deleted independently or whose lifetime you don't control.

**Why**: `QPointer` automatically becomes null when the pointed-to object is deleted, preventing dangling pointer issues.

**Examples**:
```cpp
// Stream objects that may close/delete themselves on error
QPointer<StreamBars> m_stream;
QPointer<StreamOrders> m_stream;
QPointer<StreamPositions> m_stream;
QPointer<StreamMarketDepthQuote> m_stream;
```

**Files using QPointer**:
- `Src/Algo/MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.h`
- `Src/Algo/OrdersReceiver/OrdersReceiver.h`
- `Src/Algo/PositionsReceiver/PositionsReceiver.h`
- `Src/Core/Cache/BarCache/BarCache.h`
- `Src/Recorder/LiveStreamDB.h`

### std::unique_ptr for Exclusive Ownership

**When to use**: Objects with exclusive ownership that are NOT Qt objects with parents.

**Why**: Provides automatic cleanup, move semantics, and clear ownership semantics.

**Examples**:
```cpp
// Non-Qt UI object
std::unique_ptr<Ui::GUIFrontend> ui;

// Database objects without Qt parent
std::unique_ptr<LiveStreamDB> m_liveBarsDB;
std::unique_ptr<LiveStreamDB> m_liveMarketDepthQuoteDB;

// Receiver objects (QObjects without parents initially)
std::unique_ptr<PositionsReceiver> m_positionReceiver;
std::unique_ptr<OrdersReceiver> m_orderReceiver;
```

**Construction**:
```cpp
m_liveBarsDB = std::make_unique<LiveStreamDB>(StreamType::Bars, dbPath, tickers);
```

**Cleanup**:
```cpp
// Automatic on scope exit, or explicit:
m_liveBarsDB.reset();
```

**Passing to Qt connections**:
```cpp
connect(m_positionReceiver.get(), &PositionsReceiver::signal,
        this, &MainAlgo::slot, Qt::UniqueConnection);
```

**Files converted to use std::unique_ptr**:
- `Src/FrontEnd/GUI/GUIFrontend.h/cpp` (ui member - not a QObject)
- `Src/FrontEnd/GUI/Tabs/RecorderTab.h/cpp` (database members - optional lifetime)
- `Src/Recorder/main.cpp` (LiveStreamDB objects - local variables in main)

### std::shared_ptr for Shared Ownership

**When to use**: Multiple owners need to share ownership of an object, particularly for data shared across asynchronous operations.

**Why**: Reference counting ensures object lifetime extends until all owners release it.

**Examples**:
```cpp
// Bar data shared between cache, API calls, and UI
std::shared_ptr<QVector<Bar>> barsPtr;

// Function returning shared data
QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> getBars();
```

**Construction**:
```cpp
auto bars = std::make_shared<QVector<Bar>>();
```

**Files using std::shared_ptr**:
- `Src/Core/Cache/BarCache/BarCache.h/cpp` (Bar vectors)
- `Src/FrontEnd/GUI/StockPriceChart/StockPriceChart.h/cpp` (Bar data)
- `Src/Core/Cache/DatabaseThread.h/cpp` (database operations)

### std::weak_ptr for Non-Owning References

**When to use**: Need to observe a `std::shared_ptr` without extending its lifetime.

**Why**: Prevents circular reference issues and allows checking if object still exists.

**Usage**:
```cpp
std::weak_ptr<T> weakPtr = sharedPtr;
if (auto sharedPtr = weakPtr.lock()) {
    // Object still exists, use sharedPtr
}
```

## Conversion Summary

### Files Modified in Smart Pointer Conversion

1. **.github/copilot-instructions.md**
   - Added comprehensive smart pointer guidelines
   - Added composition over pointers preference

2. **Src/FrontEnd/GUI/GUIFrontend.h/cpp**
   - Converted `Ui::GUIFrontend* ui` to `std::unique_ptr<Ui::GUIFrontend> ui`
   - Removed manual `delete ui` from destructor
   - **Why unique_ptr**: Ui::GUIFrontend is not a QObject, always needed, but can't be composed

3. **Src/FrontEnd/GUI/Tabs/RecorderTab.h/cpp**
   - Converted `LiveStreamDB* m_liveBarsDB` to `std::unique_ptr<LiveStreamDB> m_liveBarsDB`
   - Converted `LiveStreamDB* m_liveMarketDepthQuoteDB` to `std::unique_ptr<LiveStreamDB> m_liveMarketDepthQuoteDB`
   - Changed cleanup from `delete` to `.reset()`
   - **Why unique_ptr**: Optional lifetime - only exist when recording is active

4. **Src/Recorder/main.cpp**
   - Converted LiveStreamDB allocations to `std::unique_ptr`
   - Maintain raw pointers for signal handler (signal handlers cannot use smart pointers)
   - **Why unique_ptr**: Local variables in main() that persist for app lifetime

5. **Src/Core/MemoryMonitor.cpp**
   - Removed redundant `delete timer` (Qt manages it via parent-child)

6. **Src/Algo/MainAlgo.h/cpp**
   - Changed `PositionsReceiver* m_positionReceiver` and `OrdersReceiver* m_orderReceiver` to use Qt parent-child ownership
   - Pass `this` (MainAlgo) as parent to receivers, allowing Qt to manage their lifetime
   - Fixed `StockInstruments` to use Qt parent-child relationship (passes MainAlgo as parent)
   - **Why parent-child**: QObjects that persist once created, parent is available

### Decision Criteria Summary

The conversion followed these principles in order of preference:

1. **Composition first**: Use direct member objects when object lifetime matches container (e.g., StockInstruments members)
2. **Qt parent-child**: Use for QObjects with a parent available (e.g., MainAlgo receivers)
3. **std::unique_ptr**: Use for exclusive ownership when composition/parent-child don't apply (e.g., optional objects, non-QObjects)
4. **std::shared_ptr**: Use only when multiple owners truly need shared ownership (e.g., Bar vectors)
5. **QPointer**: Use for observing QObjects whose lifetime is uncertain (e.g., stream objects)

## When NOT to Use Smart Pointers

1. **Qt objects with parents** - Use raw pointers, Qt manages them
2. **Function parameters** - Use raw pointers or references for non-owning access
3. **Stack-allocated objects** - No pointers needed
4. **QStandardItem and similar Qt model items** - Model takes ownership
5. **Signal handlers in C programs** - Must use raw pointers

## Guidelines for New Code

1. **Default to smart pointers** for non-Qt objects without parents
2. **Always pass parent to Qt objects** when possible to leverage Qt's memory management
3. **Use QPointer** for Qt objects whose lifetime is uncertain
4. **Prefer std::unique_ptr** over shared_ptr unless sharing is truly needed
5. **Document ownership semantics** when the pattern is not obvious
6. **Check pointers** with `Q_CHECK_PTR()` after allocation

## Memory Leak Prevention

### Before Smart Pointers
```cpp
// Memory leak - never deleted
LiveStreamDB* m_db = new LiveStreamDB(...);
~MyClass() { } // m_db leaked!
```

### After Smart Pointers
```cpp
// Automatically cleaned up
std::unique_ptr<LiveStreamDB> m_db = std::make_unique<LiveStreamDB>(...);
~MyClass() { } // m_db automatically deleted
```

### Qt Parent-Child
```cpp
// Automatically cleaned up by Qt
StockInstruments* stock = new StockInstruments(symbol, this);
// When 'this' (MainAlgo) is deleted, Qt deletes 'stock'
```

## Common Patterns

### Creating and Storing
```cpp
m_object = std::make_unique<MyClass>(args);
```

### Passing to Functions
```cpp
someFunction(m_object.get()); // Pass raw pointer for access
```

### Qt Signal Connections
```cpp
connect(m_object.get(), &MyClass::signal, this, &MyClass::slot);
```

### Manual Cleanup
```cpp
m_object.reset(); // Explicitly delete and set to null
```

### Transferring Ownership
```cpp
auto obj = std::move(m_object); // m_object is now null, obj owns it
```

## References

- [C++ Core Guidelines for Smart Pointers](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-smartptrparam)
- [Qt Object Trees & Ownership](https://doc.qt.io/qt-6/objecttrees.html)
- [QPointer Documentation](https://doc.qt.io/qt-6/qpointer.html)
