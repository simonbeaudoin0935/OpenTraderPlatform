#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "Stream/MockNetworkReply.h"

#define LOGGING_CATEGORY TSClientLog

#warning dont forget to have a mocked version of StreamOrders and StreamPositions for replay mode if you want to replay orders/positions data in the future. currently only bars and market depth quote have mocked versions for replay.

QPointer<StreamPositions> TSClient::openStreamPositions(const QString& accountID, bool changes)
{
    // normal account numbers have 8 digits, sim have additional letters
    OBJ_ASSUME_GTE(accountID.length(), 8);

    DEBUG << "Opening StreamPositions for account " << accountID << " with changes=" << changes;

    QUrlQuery query;
    query.addQueryItem("changes", changes ? "true" : "false");

    QNetworkRequest request = buildNetworkRequest(QString(TSClientEndpoints::STREAM_POSITIONS).arg(accountID), query);

    QPointer<StreamPositions> stream;

    QMetaObject::invokeMethod(
        this,
        [this, &request, &stream, &accountID]()
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamPositions(accountID, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            OBJ_ASSUME_TRUE(c);

            // Don't emit signal for Positions stream (singleton, always 0 or 1)
        },
        Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
    // finishes executing this lambda so that a valid pointer is returned

    return stream;
}

QPointer<StreamOrders> TSClient::openStreamOrders(const QString& accountID)
{
    // normal account numbers have 8 digits, sim have additional letters
    OBJ_ASSUME_GTE(accountID.length(), 8);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders for account " << accountID;

    const QString endpoint = QString(TSClientEndpoints::STREAM_ORDERS).arg(accountID);

    QNetworkRequest request = buildNetworkRequest(endpoint);

    QPointer<StreamOrders> stream;

    QMetaObject::invokeMethod(
        this,
        [this, &request, &stream, &accountID]()
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamOrders(accountID, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            OBJ_ASSUME_TRUE(c);

            // Don't emit signal for Orders stream (singleton, always 0 or 1)
        },
        Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
    // finishes executing this lambda so that a valid pointer is returned

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

    if (p_mode == Mode::Live)
    {
        // Clear replay stream tracking when returning to live mode
        // The streams themselves are managed by their owners (BarCache, etc.)
        m_replayBarReplies.clear();
        m_replayDepthReplies.clear();
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

    if (!m_replayDepthReplies.contains(p_symbol) || m_replayDepthReplies[p_symbol].isNull())
    {
        // No stream open for this symbol - skip silently (expected for non-monitored stocks)
        return;
    }

    m_replayDepthReplies[p_symbol]->injectData(*p_data);
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

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &request]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                m_replayDepthReplies[request.symbol] = mockReply;

                stream = new StreamMarketDepthQuote(request.symbol, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened queued replay StreamMarketDepthQuote for" << request.symbol;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode
        DEBUG << "Opening queued live StreamMarketDepthQuote for" << request.symbol;

        QUrlQuery query;
        query.addQueryItem("maxlevels", QString::number(request.depth));

        QNetworkRequest netRequest =
            buildNetworkRequest(QString(TSClientEndpoints::STREAM_MARKET_DEPTH_QUOTE).arg(request.symbol), query);

        QMetaObject::invokeMethod(
            this,
            [this, &netRequest, &stream, &request]()
            {
                QNetworkReply* reply = m_networkManager->get(netRequest);
                Q_CHECK_PTR(reply);

                stream = new StreamMarketDepthQuote(request.symbol, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened queued live StreamMarketDepthQuote for" << request.symbol;
            },
            Qt::BlockingQueuedConnection);
    }

    // Fulfill the promise with the created stream
    request.promise.addResult(stream);
    request.promise.finish();

    DEBUG << "Queued market depth stream fulfilled for" << request.symbol;
}
