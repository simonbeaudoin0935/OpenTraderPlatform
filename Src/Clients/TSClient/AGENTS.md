# TSClient - TradeStation Brokerage Client - Agent Instructions

> **TSClient is the live/sim TradeStation client** for brokerage plus market data streams (bars, Level 2, quotes).

TSClient is the singleton class for TradeStation API communication in OpenTraderPlatform. It handles authentication, account/order REST requests, order/position WebSocket streams, and live/sim market-data endpoints/streams.

## Overview

**Location**: `Src/Clients/TSClient/`
**Pattern**: Meyer's Singleton running in dedicated QThread
**Primary Responsibility**: Authentication, account management, order execution, order/position streaming, and live/sim market-data streaming

## Key Characteristics

- **Thread-Safe**: Runs in dedicated worker thread, all API calls queued via signals
- **Asynchronous**: All requests are non-blocking with callback-based responses
- **OAuth 2.0**: Handles authentication, token storage, and automatic refresh
- **Rate Limited**: Tracks and respects API rate limits
- **Dual Role in Live/Sim**: Brokerage + TradeStation market data (bars, level2, quote)

## File Structure

### Core Files
- **TSClient.h/cpp**: Main singleton class, initialization, threading
- **TSClientAsyncRequests.cpp**: REST requests (accounts, balances, orders, historical bars, quote snapshots)
- **TSClientRefreshToken.cpp**: OAuth token refresh logic
- **TSClientStreams.cpp**: StreamOrders + StreamPositions + market-data stream management

### Subdirectories

