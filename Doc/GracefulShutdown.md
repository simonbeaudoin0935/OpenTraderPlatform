# Graceful Application Shutdown

## Overview

The L2Trader application implements a graceful shutdown mechanism that ensures all threads are properly stopped and all resources are cleaned up before the application exits. This is important for:

1. **Memory leak detection with valgrind**: Proper cleanup allows valgrind to accurately detect memory leaks
2. **Resource cleanup**: Ensures database connections, network streams, and file handles are properly closed
3. **Data integrity**: Prevents data corruption by ensuring all pending operations complete

## Shutdown Sequence

### 1. User Initiates Shutdown

The user can initiate shutdown by:
- Pressing **Ctrl+Q** (default quit shortcut)
- Clicking the window close button (GUI mode)
- Pressing **'q'** key (TUI mode)

### 2. Frontend Calls MainApp::shutdown()

When the user triggers shutdown, the frontend calls:
```cpp
MainApp::getInstance()->shutdown();
```

### 3. MainApp::shutdown() Stops Services

The shutdown method performs these steps in order:

```cpp
void MainApp::shutdown()
{
    // 1. Stop memory monitoring
    memoryMonitor.stopMonitoring();
    
    // 2. Stop MainAlgo balance polling
    if (mainAlgo)
    {
        mainAlgo->stopBalancePolling();
    }
    
    // 3. Request Qt event loop to quit
    QCoreApplication::quit();
}
```

### 4. Qt Event Loop Exits

`QCoreApplication::exec()` returns the exit code, and control returns to `main()`.

### 5. Cleanup Singletons

The `main()` function calls the cleanup method:

```cpp
MainApp::cleanupSingletons();
```

This destroys all singletons in the correct order:

1. **MainApp** - Deletes the frontend (GUIFrontend or TUIFrontend)
2. **MainAlgo** - Stops its worker thread
3. **TSClient** - Stops its worker thread and closes network connections
4. **DatabaseThread** - Closes all database connections and stops its worker thread

### 6. Thread Cleanup Pattern

Each singleton with a worker thread follows this cleanup pattern in its destructor:

```cpp
// Request thread to stop
m_thread.quit();

// Wait for thread to finish (with 5-second timeout)
if (!m_thread.wait(5000))
{
    qWarning() << "Thread did not finish within timeout, terminating";
    m_thread.terminate();
    m_thread.wait();
}
```

This ensures:
- The thread is given time to finish gracefully
- If the thread is stuck, it's forcibly terminated after 5 seconds
- We always wait for the thread to actually stop before continuing

## Singleton Cleanup

### Why Explicit Cleanup?

The singletons are implemented using the lazy initialization pattern:

```cpp
static MySingleton* getInstance()
{
    if (m_instance == nullptr)
    {
        m_instance = new MySingleton();
    }
    return m_instance;
}
```

These are never automatically deleted. For proper cleanup with valgrind, we need to explicitly delete them.

### destroyInstance() Method

Each singleton provides a `destroyInstance()` method:

```cpp
static void destroyInstance()
{
    if (m_instance != nullptr)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}
```

This allows controlled cleanup in the correct order.

## Components Involved

### MainApp (Core/MainApp.h/cpp)
- Singleton that owns the main application components
- Provides `shutdown()` method for graceful shutdown
- Provides `cleanupSingletons()` static method to clean up all singletons

### TSClient (Clients/TSClient/TSClient.h/cpp)
- Singleton with worker thread for TradeStation API communication
- Destructor stops thread and waits for completion

### MainAlgo (Algo/MainAlgo.h/cpp)
- Singleton with worker thread for trading algorithm logic
- Destructor stops balance polling timer and thread

### DatabaseThread (Core/Cache/DatabaseThread.h/cpp)
- Singleton with worker thread for database operations
- Destructor closes all database connections before stopping thread

### GUIFrontend (FrontEnd/GUI/GUIFrontend.h/cpp)
- Connects Ctrl+Q shortcut to `MainApp::getInstance()->shutdown()`

### TUIFrontend (FrontEnd/TUI/TUIFrontend.h/cpp)
- Calls `MainApp::getInstance()->shutdown()` when user presses 'q'

## Testing with Valgrind

To test for memory leaks with valgrind:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./build/GUI/Src/L2Trader
```

The graceful shutdown ensures that:
- All heap-allocated memory is freed
- All threads are properly joined
- All file descriptors are closed
- All database connections are closed

Without graceful shutdown, valgrind would report many false positives as "still reachable" memory.

## Future Improvements

Possible enhancements to the shutdown mechanism:

1. **Timeout handling**: Add a global shutdown timeout to prevent hanging
2. **Progress feedback**: Show shutdown progress in the UI
3. **Pending operations**: Wait for critical operations to complete before shutdown
4. **Save state**: Persist application state (open symbols, window positions, etc.)
5. **Confirmation dialog**: Ask user to confirm if there are open orders/positions
