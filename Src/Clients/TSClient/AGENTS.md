# TSClient - TradeStation API Client - Agent Instructions

TSClient is the core singleton class for all TradeStation API communication in L2Trader. It handles authentication, REST API requests, and WebSocket streaming connections.

## Overview

**Location**: `Src/Clients/TSClient/`
**Pattern**: Meyer's Singleton running in dedicated QThread
**Primary Responsibility**: All communication with TradeStation API (REST + WebSocket)

## Key Characteristics

- **Thread-Safe**: Runs in dedicated worker thread, all API calls queued via signals
- **Asynchronous**: All requests are non-blocking with callback-based responses
- **OAuth 2.0**: Handles authentication, token storage, and automatic refresh
- **Rate Limited**: Tracks and respects API rate limits
- **Stream Management**: Handles multiple concurrent WebSocket streams

## File Structure

### Core Files
- **TSClient.h/cpp**: Main singleton class, initialization, threading
- **TSClientAsyncRequests.cpp**: Async REST API request implementations
- **TSClientRefreshToken.cpp**: OAuth token refresh logic
- **TSClientStreams.cpp**: WebSocket stream management

### Subdirectories

**Auth/** - Authentication and OAuth
- OAuth 2.0 Authorization Code Flow
- Secure token storage via QKeychain
- Automatic token refresh (20-minute expiry, refresh 5 seconds early)
- GUI and TUI authentication flows
- See `Doc/AUTHENTICATION.md` for detailed flow diagrams

**MarketData/** - Market data API endpoints
- Bar data (historical and intraday)
- Quote snapshots
- Option chains
- Symbol search and validation

**Brokerage/** - Account and position endpoints
- Account information
- Balances (cash, buying power, margin)
- Position tracking

**OrderExecution/** - Order management
- Place orders (market, limit, stop, stop-limit)
- Cancel orders
- Order status tracking
- Replace/modify orders

**Stream/** - WebSocket streaming
- StreamBars: Live bar updates (1min, 5min, etc.) - **unlimited concurrent streams**
- StreamQuotes: Real-time quote updates
- StreamMarketDepthQuotes: Level 2 market depth - **maximum 10 concurrent streams**
- StreamOrders: Order status updates - **singleton (max 1)**
- StreamPositions: Position updates - **singleton (max 1)**

**Stream Concurrency Limits**:
- **StreamBars**: No limit - can open as many as needed
- **StreamMarketDepthQuote**: Hard limit of 10 concurrent streams (API restriction)
  - Opening 11+ streams triggers FIFO queue with QFuture-based async fulfillment
  - 1000ms delay before processing queue (TCP close propagation)
  - See "Stream Management" section below for details
- **StreamPositions**: Singleton - exactly 1 stream allowed (asserted)
- **StreamOrders**: Singleton - exactly 1 stream allowed (asserted)

## Architecture

### Threading Model

```
Main Thread (GUI)
    │
    ↓
TSClient Object (lives in Main Thread)
    │ moveToThread()
    ↓
TSClient Worker Thread
    │
    ├─→ QNetworkAccessManager (REST API)
    ├─→ QWebSocket connections (Streams)
    └─→ QTimer (Token refresh)
```

**Lifecycle**:
```cpp
// Constructor (main thread)
TSClient::TSClient() {
    moveToThread(&m_thread);  // Move to worker thread
    m_thread.start();         // Start dedicated thread
}

// Destructor (main thread)
TSClient::~TSClient() {
    m_thread.quit();
    if (!m_thread.wait(5000)) {
        qWarning() << "Thread did not finish, terminating";
        m_thread.terminate();
        m_thread.wait();
    }
}
```

### Replay Mode

TSClient supports a `Mode` enum that determines how requests are handled:

```cpp
enum class Mode {
    Live,   // Real API requests to TradeStation
    Replay  // Emulated locally via OrderEmulator
};
```

**Mode Switching**:
```cpp
void TSClient::setMode(Mode mode) {
    m_mode = mode;

    if (mode == Mode::Replay) {
        // Create emulator and mock network manager
        m_orderEmulator = new OrderEmulator(this);
        m_mockNetworkManager = new MockNetworkAccessManager(m_orderEmulator, this);
    } else {
        // Clean up replay mode objects
        delete m_orderEmulator;
        delete m_mockNetworkManager;
    }
}
```

**Request Routing**:

| Request | Live Mode | Replay Mode |
|---------|-----------|-------------|
| `getAccounts()` | Real API | MockNetworkAccessManager returns `SIM123456` |
| `getBalances()` | Real API | MockNetworkAccessManager returns emulator balance |
| `placeOrder()` | Real API | MockNetworkAccessManager routes to OrderEmulator |
| `cancelOrder()` | Real API | MockNetworkAccessManager routes to OrderEmulator |
| `openStreamBars()` | Real WebSocket | MockNetworkReply receives ReplayEngine data |
| `openStreamOrders()` | Real WebSocket | MockNetworkReply receives OrderEmulator signals |
| `openStreamPositions()` | Real WebSocket | MockNetworkReply receives OrderEmulator signals |

**Mock Infrastructure**:
- `MockNetworkReply`: Fake QNetworkReply that receives injected data
- `MockNetworkAccessManager`: Intercepts HTTP requests, routes to OrderEmulator

See `Src/Core/Replay/OrderEmulator/AGENTS.md` for order emulation details.

### Request Tracking System

Every async request is tracked with:
- **Request ID**: UUID for tracking
- **Callback**: Lambda function for success/failure
- **Timeout**: Configurable timeout with auto-cleanup
- **Type**: Request type enum for categorization

```cpp
struct AsyncRequest {
    QString requestId;
    std::function<void(bool success, const QJsonDocument& response)> callback;
    QTimer* timeoutTimer;
    RequestType type;
    QDateTime timestamp;
};
```

Tracking prevents:
- Memory leaks from abandoned requests
- Callback invocation after object destruction
- Duplicate request processing

### Authentication Flow

1. **Load credentials** from SecureStorage (QKeychain)
2. **Check token validity**:
   - Invalid/Missing → Full OAuth flow
   - Expired → Immediate refresh
   - Valid → Schedule refresh before expiration
3. **OAuth flow** (if needed):
   - Start local HTTP server (port 8080-8089)
   - Generate random state (CSRF protection)
   - Open browser to TradeStation authorization
   - Receive callback with authorization code
   - Exchange code for access_token + refresh_token
   - Store tokens securely
4. **Automatic refresh**:
   - Scheduled 1195 seconds after token receipt (20min - 5sec buffer)
   - Preserves original refresh_token (not returned by API)
   - Emits `authStateChanged(bool)` signal

**Important**: TradeStation does NOT return new refresh_token during refresh. Must preserve original.

### REST API Pattern

All REST requests follow this pattern:

```cpp
void TSClient::someAPICall(QString param,
                           std::function<void(bool, QJsonDocument)> callback) {
    // Build request
    QNetworkRequest request;
    request.setUrl(QUrl(TSClientEndpoints::SOME_ENDPOINT));
    request.setRawHeader("Authorization", "Bearer " + accessToken);

    // Track request
    QString requestId = QUuid::createUuid().toString();
    AsyncRequest tracking = {requestId, callback, createTimeout(), TYPE, now()};
    m_asyncRequests.insert(requestId, tracking);

    // Send request
    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("requestId", requestId);

    // Connect completion handler
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAsyncReplyFinished(reply);
    });
}
```

### WebSocket Stream Pattern

Streams follow a managed lifecycle:

```cpp
class StreamBars : public QObject {
    // Created in TSClient thread
    // Manages WebSocket connection
    // Emits signals for data

signals:
    void barReceived(const Bar& bar);
    void errorOccurred(const QString& error);
    void streamClosed();

public slots:
    void start();        // Begin streaming
    void stop();         // Gracefully close
};
```

**Stream Management**:
- TSClient maintains `QMap<QString, QPointer<Stream>>` for tracking
- QPointer auto-nulls when stream deleted (important for cleanup)
- Each stream type has dedicated class with per-type counters
- Automatic reconnection on transient failures
- Backpressure handling for high-frequency data

**Per-Stream-Type Counters**:
Each stream type maintains its own static counter for tracking:
- `StreamBars::s_numberOfBarsStreams` (size_t, no limit)
- `StreamMarketDepthQuote::s_numberOfMarketDepthStreams` (std::atomic<size_t>, max 10)
- `StreamPositions::s_numberOfPositionStreams` (size_t, max 1)
- `StreamOrders::s_numberOfOrderStreams` (size_t, max 1)

**Market Depth Stream Queue**:
The TradeStation API enforces a hard limit of 10 concurrent market depth streams. When this limit is reached:
1. Requests are queued in a FIFO `std::deque<PendingMarketDepthRequest>`
2. Each queued request includes a `QPromise<QPointer<StreamMarketDepthQuote>>`
3. When a stream closes, the queue is processed after 1000ms delay (allows TCP FIN to propagate)
4. Callers receive `std::expected<QPointer<Stream>, QFuture<QPointer<Stream>>>`:
   - **Value channel**: Stream opened immediately (< 10 active)
   - **Error channel**: QFuture that completes when queued stream opens (≥ 10 active)

**Important**: Callers MUST handle QFuture properly - do not cancel futures as the stream will still be created and needs cleanup.

## Key Signals

```cpp
signals:
    // Authentication
    void authStateChanged(bool authenticated, QString reason);

    // Data usage tracking
    void totalDataReceivedBytesIncreased(qsizetype bytesIncrease);

    // Stream tracking (separate counts for bars and market depth)
    void streamCountsChanged(size_t barsCount, size_t marketDepthCount);

    // Error reporting
    void errorOccurred(const QString& error);
```

## Common Usage Patterns

### Making Async API Call

```cpp
// From any thread (usually MainAlgo)
TSClient& client = TSClient::getInstance();

client.getBars(symbol, interval, startDate, endDate,
    [this](bool success, const QJsonDocument& response) {
        if (success) {
            // Parse response
            QVector<Bar> bars = parseBarsFromJson(response);
            // Process bars
        } else {
            qCWarning() << "Failed to get bars";
        }
    }
);
```

### Opening a Stream

**Bar Stream (unlimited)**:
```cpp
StreamBars* stream = TSClient::getInstance().openStreamBars(symbol, interval);

// Connect signals
connect(stream, &StreamBars::barReceived, this, [this](const Bar& bar) {
    // Handle new bar
    processBar(bar);
});

connect(stream, &StreamBars::errorOccurred, this, [](const QString& error) {
    qCWarning() << "Stream error:" << error;
});

// Start streaming
stream->start();

// Later: close stream
stream->stop();  // Will emit streamClosed() signal
```

**Market Depth Stream (max 10 concurrent)**:
```cpp
auto result = TSClient::getInstance().openStreamMarketDepthQuote(symbol, depth);

if (result.has_value()) {
    // Stream opened immediately (< 10 active)
    QPointer<StreamMarketDepthQuote> stream = result.value();
    connectStreamSignals(stream);
    stream->start();
} else {
    // Stream queued (≥ 10 active) - received QFuture
    QFuture<QPointer<StreamMarketDepthQuote>> future = result.error();

    // Use .then() continuation for clean async handling (Qt6)
    future.then(this, [this](QPointer<StreamMarketDepthQuote> stream) {
        if (!stream.isNull()) {
            connectStreamSignals(stream);
            stream->start();
        }
    });

    qInfo() << "Market depth stream queued for" << symbol;
}
```

**Checking Stream Availability**:
```cpp
if (StreamMarketDepthQuote::canOpenStream()) {
    // Can open immediately
} else {
    // Will be queued (10 streams already open)
    size_t count = StreamMarketDepthQuote::getNumberOfMarketDepthStreams();
    qDebug() << "Market depth streams at limit:" << count;
}
```

### Checking Authentication

```cpp
if (TSClient::getInstance().isAuthenticated()) {
    // Make authenticated API calls
} else {
    // Wait for authStateChanged signal
}

// React to auth changes
connect(&TSClient::getInstance(), &TSClient::authStateChanged,
        this, [this](bool authenticated) {
            if (authenticated) {
                // Start operations that require auth
            } else {
                // Stop operations, show login
            }
        });
```

## Error Handling

### Request Timeouts

Default timeout: 30 seconds per request
Timeout handler automatically:
1. Removes request from tracking
2. Invokes callback with `success = false`
3. Logs warning with request type
4. Cleans up QNetworkReply

### Network Errors

```cpp
if (reply->error() != QNetworkReply::NoError) {
    QString error = reply->errorString();
    qCWarning() << "Network error:" << error;
    callback(false, QJsonDocument());
}
```

### Stream Errors

Streams emit `errorOccurred(QString)` for:
- Connection failures
- Authentication errors
- Protocol errors
- Server-side errors

Consumers should reconnect or notify user.

## API Endpoints

All endpoints defined in `Src/Misc/CONSTANTS.h`:

```cpp
namespace TSClientEndpoints {
    constexpr const char* BASE_URL = "https://api.tradestation.com/v3";
    constexpr const char* GET_BARS = "/marketdata/barcharts";
    constexpr const char* GET_QUOTES = "/marketdata/quotes";
    constexpr const char* GET_ACCOUNTS = "/brokerage/accounts";
    constexpr const char* POST_ORDER = "/orderexecution/orders";
    // ... many more
}
```

## Security Considerations

1. **Never log tokens** - Use qCDebug with separate category that can be disabled
2. **Secure storage** - Tokens stored via QKeychain (OS-level encryption)
3. **CSRF protection** - State parameter validated in OAuth flow
4. **TLS only** - All communication over HTTPS/WSS
5. **Token refresh** - Automatic refresh minimizes token exposure time
6. **Obfuscated fallback** - If QKeychain unavailable, use XOR obfuscation (development only)

## Rate Limiting

TradeStation API has rate limits:
- Per-endpoint limits
- Global account limits

TSClient tracks:
- Request count per time window
- Last request timestamp per endpoint
- Automatic throttling (future feature)

## Testing Considerations

### Mock TSClient

For testing components that depend on TSClient:
1. Create test double class implementing same interface
2. Inject via dependency injection (not singleton)
3. Control responses and timing
4. Verify request parameters

### Integration Testing

With real API:
1. Use TradeStation sandbox environment
2. Store test credentials separately
3. Clean up orders/positions after tests
4. Respect rate limits
5. Mock streams for high-frequency testing

## Performance Notes

- Request tracking uses QHash (O(1) lookup)
- Token refresh scheduled once, not polled
- Network operations fully async (non-blocking)
- Stream parsing optimized for high-frequency data
- Memory pooling for frequent allocations (future)

## Common Pitfalls

1. **Don't call from wrong thread directly** - Use signal/slot or QMetaObject::invokeMethod
2. **Don't store raw pointers to streams** - Use QPointer (auto-nulling)
3. **Don't ignore callback bool success** - Always check before using response
4. **Don't assume immediate auth** - Connect to authStateChanged signal
5. **Don't make sequential calls in loop** - Use batch endpoints when available
6. **Don't cancel QFuture from market depth queue** - Stream still created, needs cleanup
7. **Don't open 11+ market depth streams** - Check `canOpenStream()` or handle QFuture
8. **Don't create multiple Positions/Orders streams** - Singletons, will assert

## Related Documentation

- **Doc/AUTHENTICATION.md**: Complete OAuth flow diagrams and security details
- **Doc/ARCHITECTURE.md**: TSClient threading and lifecycle in system context
- **Src/Misc/CONSTANTS.h**: All API endpoints and constants
- **Qt Network Module Docs**: QNetworkAccessManager, QNetworkReply, QWebSocket

## Debugging Tips

### Enable TSClient logging categories:

```cpp
QLoggingCategory::setFilterRules("TSClient*=true");
```

Categories:
- `TSClient.auth` - Authentication flow
- `TSClient.request` - REST API requests
- `TSClient.stream` - WebSocket streams
- `TSClient.token` - Token management

### Monitor request tracking:

```cpp
qCDebug() << "Active requests:" << m_asyncRequests.size();
for (const auto& req : m_asyncRequests) {
    qCDebug() << req.requestId << req.type << req.timestamp;
}
```

### Check thread affinity:

```cpp
Q_ASSERT(QThread::currentThread() == &m_thread);
```

This ensures methods run on correct thread.
