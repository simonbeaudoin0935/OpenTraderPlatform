#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "Stream/MockNetworkReply.h"
#include "Stream/MockNetworkAccessManager.h"
#include "OrderEmulator.h"
#include "MarketDepthQuote.h"
#include "Quote.h"

#define LOGGING_CATEGORY TSClientLog

QPointer<StreamPositions> TSClient::openStreamPositions(const QString& accountID, bool changes)
{
    // In replay mode, accept the simulated account ID
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
        // normal account numbers have 8 digits, sim have additional letters
        OBJ_ASSUME_GTE(accountID.length(), 8);
    }

    DEBUG << "Opening StreamPositions for account " << accountID << " with changes=" << changes;

    QPointer<StreamPositions> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamPositions for account" << accountID;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &accountID]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for position updates from OrderEmulator
                m_replayPositionsReply = mockReply;

                // Start heartbeat for stream keep-alive
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamPositions(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Connect OrderEmulator position updates to the mock reply
                if (m_orderEmulator)
                {
                    connect(m_orderEmulator, &OrderEmulator::positionUpdate, mockReply, &MockNetworkReply::injectData);
                }

                // Send initial empty snapshot with EndSnapshot event
                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamPositions for account" << accountID;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        QUrlQuery query;
        query.addQueryItem("changes", changes ? "true" : "false");

        QNetworkRequest request =
            buildNetworkRequest(QString(TSClientEndpoints::STREAM_POSITIONS).arg(accountID), query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &accountID]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamPositions(accountID, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Don't emit signal for Positions stream (singleton, always 0 or 1)
            },
            Qt::BlockingQueuedConnection);
    }

    return stream;
}

QPointer<StreamOrders> TSClient::openStreamOrders(const QString& accountID)
{
    // In replay mode, accept the simulated account ID
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
        // normal account numbers have 8 digits, sim have additional letters
        OBJ_ASSUME_GTE(accountID.length(), 8);
    }

    DEBUG << "Opening StreamOrders for account" << accountID;

    QPointer<StreamOrders> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamOrders for account" << accountID;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &accountID]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for order updates from OrderEmulator
                m_replayOrdersReply = mockReply;

                // Start heartbeat for stream keep-alive
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamOrders(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Connect OrderEmulator order updates to the mock reply
                if (m_orderEmulator)
                {
                    connect(m_orderEmulator,
                            &OrderEmulator::orderStatusUpdate,
                            mockReply,
                            &MockNetworkReply::injectData);
                }

                // Send initial empty snapshot with EndSnapshot event
                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamOrders for account" << accountID;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        const QString endpoint = QString(TSClientEndpoints::STREAM_ORDERS).arg(accountID);
        QNetworkRequest request = buildNetworkRequest(endpoint);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &accountID]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamOrders(accountID, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Don't emit signal for Orders stream (singleton, always 0 or 1)
            },
            Qt::BlockingQueuedConnection);
    }

    return stream;
}


QPointer<StreamBars> TSClient::openStreamBars(const QString& symbol,
                                              unsigned int interval,
                                              Bar::BarUnit unit,
                                              unsigned int barsback,
                                              Bar::BarSessionTemplate sessionTemplate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute)
    {
        OBJ_ASSUME_GTE(interval, 1u);
    }
    else
    {
        OBJ_ASSUME_EQUAL(interval, 1u);
    }
    OBJ_ASSUME_LTE(barsback, 57600u);

    QPointer<StreamBars> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamBars for" << symbol;

        // In replay mode, create a MockNetworkReply instead of a real network request
        QMetaObject::invokeMethod(
            this,
            [this, &stream, &symbol]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for data injection later
                m_replayBarReplies[symbol] = mockReply;

                stream = new StreamBars(symbol, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Emit updated stream counts
                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthQuote::getNumberOfMarketDepthStreams());

                INFO << "Opened replay StreamBars for" << symbol;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        DEBUG << "Opening live StreamBars for" << symbol;

        // Live mode - make real network request
        const QString endpoint = QString(TSClientEndpoints::STREAM_BARS).arg(symbol);
        QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);
        QNetworkRequest request = buildNetworkRequest(endpoint, query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &symbol]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamBars(symbol, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Emit updated stream counts
                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthQuote::getNumberOfMarketDepthStreams());
            },
            Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
        // finishes executing this lambda so that a valid pointer is returned
    }

    return stream;
}

