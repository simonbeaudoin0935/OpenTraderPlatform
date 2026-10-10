#include "TSClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "Assume.h"
#include "CONSTANTS.h"
#include "Logging.h"
#include "OrderEmulator.h"
#include "Stream/MockNetworkAccessManager.h"
#include "Stream/MockNetworkReply.h"

#define LOGGING_CATEGORY TSClientLog

namespace
{
    QString barUnitToString(TSClient::BarUnit p_unit)
    {
        switch (p_unit)
        {
        case TSClient::BarUnit::Minute:
            return "Minute";
        case TSClient::BarUnit::Daily:
            return "Daily";
        case TSClient::BarUnit::Weekly:
            return "Weekly";
        case TSClient::BarUnit::Monthly:
            return "Monthly";
        }

        Q_UNREACHABLE();
    }

    QString barSessionTemplateToString(TSClient::BarSessionTemplate p_template)
    {
        switch (p_template)
        {
        case TSClient::BarSessionTemplate::USEQPre:
            return "USEQPre";
        case TSClient::BarSessionTemplate::USEQPost:
            return "USEQPost";
        case TSClient::BarSessionTemplate::USEPreAndPost:
            return "USEPreAndPost";
        case TSClient::BarSessionTemplate::USEQ24Hour:
            return "USEQ24Hour";
        case TSClient::BarSessionTemplate::Default:
            return "Default";
        }

        Q_UNREACHABLE();
    }

    QUrlQuery buildBarsQuery(unsigned int p_interval,
                             TSClient::BarUnit p_unit,
                             unsigned int p_barsback,
                             TSClient::BarSessionTemplate p_sessionTemplate)
    {
        QUrlQuery query;
        query.addQueryItem("interval", QString::number(p_interval));
        query.addQueryItem("unit", barUnitToString(p_unit));
        query.addQueryItem("sessiontemplate", barSessionTemplateToString(p_sessionTemplate));
        query.addQueryItem("barsback", QString::number(p_barsback));
        return query;
    }
} // namespace

QPointer<StreamPositions> TSClient::openStreamPositions(const QString& accountID, bool changes)
{
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
        OBJ_ASSUME_GTE(accountID.length(), 8);
    }

    DEBUG << "Opening StreamPositions for account" << accountID << "with changes=" << changes;

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

                m_replayPositionsReply = mockReply;
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamPositions(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                if (m_orderEmulator)
                {
                    connect(m_orderEmulator, &OrderEmulator::positionUpdate, mockReply, &MockNetworkReply::injectData);
                }

                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamPositions for account" << accountID;
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }
    else
    {
        QUrlQuery query;
        query.addQueryItem("changes", changes ? "true" : "false");

        QNetworkRequest request =
            buildStreamRequest(QString(TSClientEndpoints::STREAM_POSITIONS).arg(accountID), query);

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
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }

    return stream;
}

QPointer<StreamOrders> TSClient::openStreamOrders(const QString& accountID)
{
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
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

                m_replayOrdersReply = mockReply;
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamOrders(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                if (m_orderEmulator)
                {
                    connect(m_orderEmulator,
                            &OrderEmulator::orderStatusUpdate,
                            mockReply,
                            &MockNetworkReply::injectData);
                }

                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamOrders for account" << accountID;
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }
    else
    {
        const QString endpoint = QString(TSClientEndpoints::STREAM_ORDERS).arg(accountID);
        QNetworkRequest request = buildStreamRequest(endpoint);

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
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }

    return stream;
}

