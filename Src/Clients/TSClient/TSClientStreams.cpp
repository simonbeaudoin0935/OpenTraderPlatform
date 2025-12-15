#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY TSClientLog

QPointer<StreamPositions> TSClient::openStreamPositions(const QString &accountID, bool changes)
{
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters
    
    DEBUG << "Opening StreamPositions for account " << accountID << " with changes=" << changes;
    
    QUrlQuery query;
    query.addQueryItem("changes", changes? "true":"false");

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_STREAM_POSITIONS).arg(accountID),
                                                  query);

    QPointer<StreamPositions> stream;

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

    return stream;
}

QPointer<StreamOrders> TSClient::openStreamOrders(const QString &accountID) {
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders for account " << accountID;

    const QString endpoint = QString(ENDPOINT_STREAM_ORDERS).arg(accountID);

    QNetworkRequest request = buildNetworkRequest(endpoint);

    QPointer<StreamOrders> stream;

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

    return stream;
}


QPointer<StreamBars> TSClient::openStreamBars(const QString &symbol,
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

    QPointer<StreamBars> stream;

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

    return stream;
}

QPointer<StreamMarketDepthQuote> TSClient::openStreamMarketDepthQuote(const QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";


    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_STREAM_MARKET_DEPTH_QUOTE).arg(symbol),
                                                  query);

    QPointer<StreamMarketDepthQuote> stream;

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

    return stream;
}

void TSClient::closeStream(Stream * const stream)
{
    Q_ASSERT(stream != nullptr);

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            // Deleting the stream will also close the associated QNetworkReply
            delete stream;

            emit openStreamCountChanged(Stream::getNumberOpenStream());
        },
    Qt::QueuedConnection);
}