**Auth/** - Authentication and OAuth
- OAuth 2.0 Authorization Code Flow
- Secure token storage via QKeychain
- Automatic token refresh (20-minute expiry, refresh 5 seconds early)
- See `Doc/AUTHENTICATION.md` for detailed flow diagrams

**Brokerage/** - Account and position endpoints
- Account information
- Balances (cash, buying power, margin)
- Position tracking
- **Order class** (`Brokerage/GetOrders/Order.h`):
  - Data structure representing orders from API
  - **Centralized status descriptions**: `Order::getStatusDescriptionForStatus()` provides single source of truth mapping Order::Status enum to TradeStation API status strings (e.g., `ACK` → "Received", `FLL` → "Filled")
  - Used by OrderEmulator to ensure replay mode matches real API behavior

**OrderExecution/** - Order management
- Place orders (market, limit, stop, stop-limit)
- Cancel orders
- Order status tracking
- Replace/modify orders
- Managed bracket native legs use OCO fields in `PlaceOrderRequest::toJson()`:
  - `OCOGroupID` (group name)
  - `OCORoute` (group type/route hint)

**Stream/** - WebSocket streaming infrastructure
- StreamOrders: Order status updates - **singleton (max 1)**
- StreamPositions: Position updates - **singleton (max 1)**

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
    ├─→ QWebSocket connections (StreamOrders, StreamPositions)
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

TSClient supports a `Mode` enum that determines how brokerage requests are handled:

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

Brokerage streams (StreamOrders, StreamPositions) follow a managed lifecycle:

```cpp
class StreamOrders : public QObject {
    // Created in TSClient thread
    // Manages WebSocket connection
    // Emits signals for order updates

signals:
    void orderReceived(const Order& order);
    void errorOccurred(const QString& error);
    void streamClosed();

public slots:
    void start();        // Begin streaming
    void stop();         // Gracefully close
};
```

**Stream Management**:
- TSClient maintains QPointer references to the singleton streams
- QPointer auto-nulls when stream deleted (important for cleanup)
- StreamOrders and StreamPositions are singletons (max 1 each, asserted)

## Key Signals

```cpp
signals:
    // Authentication
    void authStateChanged(bool authenticated, QString reason);

    // Data usage tracking
    void totalDataReceivedBytesIncreased(qsizetype bytesIncrease);

    // Error reporting
    void errorOccurred(const QString& error);
```

## Common Usage Patterns

### Making Brokerage API Call

```cpp
// From any thread (usually MainAlgo)
TSClient& client = TSClient::getInstance();

client.getBalances(accountId,
    [this](bool success, const QJsonDocument& response) {
        if (success) {
            // Parse balance response
        } else {
            qCWarning() << "Failed to get balances";
        }
    }
);
```

### Opening Order/Position Streams

```cpp
// Singleton streams - only one of each allowed
StreamOrders* orderStream = TSClient::getInstance().openStreamOrders();
StreamPositions* posStream = TSClient::getInstance().openStreamPositions();

connect(orderStream, &StreamOrders::orderReceived, this, [this](const Order& order) {
    processOrderUpdate(order);
});

connect(posStream, &StreamPositions::positionReceived, this, [this](const Position& pos) {
    processPositionUpdate(pos);
});
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

### Request Timeouts and Stalled HTTP/2 Connections

All TradeStation traffic from one `AuthenticatedNetworkAccessManager` is multiplexed over a single
HTTP/2 connection. After some server `GoAway` frames, that connection can keep serving established
streams while never answering new requests (every reconnect/REST call hangs). The manager watches each
real reply (`TSClientNetworkConstants` in `CONSTANTS.h`):

- **Streams** (requests built with `buildStreamRequest()`): no headers/bytes within
  `STREAM_FIRST_RESPONSE_TIMEOUT_MS` (8 s, below the 10 s `Stream` heartbeat) → `connectionStalled()`.
  The stream itself is not aborted; its heartbeat timeout handles that and the owner retries.
- **REST** (requests built with `buildNetworkRequest()`): no progress for `REST_TRANSFER_TIMEOUT_MS`
  (20 s) → the reply is aborted (`OperationCanceledError`), and `connectionStalled()` is emitted if no
  response was ever received.

`TSClient::onNetworkConnectionStalled()` replaces the active manager with a fresh one (fresh connection)
and `retire()`s the stalled one, which deletes itself once its last reply is destroyed. Stalls reported
by already-retired managers are ignored. Always use `buildStreamRequest()` for long-lived streams.

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
    constexpr const char* GET_ACCOUNTS = "/brokerage/accounts";
    constexpr const char* GET_BALANCES = "/brokerage/accounts/{account_id}/balances";
    constexpr const char* GET_ORDERS = "/brokerage/accounts/{account_id}/orders";
    constexpr const char* POST_ORDER = "/orderexecution/orders";
    // ... other brokerage endpoints
}
```

## Security Considerations

1. **Never log tokens** - Use qCDebug with separate category that can be disabled
2. **Secure storage** - Tokens stored via QKeychain (OS-level encryption)
3. **CSRF protection** - State parameter validated in OAuth flow
4. **TLS only** - All communication over HTTPS/WSS
5. **Token refresh** - Automatic refresh minimizes token exposure time
6. **No insecure fallback** - Qt6Keychain is mandatory; keyring failures are reported, never redirected to file storage

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

## Performance Notes

- Request tracking uses QHash (O(1) lookup)
- Token refresh scheduled once, not polled
- Network operations fully async (non-blocking)

## Common Pitfalls

1. **Don't call from wrong thread directly** - Use signal/slot or QMetaObject::invokeMethod
2. **Don't store raw pointers to streams** - Use QPointer (auto-nulling)
3. **Don't ignore callback bool success** - Always check before using response
4. **Don't assume immediate auth** - Connect to authStateChanged signal
5. **Don't make sequential calls in loop** - Use batch endpoints when available
6. **Don't create multiple Positions/Orders streams** - Singletons, will assert

## Related Documentation

- **Doc/AUTHENTICATION.md**: Complete OAuth flow diagrams and security details
- **Doc/ARCHITECTURE.md**: TSClient threading and lifecycle in system context
- **Src/Clients/DBClient/**: Market data client (Databento) — all market data goes through DBClient
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
- `TSClient.stream` - WebSocket streams (orders/positions)
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