QPointer<StreamBars> TSClient::openStreamBars(const QString& symbol,
                                              unsigned int interval,
                                              BarUnit unit,
                                              unsigned int barsback,
                                              BarSessionTemplate sessionTemplate)
{
    if (unit == BarUnit::Minute)
    {
        OBJ_ASSUME_GTE(interval, 1u);
    }
    else
    {
        OBJ_ASSUME_EQUAL(interval, 1u);
    }
    OBJ_ASSUME_LTE(barsback, 57600u);

    QPointer<StreamBars> stream;
    const int minuteIntervalSeconds = (unit == BarUnit::Minute) ? static_cast<int>(interval) * 60 : 0;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamBars for" << symbol;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &symbol, minuteIntervalSeconds]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                m_replayBarReplies[symbol] = mockReply;

                stream = new StreamBars(symbol, mockReply, this, minuteIntervalSeconds);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                c = connect(stream,
                            &StreamBars::newBarReceived,
                            this,
                            [this, symbol](const Bar& p_bar) { emit newBarReceived(symbol, p_bar); });
                OBJ_ASSUME_TRUE(c);

                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams());

                INFO << "Opened replay StreamBars for" << symbol;
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }
    else
    {
        DEBUG << "Opening live StreamBars for" << symbol;

        const QString endpoint = QString(TSClientEndpoints::STREAM_BARS).arg(symbol);
        QUrlQuery query = buildBarsQuery(interval, unit, barsback, sessionTemplate);
        QNetworkRequest request = buildStreamRequest(endpoint, query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &symbol, minuteIntervalSeconds]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamBars(symbol, reply, this, minuteIntervalSeconds);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                c = connect(stream,
                            &StreamBars::newBarReceived,
                            this,
                            [this, symbol](const Bar& p_bar) { emit newBarReceived(symbol, p_bar); });
                OBJ_ASSUME_TRUE(c);

                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams());
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }

    return stream;
}

std::expected<QPointer<StreamMarketDepthAggregate>, QFuture<QPointer<StreamMarketDepthAggregate>>>
TSClient::openStreamMarketDepthAggregate(const QString& symbol, unsigned int depth)
{
    OBJ_ASSUME_GTE(depth, 1u);
    OBJ_ASSUME_LTE(depth, 20u);

    if (QThread::currentThread() != thread())
    {
        std::optional<
            std::expected<QPointer<StreamMarketDepthAggregate>, QFuture<QPointer<StreamMarketDepthAggregate>>>>
            result;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            [this, &result, &symbol, depth]() { result = openStreamMarketDepthAggregate(symbol, depth); },
            Qt::BlockingQueuedConnection);
        OBJ_ASSUME_TRUE(invoked);
        OBJ_ASSUME_TRUE(result.has_value());
        return result.value();
    }

    if (!StreamMarketDepthAggregate::canOpenStream())
    {
        INFO << "Market depth stream limit reached ("
             << StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams() << "/"
             << MarketDepthConstants::MAX_CONCURRENT_STREAMS << ") - queuing request for" << symbol;

        QPromise<QPointer<StreamMarketDepthAggregate>> promise;
        QFuture<QPointer<StreamMarketDepthAggregate>> future = promise.future();
        promise.start();

        m_marketDepthQueue.emplace_back(symbol, depth, std::move(promise));

        INFO << "Request queued for" << symbol << "- Queue size:" << m_marketDepthQueue.size();
        return std::unexpected(future);
    }

    QPointer<StreamMarketDepthAggregate> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamMarketDepthAggregate for" << symbol;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &symbol]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                m_replayDepthReplies[symbol] = mockReply;

                stream = new StreamMarketDepthAggregate(symbol, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                c = connect(stream,
                            &StreamMarketDepthAggregate::newLevel2Received,
                            this,
                            [this, symbol](const Level2& p_level2) { emit newLevel2Received(symbol, p_level2); });
                OBJ_ASSUME_TRUE(c);

                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams());

                INFO << "Opened replay StreamMarketDepthAggregate for" << symbol;
            },
            Qt::DirectConnection);
    }
    else
    {
        QUrlQuery query;
        query.addQueryItem("maxlevels", QString::number(depth));

        DEBUG << "Opening live StreamMarketDepthAggregate for" << symbol;

        QNetworkRequest request =
            buildStreamRequest(QString(TSClientEndpoints::STREAM_MARKET_DEPTH_AGGREGATE).arg(symbol), query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &symbol]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamMarketDepthAggregate(symbol, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                c = connect(stream,
                            &StreamMarketDepthAggregate::newLevel2Received,
                            this,
                            [this, symbol](const Level2& p_level2) { emit newLevel2Received(symbol, p_level2); });
                OBJ_ASSUME_TRUE(c);

                emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                         StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams());
            },
            Qt::DirectConnection);
    }

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

                c = connect(stream,
                            &StreamQuote::newQuoteReceived,
                            this,
                            [this](const Quote& quote) { emit newQuoteReceived(quote.getSymbol(), quote); });
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened replay StreamQuote for" << symbols.size() << "symbols";
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }
    else
    {
        DEBUG << "Opening live StreamQuote for" << symbols.size() << "symbols";

        const QString endpoint = QString(TSClientEndpoints::STREAM_QUOTES).arg(symbols.join(','));
        QNetworkRequest request = buildStreamRequest(endpoint);

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

                c = connect(stream,
                            &StreamQuote::newQuoteReceived,
                            this,
                            [this](const Quote& quote) { emit newQuoteReceived(quote.getSymbol(), quote); });
                OBJ_ASSUME_TRUE(c);

                INFO << "Opened live StreamQuote for" << symbols.size() << "symbols";
            },
            QThread::currentThread() == thread() ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
    }

    return stream;
}