std::expected<QPointer<StreamMarketDepthQuote>, QFuture<QPointer<StreamMarketDepthQuote>>>
TSClient::openStreamMarketDepthQuote(const QString& symbol, unsigned int depth)
{
    OBJ_ASSUME_GTE(depth, 1u);
    OBJ_ASSUME_LTE(depth, 20u);

    // Check if we can open immediately (atomic check, thread-safe)
    if (!StreamMarketDepthQuote::canOpenStream())
    {
        // Limit reached - queue the request
        INFO << "Market depth stream limit reached (" << StreamMarketDepthQuote::getNumberOfMarketDepthStreams() << "/"
             << MarketDepthConstants::MAX_CONCURRENT_STREAMS << ") - queuing request for" << symbol;

        // Create promise/future pair
        QPromise<QPointer<StreamMarketDepthQuote>> promise;
        QFuture<QPointer<StreamMarketDepthQuote>> future = promise.future();
        promise.start();

        // Add to queue (move promise since it's not copyable)
        m_marketDepthQueue.emplace_back(symbol, depth, std::move(promise));

        INFO << "Request queued for" << symbol << "- Queue size:" << m_marketDepthQueue.size();

        // Return future in expected's error channel
        return std::unexpected(future);
    }

    // Can open immediately - proceed with normal flow
    QPointer<StreamMarketDepthQuote> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamMarketDepthQuote for" << symbol;

        // In replay mode, create a MockNetworkReply instead of a real network request
        QMetaObject::invokeMethod(
            this,
            [this, &stream, &symbol]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for data injection later
                m_replayDepthReplies[symbol] = mockReply;

                stream = new StreamMarketDepthQuote(symbol, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Emit updated stream counts
                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthQuote::getNumberOfMarketDepthStreams());

                INFO << "Opened replay StreamMarketDepthQuote for" << symbol;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        QUrlQuery query;
        query.addQueryItem("maxlevels", QString::number(depth));

        DEBUG << "Opening live StreamMarketDepthQuote for" << symbol;

        QNetworkRequest request =
            buildNetworkRequest(QString(TSClientEndpoints::STREAM_MARKET_DEPTH_QUOTE).arg(symbol), query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &symbol]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamMarketDepthQuote(symbol, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Emit updated stream counts
                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthQuote::getNumberOfMarketDepthStreams());
            },
            Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
        // finishes executing this lambda so that a valid pointer is returned
    }

    // Return stream in expected's value channel
    return stream;
}

QPointer<StreamQuote> TSClient::openStreamQuote(const QStringList& symbols)
{
    OBJ_ASSUME_FALSE(symbols.isEmpty());
    OBJ_ASSUME_LTE(symbols.size(), static_cast<qsizetype>(QuoteConstants::MAX_SYMBOLS_PER_STREAM));

    for (const QString& symbol: symbols)
    {
        OBJ_ASSUME_FALSE(symbol.isEmpty());
        OBJ_ASSUME_FALSE(symbol.contains(','));
    }

    QPointer<StreamQuote> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamQuote for" << symbols.size() << "symbols";

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &symbols]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                for (const QString& symbol: symbols)
                {
                    m_replayQuoteReplies[symbol] = mockReply;
                }

                stream = new StreamQuote(symbols, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened replay StreamQuote for" << symbols.size() << "symbols";
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        DEBUG << "Opening live StreamQuote for" << symbols.size() << "symbols";

        const QString endpoint = QString(TSClientEndpoints::STREAM_QUOTES).arg(symbols.join(','));
        QNetworkRequest request = buildNetworkRequest(endpoint);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &symbols]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamQuote(symbols, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened live StreamQuote for" << symbols.size() << "symbols";
            },
            Qt::BlockingQueuedConnection);
    }

    return stream;
}

