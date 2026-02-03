#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "Stream/MockNetworkReply.h"

#define LOGGING_CATEGORY TSClientLog

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

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());
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

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());
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

                emit openStreamCountChanged(Stream::getNumberOpenStream());

                INFO << "Opened replay StreamBars for" << symbol;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
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

                // Emit signal that stream count has changed
                emit openStreamCountChanged(Stream::getNumberOpenStream());
            },
            Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
        // finishes executing this lambda so that a valid pointer is returned
    }

    return stream;
}

QPointer<StreamMarketDepthQuote> TSClient::openStreamMarketDepthQuote(const QString& symbol, unsigned int depth)
{
    OBJ_ASSUME_GTE(depth, 1u);
    OBJ_ASSUME_LTE(depth, 20u);

    QPointer<StreamMarketDepthQuote> stream;

    if (m_mode == Mode::Replay)
    {
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

                emit openStreamCountChanged(Stream::getNumberOpenStream());

                INFO << "Opened replay StreamMarketDepthQuote for" << symbol;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        QUrlQuery query;
        query.addQueryItem("maxlevels", QString::number(depth));

        qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";

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

                // Emit signal that stream count has changed
                emit openStreamCountChanged(Stream::getNumberOpenStream());
            },
            Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
        // finishes executing this lambda so that a valid pointer is returned
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
            // Deleting the stream will also close the associated QNetworkReply
            delete stream;

            emit openStreamCountChanged(Stream::getNumberOpenStream());
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