void TSClient::processMarketDepthQueue()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());

    if (m_marketDepthQueue.empty())
    {
        return;
    }

    if (!StreamMarketDepthAggregate::canOpenStream())
    {
        return;
    }

    PendingMarketDepthRequest request = std::move(m_marketDepthQueue.front());
    m_marketDepthQueue.pop_front();

    INFO << "Processing queued market depth request for" << request.symbol
         << "- Remaining queue size:" << m_marketDepthQueue.size();

    auto result = openStreamMarketDepthAggregate(request.symbol, request.depth);

    if (result.has_value())
    {
        request.promise.addResult(result.value());
        request.promise.finish();
    }
    else
    {
        request.promise.addResult(QPointer<StreamMarketDepthAggregate>());
        request.promise.finish();
    }
}

void TSClient::closeStream(Stream* const stream)
{
    OBJ_ASSUME_DIFF(stream, nullptr);

    QMetaObject::invokeMethod(
        this,
        [this, stream]()
        {
            stream->deleteLater();

            QMetaObject::invokeMethod(
                this,
                [this]()
                {
                    emit streamCountsChanged(StreamBars::getNumberOfBarsStreams(),
                                             StreamMarketDepthAggregate::getNumberOfMarketDepthAggregateStreams());
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

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
        m_replaySessionTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
        INFO << "Replay session timestamp:" << m_replaySessionTimestamp;

        m_orderEmulator = new OrderEmulator(this);
        m_mockNetworkManager = new MockNetworkAccessManager(m_orderEmulator, this);
    }
    else
    {
        if (!m_replayOrdersReply.isNull())
        {
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

        if (m_orderEmulator)
        {
            m_orderEmulator->clear();
        }

        delete m_orderEmulator;
        m_orderEmulator = nullptr;

        delete m_mockNetworkManager;
        m_mockNetworkManager = nullptr;

        m_replaySessionTimestamp.clear();

        m_replayBarReplies.clear();
        m_replayDepthReplies.clear();
        m_replayQuoteReplies.clear();
        m_replayQuoteState.clear();
        m_replayOrdersReply.clear();
        m_replayPositionsReply.clear();
    }
}

bool TSClient::hasOpenBarStream(const QString& p_symbol) const
{
    if (m_mode == Mode::Replay)
    {
        return m_replayBarReplies.contains(p_symbol) && !m_replayBarReplies[p_symbol].isNull();
    }
    return false;
}

bool TSClient::hasOpenMarketDepthStream(const QString& p_symbol) const
{
    if (m_mode == Mode::Replay)
    {
        return m_replayDepthReplies.contains(p_symbol) && !m_replayDepthReplies[p_symbol].isNull();
    }
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

    return StreamQuote::getNumberOfQuoteStreams() > 0;
}

MockNetworkReply* TSClient::getBarReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay || !m_replayBarReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayBarReplies[p_symbol].data();
}

MockNetworkReply* TSClient::getMarketDepthReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay || !m_replayDepthReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayDepthReplies[p_symbol].data();
}

MockNetworkReply* TSClient::getQuoteReplyForSymbol(const QString& p_symbol) const
{
    if (m_mode != Mode::Replay || !m_replayQuoteReplies.contains(p_symbol))
    {
        return nullptr;
    }

    return m_replayQuoteReplies[p_symbol].data();
}

void TSClient::rotateReplaySessionTimestamp()
{
    ASSUME_EQUAL(m_mode, Mode::Replay);
    m_replaySessionTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    INFO << "Replay session timestamp rotated to" << m_replaySessionTimestamp;
}