void TSClient::closeStream(Stream* const stream)
{
    OBJ_ASSUME_DIFF(stream, nullptr);

    QMetaObject::invokeMethod(
        this,
        [this, stream]() // Capture stream by value, not by reference
        {
            // Use deleteLater() for Stream objects (have timers, network replies, signals)
            stream->deleteLater();

            // Emit count change after the stream is actually deleted
            // Use QueuedConnection to ensure destructor has run first
            QMetaObject::invokeMethod(
                this,
                [this]()
                {
                    emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                             StreamMarketDepthQuote::getNumberOfMarketDepthStreams());
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

// ============================================================================
// Replay Mode Support
// ============================================================================

void TSClient::setMode(Mode p_mode)
{
    if (m_mode == p_mode)
    {
        return;
    }

    INFO << "TSClient mode changing from" << (m_mode == Mode::Live ? "Live" : "Replay") << "to"
         << (p_mode == Mode::Live ? "Live" : "Replay");

    m_mode = p_mode;

    if (p_mode == Mode::Replay)
    {
        // Generate replay session timestamp
        m_replaySessionTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
        INFO << "Replay session timestamp:" << m_replaySessionTimestamp;

        // Create OrderEmulator
        m_orderEmulator = new OrderEmulator(this);

        // Create MockNetworkAccessManager (routes order requests to emulator)
        m_mockNetworkManager = new MockNetworkAccessManager(m_orderEmulator, this);
    }
    else
    {
        // Returning to Live mode - cleanup replay resources

        // Delete replay streams BEFORE cleaning up emulator
        // This ensures StreamOrders/StreamPositions are destroyed before MockNetworkReply
        if (!m_replayOrdersReply.isNull())
        {
            // Find and delete the StreamOrders that owns this MockNetworkReply
            for (QObject* child: children())
            {
                if (auto* stream = qobject_cast<StreamOrders*>(child))
                {
                    delete stream;
                    break;
                }
            }
            m_replayOrdersReply.clear();
        }

        if (!m_replayPositionsReply.isNull())
        {
            // Find and delete the StreamPositions that owns this MockNetworkReply
            for (QObject* child: children())
            {
                if (auto* stream = qobject_cast<StreamPositions*>(child))
                {
                    delete stream;
                    break;
                }
            }
            m_replayPositionsReply.clear();
        }

        // Cancel any pending orders in emulator
        if (m_orderEmulator)
        {
            m_orderEmulator->clear();
        }

        // Delete mock objects
        delete m_orderEmulator;
        m_orderEmulator = nullptr;

        delete m_mockNetworkManager;
        m_mockNetworkManager = nullptr;

        m_replaySessionTimestamp.clear();

        // Clear replay stream tracking
        m_replayBarReplies.clear();
        m_replayDepthReplies.clear();
        m_replayQuoteReplies.clear();
    }
}

bool TSClient::hasOpenBarStream(const QString& p_symbol) const
{
    if (m_mode == Mode::Replay)
    {
        return m_replayBarReplies.contains(p_symbol) && !m_replayBarReplies[p_symbol].isNull();
    }

    // For live mode, we don't track streams by symbol currently
    // This could be enhanced if needed
    return false;
}

bool TSClient::hasOpenMarketDepthStream(const QString& p_symbol) const
{
    if (m_mode == Mode::Replay)
    {
        return m_replayDepthReplies.contains(p_symbol) && !m_replayDepthReplies[p_symbol].isNull();
    }

    // For live mode, we don't track streams by symbol currently
    return false;
}

bool TSClient::hasOpenQuoteStream(const QString& p_symbol) const
{
    if (m_mode == Mode::Replay)
    {
        return m_replayQuoteReplies.contains(p_symbol) && !m_replayQuoteReplies[p_symbol].isNull();
    }

    return false;
}

bool TSClient::hasOpenQuoteStream() const
{
    if (m_mode == Mode::Replay)
    {
        return !m_replayQuoteReplies.isEmpty();
    }

    // Live mode: check static counter for actual stream count
    return StreamQuote::getNumberOfQuoteStreams() > 0;
}

MockNetworkReply* TSClient::getBarReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay)
    {
        return nullptr;
    }

    if (!m_replayBarReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayBarReplies[p_symbol].data();
}

MockNetworkReply* TSClient::getMarketDepthReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay)
    {
        return nullptr;
    }

    if (!m_replayDepthReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayDepthReplies[p_symbol].data();
}

MockNetworkReply* TSClient::getQuoteReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay)
    {
        return nullptr;
    }

    if (!m_replayQuoteReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayQuoteReplies[p_symbol].data();
}

void TSClient::onInjectBarData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data)
{
    if (m_mode != Mode::Replay)
    {
        WARNING << "onInjectBarData called but not in replay mode";
        return;
    }

    if (!p_data)
    {
        WARNING << "onInjectBarData called with null data for" << p_symbol;
        return;
    }

    // Forward bar close price to OrderEmulator for P&L calculation
    if (m_orderEmulator)
    {
        // Parse bar data to extract close price
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(*p_data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject())
        {
            QJsonObject barObj = doc.object();
            if (barObj.contains("Close"))
            {
                double closePrice = barObj["Close"].toString().toDouble();
                m_orderEmulator->updateBarClose(p_symbol, closePrice);
            }
        }
    }

    if (!m_replayBarReplies.contains(p_symbol) || m_replayBarReplies[p_symbol].isNull())
    {
        // No stream open for this symbol - skip silently (expected for non-monitored stocks)
        return;
    }

    m_replayBarReplies[p_symbol]->injectData(*p_data);
}

