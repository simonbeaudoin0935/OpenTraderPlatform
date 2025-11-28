#include "TSClient.h"

// Generic stream opening method
Stream* TSClient::openStream(const QNetworkRequest &request, StreamType_t streamType)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, "lambda", "The TSClient thread by design shall never be the one opening a stream, always another thread");

    Stream * stream = nullptr;

    QMetaObject::invokeMethod(this,
        [this, &request, &stream]()
        {
            stream = new Stream();
            Q_CHECK_PTR(stream);

            QNetworkReply *reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            Q_ASSERT(!m_networkReplyToOpenStreams.contains(reply)); // Paranoia

            auto a = connect(reply, &QNetworkReply::readyRead, stream, &Stream::onReplyStreamReadyRead);
            Q_ASSERT(a);
            auto b = connect(reply, &QNetworkReply::finished, stream, &Stream::onReplyStreamReadyRead);
            Q_ASSERT(b);
            auto c = connect(reply, &QNetworkReply::errorOccurred, stream, &Stream::onReplyStreamErrorOccurred);
            Q_ASSERT(c);

            m_networkReplyToOpenStreams[reply] = stream;
                
            // Emit signal that stream count has changed
            emit openStreamCountChanged(m_networkReplyToOpenStreams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned

    qCDebug(TSClientLog) << "Opened stream = " << static_cast<void*>(stream) << "with URL : " << request.url();

    return stream;
}

// Generic stream closing method
void TSClient::closeStream(Stream * const stream)
{
    Q_ASSERT(stream != nullptr);

    QMetaObject::invokeMethod(this,
        [this, &stream]()
        {
            Q_ASSERT(QThread::currentThread() == m_thread);

            QNetworkReply *replyToDelete = nullptr;

            qCDebug(TSClientLog) << "Going through all pending replies";

            for (auto it = m_networkReplyToOpenStreams.constBegin(); it != m_networkReplyToOpenStreams.constEnd(); ++it) {
                QNetworkReply *reply = it.key();

                qCDebug(TSClientLog) << "Checking reply " << static_cast<void*>(reply);

                if (stream == it.value()) {

                    replyToDelete = reply;

                    qCDebug(TSClientLog) << "Found thre reply for stream :" << static_cast<void*>(reply);

                    break;
               }
            }

            Q_ASSERT(replyToDelete != nullptr);

            qCDebug(TSClientLog) << "Aborting request";
     
            replyToDelete->abort();
            replyToDelete->deleteLater();

            bool removed = m_networkReplyToOpenStreams.remove(replyToDelete);
            Q_ASSERT(removed);
     
            delete stream;

            emit openStreamCountChanged(m_networkReplyToOpenStreams.size());
        },
    Qt::BlockingQueuedConnection); // Ensures this thread is blocked until the client thread
                                   // finishes executing this lambda so that a valid pointer is returned
}




// -- Specific stream opening methods - -

StreamPositions *TSClient::openStreamPositions(QString &accountID, bool changes)
{
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    const QString endpoint = QString(ENDPOINT_STREAM_POSITIONS).arg(accountID);

    QUrlQuery query;
    query.addQueryItem("changes", changes? "true":"false");

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamPositions";

    return openStream<StreamPositions>("NOSYMBOL", endpoint, query, accountID);
}

StreamOrders* TSClient::openStreamOrders(QString &accountID) {
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    const QString endpoint = QString(ENDPOINT_STREAM_ORDERS).arg(accountID);

    QUrlQuery query;

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders";

    return openStream<StreamOrders>("NOSYMBOL", endpoint, query, accountID);
}


StreamBars *TSClient::openStreamBars(const QString &symbol,
                                     unsigned int interval,
                                     Bar::BarUnit unit,
                                     unsigned int barsback,
                                     Bar::BarSessionTemplate sessionTemplate,
                                     bool mock)
{
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    const QString endpoint = ENDPOINT_STREAM_BARS;

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamBars";

    return openStream<StreamBars>(symbol, endpoint, query, symbol);
}

StreamMarketDepthQuote* TSClient::openStreamMarketDepthQuote(const QString &symbol, unsigned int depth)
{
    Q_ASSERT(depth >= 1 && depth <= 20);

    const QString endpoint = ENDPOINT_STREAM_MARKET_DEPTH_QUOTE;
    QUrlQuery query;
    query.addQueryItem("maxlevels", QString::number(depth));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamMarketDepthQuote";

    return openStream<StreamMarketDepthQuote>(symbol, endpoint, query, symbol);
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
