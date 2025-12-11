#include "TSClient.h"

// Generic stream closing method
void TSClient::closeStream(Stream * const stream)
{
    Q_ASSERT(stream != nullptr);

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            Q_ASSERT(QThread::currentThread() == m_thread);

            // Deleting the stream will also close the associated QNetworkReply
            delete stream;

            emit openStreamCountChanged(Stream::getNumberOpenStream());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned
}




// -- Specific stream opening methods - -

QPair<QFuture<Position>, StreamPositions*> TSClient::openStreamPositions(const QString &accountID, bool changes)
{
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters
    
    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamPositions for account " << accountID << " with changes=" << changes;

    
    const QString endpoint = QString(ENDPOINT_STREAM_POSITIONS).arg(accountID);

    QUrlQuery query;
    query.addQueryItem("changes", changes? "true":"false");

    QNetworkRequest request = buildNetworkRequest(endpoint, query);

    StreamPositions* stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream, &accountID]()
        {
            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamPositions(accountID, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            Q_ASSERT(c);

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    return QPair<QFuture<Position>, StreamPositions*>(stream->future(), stream);
}

QPair<QFuture<Order>, StreamOrders*> TSClient::openStreamOrders(const QString &accountID) {
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders for account " << accountID;

    const QString endpoint = QString(ENDPOINT_STREAM_ORDERS).arg(accountID);

    QNetworkRequest request = buildNetworkRequest(endpoint);

    StreamOrders* stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream, &accountID]()
        {
            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamOrders(accountID, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            Q_ASSERT(c);

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());

        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    return QPair<QFuture<Order>, StreamOrders*>(stream->future(), stream);
}


QPair<QFuture<Bar>, StreamBars*> TSClient::openStreamBars(const QString &symbol,
                                     unsigned int interval,
                                     Bar::BarUnit unit,
                                     unsigned int barsback,
                                     Bar::BarSessionTemplate sessionTemplate)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = QString(ENDPOINT_STREAM_BARS).arg(symbol);

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    QNetworkRequest request = buildNetworkRequest(endpoint, query);

    StreamBars* stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream, &symbol]()
        {
            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamBars(symbol, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            Q_ASSERT(c);

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    return QPair<QFuture<Bar>, StreamBars*>(stream->future(), stream);
}

QPair<QFuture<MarketDepthQuote>, StreamMarketDepthQuote*> TSClient::openStreamMarketDepthQuote(const QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    const QString endpoint = QString(ENDPOINT_STREAM_MARKET_DEPTH_QUOTE).arg(symbol);
    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";


    QNetworkRequest request = buildNetworkRequest(endpoint, query);

    StreamMarketDepthQuote* stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream, &symbol]()
        {
            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            stream = new StreamMarketDepthQuote(symbol, reply, this);
            Q_CHECK_PTR(stream);

            auto c = connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
            Q_ASSERT(c);

            // Emit signal that stream count has changed
            emit openStreamCountChanged(Stream::getNumberOpenStream());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    return QPair<QFuture<MarketDepthQuote>, StreamMarketDepthQuote*>(stream->future(), stream);
}


// -- Specific stream closing methods - -
void TSClient::closeStreamPositions(StreamPositions *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamPositions " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

void TSClient::closeStreamOrders(StreamOrders* stream) {
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamOrders " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

void TSClient::closeStreamBars(StreamBars *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamBars " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}

void TSClient::closeStreamMarketDepthQuote(StreamMarketDepthQuote *stream)
{
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamMarketDepthQuote " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}



// -- Generic stream reply handlers - -
