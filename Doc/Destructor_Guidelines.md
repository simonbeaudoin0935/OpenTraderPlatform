# Destructor and Graceful Teardown Guidelines

**Version:** 1.0  
**Date:** 2026-02-07  
**Purpose:** Comprehensive guidelines for writing correct destructors and ensuring graceful application teardown

## Table of Contents

1. [Critical Threading Rules](#critical-threading-rules)
2. [delete vs deleteLater() Decision Guide](#delete-vs-deletelater-decision-guide)
3. [Destructor Best Practices](#destructor-best-practices)
4. [Thread Affinity Assertions](#thread-affinity-assertions)
5. [Common Pitfalls](#common-pitfalls)
6. [Singleton Destruction Order](#singleton-destruction-order)
7. [Testing Checklist](#testing-checklist)

---

## Critical Threading Rules

### Rule 1: Stop threads BEFORE destroying thread-owned objects

**WHY:** Thread event loops may still be processing events that reference objects being destroyed.

```cpp
// ✓ CORRECT
~MyClass() {
    // 1. Stop thread FIRST
    m_thread.quit();
    if (!m_thread.wait(5000)) {
        CRITICAL << "Thread timeout, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
    
    // 2. THEN destroy thread-owned objects
    m_socketNotifier.reset();
    m_timer->stop();
    delete m_threadOwnedResource;
}

// ✗ WRONG - Objects destroyed while thread running
~MyClass() {
    m_socketNotifier.reset();  // ❌ Thread still processing events!
    m_thread.quit();           // Too late
}
```

**Real Bug:** MainAlgo segfaulted because `m_crashNotifier` was destroyed before thread stopped.

---

### Rule 2: Delete child QObjects on their thread

**WHY:** Qt objects must be destroyed on the thread where they were created.

```cpp
// ✓ CORRECT - Delete children on their thread
~MyWorkerClass() {
    // 1. Schedule deletion on correct thread
    QMetaObject::invokeMethod(
        this,
        [this]() {
            for (QObject* child : children()) {
                child->deleteLater();
            }
        },
        Qt::BlockingQueuedConnection);
    
    // 2. Process deleteLater queue
    QMetaObject::invokeMethod(
        this,
        []() {
            QCoreApplication::processEvents();
        },
        Qt::BlockingQueuedConnection);
    
    // 3. Now safe to stop thread
    m_thread.quit();
    m_thread.wait(5000);
}

// ✗ WRONG - Deletes from wrong thread
~MyWorkerClass() {
    for (QObject* child : children()) {
        delete child;  // ❌ Called from main thread, child on worker thread!
    }
}
```

**Real Bug:** TSClient crashed with thread affinity assertion when Stream objects deleted from main thread.

---

### Rule 3: Control deletion order for dependent objects

**WHY:** Qt parent-child auto-deletion doesn't guarantee order. Dependent objects may access already-deleted resources.

```cpp
// ✓ CORRECT - Explicit order
~TSClient() {
    // 1. Delete Streams FIRST (while QNetworkAccessManager valid)
    for (QObject* child : children()) {
        if (child == m_networkManager)
            continue;  // Skip manager
        child->deleteLater();
    }
    processEvents();
    
    // 2. THEN delete QNetworkAccessManager
    m_networkManager->deleteLater();
    processEvents();
    
    // 3. Stop thread
    m_thread.quit();
    m_thread.wait(5000);
}

// ✗ WRONG - All children deleted together
~TSClient() {
    for (QObject* child : children()) {
        child->deleteLater();  // ❌ Order not guaranteed!
    }
}
```

**Real Bug:** QNetworkAccessManager deleted before Streams, causing crash in `Stream::~Stream()` when aborting network reply.

---

### Rule 4: Use deleteLater() for QObjects with signals/events

**WHY:** Direct `delete` can crash if signals fire or events process during destruction.

```cpp
// ✓ CORRECT
void cleanup() {
    m_stream->deleteLater();        // Has timers, signals, events
    m_networkReply->deleteLater();  // Qt requirement
    m_timerObject->deleteLater();   // May have pending timeout
}

// ✗ WRONG
void cleanup() {
    delete m_stream;        // ❌ Timer may fire during destruction
    delete m_networkReply;  // ❌ Violates Qt documentation
}
```

**Real Bug:** Direct `delete` on Stream caused `QNetworkAccessManager::finished()` to crash.

---

## delete vs deleteLater() Decision Guide

### Use `deleteLater()` when:

✅ **QNetworkReply** - Qt requirement (ALWAYS)  
✅ **Signal connections exist** - Pending signals may fire  
✅ **Pending events in queue** - Event loop may process them  
✅ **QTimer or QSocketNotifier** - Timeouts/notifications may be pending  
✅ **Called from signal/slot handler** - Currently in event processing  
✅ **Complex object graph** - Cross-dependencies with other QObjects  
✅ **Child QObjects that emit signals** - Children may signal during destruction  

### Use direct `delete` when:

✅ **Thread stopped** - Event loop exited, no more events  
✅ **No signal connections** - Object never connected to signals  
✅ **No Qt features** - Simple C++ object, not QObject-derived  
✅ **Stack-allocated** - Never use delete (automatic cleanup)  
✅ **Smart pointer managed** - Let smart pointer handle it  

### Examples:

```cpp
// QNetworkReply - ALWAYS deleteLater()
void onFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();  // ✓ Correct
}

// Stream with timers/signals - Use deleteLater()
void closeStream() {
    m_stream->deleteLater();  // ✓ Correct
}

// After thread stopped - Direct delete safe for simple objects
~TSClient() {
    m_thread.quit();
    m_thread.wait();
    // Thread stopped, safe for non-Qt objects
    delete m_simpleResource;  // ✓ OK if no Qt features
}

// Smart pointer - Let it handle cleanup
~MyClass() {
    // m_uniquePtr automatically deleted
    // m_sharedPtr ref count decremented
}  // ✓ Correct
```

---

## Destructor Best Practices

### 1. Always log destruction

```cpp
~MyClass() {
    DEBUG << "MyClass destructor - cleaning up";
    
    // ... cleanup code ...
    
    DEBUG << "MyClass destroyed successfully";
}
```

**WHY:** Helps debug shutdown issues and verify destruction order.

---

### 2. Document cleanup order

```cpp
~MyClass() {
    DEBUG << "MyClass destructor";
    
    // 1. Stop active operations
    m_timer->stop();
    m_activeOperation.cancel();
    
    // 2. Stop threads
    m_thread.quit();
    m_thread.wait(5000);
    
    // 3. Delete thread-owned objects
    m_threadOwnedObject.reset();
    
    // 4. Cleanup in reverse order of initialization
    m_networkResources.clear();
    m_fileHandles.close();
    
    DEBUG << "Destroyed";
}
```

**WHY:** Makes cleanup order explicit and maintainable.

---

### 3. Handle thread termination

```cpp
m_thread.quit();
if (!m_thread.wait(5000)) {
    CRITICAL << "Thread did not finish within 5s, terminating";
    m_thread.terminate();
    m_thread.wait();  // Always wait after terminate
}
```

**WHY:** Prevents app hang if thread doesn't exit gracefully.

---

### 4. Use composition over pointers

```cpp
// ✓ CORRECT - Automatic cleanup
class MyClass {
    QTimer m_timer;        // Stack-allocated
    BarCache m_cache;      // Composition
    std::unique_ptr<Resource> m_resource;  // Smart pointer
};

// ✗ AVOID - Manual cleanup required
class MyClass {
    QTimer* m_timer;       // Raw pointer
    BarCache* m_cache;     // Manual delete
    Resource* m_resource;  // Manual delete
};
```

**WHY:** Reduces chance of leaks and simplifies destructors.

---

## Thread Affinity Assertions

### Why Add Them?

Thread affinity assertions catch cross-thread deletion bugs **at development time** instead of in production.

### Pattern:

```cpp
~MyThreadedClass() {
    // Assert destructor called from expected thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), 
                     QCoreApplication::instance()->thread());
    
    // Rest of cleanup...
}
```

### Classes That MUST Have Assertions:

1. **Thread-owner classes** - MainAlgo, TSClient, DatabaseThread
2. **moveToThread() classes** - Objects moved to worker threads
3. **Classes with thread-owned Qt objects** - QTimer, QSocketNotifier, etc.

### Example - Stream Class:

```cpp
Stream::~Stream() {
    DEBUG << "Stream destroyed";
    
    // Catch cross-thread deletion early
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());
    
    // Cleanup...
}
```

**Real Success:** This assertion caught the TSClient child deletion bug immediately!

---

## Common Pitfalls

### ❌ Pitfall 1: QSocketNotifier while thread running

**Symptom:** "QSocketNotifier: Socket notifiers cannot be enabled or disabled from another thread"  
**Cause:** QSocketNotifier destroyed from main thread while worker thread event loop running  
**Fix:** Stop thread BEFORE destroying QSocketNotifier

---

### ❌ Pitfall 2: Direct delete on Stream objects

**Symptom:** Segfault in `QNetworkAccessManager::finished()`  
**Cause:** Stream has pending network events/signals  
**Fix:** Use `deleteLater()` to handle pending events gracefully

---

### ❌ Pitfall 3: Cross-thread QObject deletion

**Symptom:** Assertion failure "QThread::currentThread() != this->thread()"  
**Cause:** QObject destroyed from different thread than created  
**Fix:** Use `QMetaObject::invokeMethod` with `BlockingQueuedConnection` to delete on correct thread

---

### ❌ Pitfall 4: Forgetting to process deleteLater()

**Symptom:** Objects not deleted, memory leaks  
**Cause:** `deleteLater()` queues deletion, but thread stopped before processing  
**Fix:** Call `QCoreApplication::processEvents()` after `deleteLater()` before stopping thread

---

### ❌ Pitfall 5: Wrong deletion order

**Symptom:** Crash accessing deleted object during another's destructor  
**Cause:** Dependent objects deleted in wrong order  
**Fix:** Explicitly control deletion order, dependencies first

---

## Singleton Destruction Order

Singletons must be destroyed in **dependency order** (dependents before dependencies):

```cpp
void MainApp::cleanupSingletons() {
    qInfo() << "Cleaning up singletons";
    
    // Prevent Stream promise resolution during shutdown
    Stream::setShuttingDown(true);
    
    // Dependency order (dependent → dependency):
    MainApp::destroyInstance();       // 1. Frontend (uses Algo)
    MainAlgo::destroyInstance();      // 2. Algo (uses Client)
    TSClient::destroyInstance();      // 3. Client (uses Database)
    DatabaseThread::destroyInstance(); // 4. Database (no dependencies)
    
    qInfo() << "All singletons cleaned up";
}
```

**WHY:** Prevents use-after-free when destructors call methods on other singletons.

---

## Testing Checklist

### Manual Testing

- [ ] Launch app → immediate Ctrl+Q → No segfault, clean exit
- [ ] Launch app → perform operations → Ctrl+Q → No segfault
- [ ] Check logs for all "Destroyed singleton instance" messages
- [ ] Verify no "Socket notifiers" or Qt warnings
- [ ] Test during active streaming
- [ ] Test during database operations
- [ ] Test during strategy execution

### Expected Clean Shutdown Log:

```
[HH:MM:SS] INFO : MainApp shutdown initiated - stopping all threads and cleaning up
[HH:MM:SS] INFO : Memory monitor stopped
[HH:MM:SS] INFO : MainAlgo balance polling stopped
[HH:MM:SS] INFO : Application event loop exited, cleaning up singletons
[HH:MM:SS] INFO : Cleaning up singletons
[HH:MM:SS] INFO : Destroying MainApp singleton instance
[HH:MM:SS] DEBG : MainAlgo destructor - stopping thread
[HH:MM:SS] DEBG : Destroyed singleton instance
[HH:MM:SS] DEBG : TSClient shutting down
[HH:MM:SS] DEBG : Destroyed singleton instance
[HH:MM:SS] INFO : All singletons cleaned up
```

### Automated Tests

- [ ] Unit test: Singleton destruction order
- [ ] Unit test: Thread cleanup doesn't timeout
- [ ] Unit test: No segfaults during shutdown
- [ ] Integration test: Shutdown during various operations
- [ ] Valgrind: No memory leaks
- [ ] CI: Graceful shutdown test on every PR

---

## Quick Reference

### Destructor Checklist for New Classes:

- [ ] Add DEBUG logging (start and end)
- [ ] Document cleanup order in comments
- [ ] Stop threads BEFORE destroying thread-owned objects
- [ ] Use `deleteLater()` for QObjects with signals/events
- [ ] Add thread affinity assertion if thread-owned
- [ ] Handle thread termination timeout
- [ ] Test with Ctrl+Q shutdown
- [ ] Verify no warnings in logs

### When in Doubt:

1. **Is it a QObject?** → Probably use `deleteLater()`
2. **Is it on another thread?** → Delete on its thread
3. **Does it have dependencies?** → Control deletion order
4. **Can I use composition?** → Prefer over pointers

---

## References

- [Architecture Improvements](./Improvements/Architecture_Improvements.md) - Section 1.1.1
- [Copilot Instructions](../.github/copilot-instructions.md) - Graceful Teardown section
- [Qt Documentation](https://doc.qt.io) - QObject, QThread lifecycle

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-07 | Initial version based on Phase 1 fixes |
