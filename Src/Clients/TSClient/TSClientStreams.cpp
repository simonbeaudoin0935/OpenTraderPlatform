#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

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

    const QString endpoint = QString(TSClientEndpoints::STREAM_BARS).arg(symbol);

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    QNetworkRequest request = buildNetworkRequest(endpoint, query);

    QPointer<StreamBars> stream;

    QMetaObject::invokeMethod(
        this,
        [this, &request, &stream, &symbol]()
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamBars(symbol, reply, this);
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

QPointer<StreamMarketDepthQuote> TSClient::openStreamMarketDepthQuote(const QString& symbol, unsigned int depth)
{
    OBJ_ASSUME_GTE(depth, 1u);
    OBJ_ASSUME_LTE(depth, 20u);

    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";


    QNetworkRequest request =
        buildNetworkRequest(QString(TSClientEndpoints::STREAM_MARKET_DEPTH_QUOTE).arg(symbol), query);

    QPointer<StreamMarketDepthQuote> stream;

    QMetaObject::invokeMethod(
        this,
        [this, &request, &stream, &symbol]()
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamMarketDepthQuote(symbol, reply, this);
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