void TSClient::onInjectDepthData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data)
{
    if (m_mode != Mode::Replay)
    {
        WARNING << "onInjectDepthData called but not in replay mode";
        return;
    }

    if (!p_data)
    {
        WARNING << "onInjectDepthData called with null data for" << p_symbol;
        return;
    }

    // Also forward depth data to OrderEmulator for order fill monitoring
    if (m_orderEmulator)
    {
        // Parse the depth data and forward to emulator
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(*p_data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject())
        {
            MarketDepthQuote depth(doc.object());
            m_orderEmulator->updateMarketDepth(p_symbol, depth);
        }
    }

    if (!m_replayDepthReplies.contains(p_symbol) || m_replayDepthReplies[p_symbol].isNull())
    {
        // No stream open for this symbol - skip silently (expected for non-monitored stocks)
        return;
    }

    m_replayDepthReplies[p_symbol]->injectData(*p_data);
}

void TSClient::onInjectQuoteData(const QString& p_symbol, std::shared_ptr<const QByteArray> p_data)
{
    if (m_mode != Mode::Replay)
    {
        WARNING << "onInjectQuoteData called but not in replay mode";
        return;
    }

    if (!p_data)
    {
        WARNING << "onInjectQuoteData called with null data for" << p_symbol;
        return;
    }

    // Forward quote data to OrderEmulator for Level 1 order fill monitoring
    if (m_orderEmulator)
    {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(*p_data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject())
        {
            Quote quote(doc.object());
            if (quote.isValid())
            {
                m_orderEmulator->updateQuote(p_symbol, quote);
            }
        }
    }

    if (!m_replayQuoteReplies.contains(p_symbol) || m_replayQuoteReplies[p_symbol].isNull())
    {
        // No stream open for this symbol - skip silently (expected for non-monitored stocks)
        return;
    }

    m_replayQuoteReplies[p_symbol]->injectData(*p_data);
}

// ============================================================================
// Market Depth Queue Processing
// ============================================================================

void TSClient::processMarketDepthQueue()
{
    // Must be called from TSClient thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());

    // Check if shutdown in progress
    if (Stream::isShuttingDown())
    {
        DEBUG << "Shutdown in progress - clearing market depth queue";
        m_marketDepthQueue.clear();
        return;
    }

    // Check if queue is empty
    if (m_marketDepthQueue.empty())
    {
        DEBUG << "Market depth queue is empty - nothing to process";
        return;
    }

    // Check if we can open a new stream (should always be true here, but verify)
    if (!StreamMarketDepthQuote::canOpenStream())
    {
        WARNING << "processMarketDepthQueue called but limit still reached - count:"
                << StreamMarketDepthQuote::getNumberOfMarketDepthStreams();
        return;
    }

    // Get the next request from queue (FIFO)
    PendingMarketDepthRequest request = std::move(m_marketDepthQueue.front());
    m_marketDepthQueue.pop_front();

    INFO << "Processing queued market depth request for" << request.symbol
         << "- Queue size after dequeue:" << m_marketDepthQueue.size();

    // Open the stream (same logic as immediate open)
    QPointer<StreamMarketDepthQuote> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening queued replay StreamMarketDepthQuote for" << request.symbol;

        // Already on TSClient thread - no need for invokeMethod
        auto* mockReply = new MockNetworkReply(this);
        Q_CHECK_PTR(mockReply);

        m_replayDepthReplies[request.symbol] = mockReply;

        stream = new StreamMarketDepthQuote(request.symbol, mockReply, this);
        Q_CHECK_PTR(stream);

        auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
        OBJ_ASSUME_TRUE(c);

        INFO << "Opened queued replay StreamMarketDepthQuote for" << request.symbol;
    }
    else
    {
        // Live mode
        DEBUG << "Opening queued live StreamMarketDepthQuote for" << request.symbol;

        QUrlQuery query;
        query.addQueryItem("maxlevels", QString::number(request.depth));

        QNetworkRequest netRequest =
            buildNetworkRequest(QString(TSClientEndpoints::STREAM_MARKET_DEPTH_QUOTE).arg(request.symbol), query);

        // Already on TSClient thread - no need for invokeMethod
        QNetworkReply* reply = m_networkManager->get(netRequest);
        Q_CHECK_PTR(reply);

        stream = new StreamMarketDepthQuote(request.symbol, reply, this);
        Q_CHECK_PTR(stream);

        auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
        OBJ_ASSUME_TRUE(c);

        INFO << "Opened queued live StreamMarketDepthQuote for" << request.symbol;
    }

    // Fulfill the promise with the created stream
    request.promise.addResult(stream);
    request.promise.finish();

    DEBUG << "Queued market depth stream fulfilled for" << request.symbol;
}
