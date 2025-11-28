#include <QJsonObject>
#include <QJsonArray>

#include "TSClient.h"


/*
 * @brief Fetches data asynchronously.
 *
 * This function sends an asynchronous network request using the specified HTTP method and parameters.
 * By design, the thread executint this function will never be the Client thread itself, except for the token refresh
 */
TSClient::AsyncRequestID_t TSClient::sendAsyncRequest(const QNetworkRequest &request, AsyncRequestType_t type, HttpMethod method, const QByteArray &postData)
{ 
    Q_ASSERT(type != AsyncRequestType_t::None && type < AsyncRequestType_t::MAX_REQUEST_TYPE);
    Q_ASSERT(QThread::currentThread() != m_thread || type == AsyncRequestType_t::GetRefreshAccessToken); // Only token refresh (type 1) can be sent from the client thread itself

    // Increment and fetch a new request ID atomically
    AsyncRequestID_t requestID = m_asyncRequestIDCurrentSequence.fetchAndAddRelaxed(1);

    // Because this is a queud method invocation, the parameters have to be passed by value
    // TODO in the future, to avoid postData especialy, we could new it where it is build and deleted inside here
    QMetaObject::invokeMethod(this, [this, request, requestID, type, method, postData]() {
        Q_ASSERT(QThread::currentThread() == m_thread);

        QNetworkReply *reply = nullptr;
        
        switch (method) {
            case HttpMethod::GET:
                reply = m_networkManager->get(request);
                break;
            case HttpMethod::POST:
                // Use the provided postData if available, otherwise send empty data
                reply = m_networkManager->post(request, postData);
                break;
            case HttpMethod::PUT:
                Q_ASSERT_X(0, "fetchAsync()", "No feature require a PUT method, this is a bugS");
                break;
            case HttpMethod::DELETE:
                reply = m_networkManager->deleteResource(request);
                break;
            default:
                Q_UNREACHABLE();
                break;
        }

        Q_CHECK_PTR(reply);

        Q_ASSERT(!m_networkReplyToPendingAsyncRequests.contains(reply)); // Paranoia

        auto a = connect(reply, &QNetworkReply::readyRead, this, &TSClient::onReplyAsyncRequestReadyRead);
        Q_ASSERT(a);
        auto b = connect(reply, &QNetworkReply::finished, this, &TSClient::onReplyAsyncRequestFinished);
        Q_ASSERT(b);
        auto c = connect(reply, &QNetworkReply::errorOccurred, this, &TSClient::onReplyAsyncRequestErrorOccurred);
        Q_ASSERT(c);


        m_networkReplyToPendingAsyncRequests[reply] = {.requestID = requestID, .type = type};

        emit pendingAsyncRequestsCountChanges(m_networkReplyToOpenStreams.size());

        qCDebug(TSClientLog) << "Sent request to Network Manager and registered: request_id=" << requestID << " with reply addr=" << static_cast<void*>(reply);

    }, Qt::QueuedConnection);

    return requestID;
}


TSClient::AsyncRequestID_t TSClient::getAccountsAsync()
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching accounts async";

    QNetworkRequest request = buildNetworkRequest(ENDPOINT_GET_ACCOUNTS);

    return sendAsyncRequest(request, AsyncRequestType_t::GetAccounts);
}

TSClient::AsyncRequestID_t TSClient::getBalancesAsync(const QStringList &accounts)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    Q_ASSERT(!accounts.isEmpty());
    Q_ASSERT(accounts.size() == 1); // For now only single account is supported
    QString account = accounts.at(0);

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_BALANCES).arg(account));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Balances";

    return sendAsyncRequest(request, AsyncRequestType_t::GetBalances);
}

TSClient::AsyncRequestID_t TSClient::getBarsAsync(const QString &symbol,
                           unsigned int interval,
                           Bar::BarUnit unit,
                           unsigned int barsback,
                           Bar::BarSessionTemplate sessionTemplate,
                           QDateTime firstDate,
                           QDateTime lastDate)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");

    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);

    if (barsback > 0) Q_ASSERT(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);

    Q_ASSERT(!symbol.isEmpty());

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_BARS).arg(symbol), query);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching Bars for symbols : " << symbol;

    return sendAsyncRequest(request, AsyncRequestType_t::GetBars);
}


TSClient::AsyncRequestID_t TSClient::getQuoteSnapshotsAsync(const QStringList &symbols)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(!symbols.isEmpty());
    Q_ASSERT(symbols.size() == 1); // For now only single account is supported
    QString symbol = symbols.at(0);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Fetching quotes for symbols : " << symbols;

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_QUOTE_SNAPSHOTS).arg(symbol));

    return sendAsyncRequest(request, AsyncRequestType_t::GetQuoteSnapshots);
}

TSClient::AsyncRequestID_t TSClient::placeOrderAsync(const PlaceOrderRequest &order)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(order.isValid());

    QNetworkRequest request = buildNetworkRequest(ENDPOINT_PLACE_ORDER);

    QByteArray postData = QJsonDocument(order.toJson()).toJson(QJsonDocument::Compact);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Placing order async";

    return sendAsyncRequest(request,
                      AsyncRequestType_t::PlaceOrder,
                      HttpMethod::POST,
                      postData);
}

TSClient::AsyncRequestID_t TSClient::cancelOrderAsync(const QString &orderID)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread, qPrintable(QThread::currentThread()->objectName()), "TSClient object cannot call this function itself");
    Q_ASSERT(!orderID.isEmpty());
    Q_ASSERT(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_CANCEL_ORDER).arg(orderID));

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Cancel order async";

    return sendAsyncRequest(request,
                      AsyncRequestType_t::CancelOrder,
                      HttpMethod::DELETE);
}



void TSClient::onReplyAsyncRequestReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT(false);
}

void TSClient::onReplyAsyncRequestFinished()
{
    QJsonParseError parseError;
    QJsonDocument doc;

    Q_ASSERT(QThread::currentThread() == m_thread); // Paranoia

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT(m_networkReplyToPendingAsyncRequests.contains(reply));

    qCDebug(TSClientLog) << " reply of request " << static_cast<void*>(reply);

    QByteArray rawData = reply->readAll();
    
    processNewAmountOfDataReceived(rawData.size());
    
    RequestInfo * const requestInfo = &m_networkReplyToPendingAsyncRequests[reply];

    AsyncRequestStatus_e status = AsyncRequestStatus_e::UNSET;

    switch (reply->error())
    {
        case QNetworkReply::NoError:
            status = AsyncRequestStatus_e::SUCCESS;
            break;

        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::UnknownNetworkError:
            status = AsyncRequestStatus_e::TIMEOUT;
            qCCritical(TSClientLog) << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
            goto notify;
            break;

        case QNetworkReply::ContentNotFoundError:
            #warning fix this crap 
            if (requestInfo->type == AsyncRequestType_t::GetBars) {
                // IMPORTANT EDGE CASE
                // Its possible in the case of getBars for example to receive this, as its possible to ask for a range of bars
                // in the after market for instance where there just isnt any bars
                status = AsyncRequestStatus_e::SUCCESS;
            } else {
                status = AsyncRequestStatus_e::ERROR;
                qCCritical(TSClientLog) << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
                goto notify;
            }
            break;

        default:
            status = AsyncRequestStatus_e::ERROR;

            qCCritical(TSClientLog) << " : Error with the reply " << static_cast<void*>(reply) << " : " << reply->errorString() << " : " << reply->error();
            goto notify;
            break;
    };

    {
        doc = QJsonDocument::fromJson(rawData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            status = AsyncRequestStatus_e::ERROR;
            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
            goto notify;
        }

        if (doc.isNull()){
            status = AsyncRequestStatus_e::ERROR;
            qCCritical(TSClientLog) << " : JSON doc is null";
            goto notify;
        }

        if (doc.isArray() && doc.array().isEmpty()) {
            qCWarning(TSClientLog) << " : Doc array is empty";
            //Ccontinue, this is legal
        }

        if (doc.isObject() && doc.object().isEmpty()) {
            qCWarning(TSClientLog) << " : Doc object is empty";
            // Continue, this is legal
        }
    }

notify:
    // The request might have failed, this info is passed along
    demuxReceivedAsyncRequestReply(requestInfo->type, doc, requestInfo->requestID, status);

    // Whether the request was successful or not, take it out of the map
    bool removed = m_networkReplyToPendingAsyncRequests.remove(reply);
    Q_ASSERT(removed);

    reply->deleteLater();
}

void TSClient::onReplyAsyncRequestErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    Q_ASSERT(false);
}


void TSClient::demuxReceivedAsyncRequestReply(AsyncRequestType_t type, const QJsonDocument &doc, AsyncRequestID_t requestID, AsyncRequestStatus_e status)
{
    QJsonObject obj = doc.object();

    Q_ASSERT(requestID > 0);

    if (status != AsyncRequestStatus_e::SUCCESS) {
        qCCritical(TSClientLog) << "Async operation not completed";
    }

    switch(type) {

        case AsyncRequestType_t::None:
            Q_ASSERT_X(0,"","Should not be None anymore");
            break;

        case AsyncRequestType_t::GetAccounts:
        {
            QVector<Account> results;

            if (status == AsyncRequestStatus_e::SUCCESS) {
                const QJsonArray accountsArray = obj["Accounts"].toArray();
            
                for (const QJsonValue &json: accountsArray) {
                    results.push_back(Account(json.toObject()));
                }
            }

            emit receivedAsyncGetAccounts(requestID, status, results);
            break;
        }

        case AsyncRequestType_t::GetBalances:
        {
            QVector<Balance> results;

            if (status == AsyncRequestStatus_e::SUCCESS) {
                const QJsonArray balancesArray = obj["Balances"].toArray();

                for (const QJsonValue &json: balancesArray) {
                    results.push_back(Balance(json.toObject()));
                }
            }

            emit receivedAsyncGetBalances(requestID, status, results);
            break;
        }

        case AsyncRequestType_t::GetBars:
        {
            QVector<Bar> results;

            if (status == AsyncRequestStatus_e::SUCCESS) {
                const QJsonArray barsArray = obj["Bars"].toArray();

                for (const QJsonValue &json: barsArray) {
                    results.push_back(Bar(json.toObject()));
                }
            }

            emit receivedAsyncGetBars(requestID, status, *symbol, results);

                break;
        }

        case AsyncRequestType_t::GetQuoteSnapshots:
            Q_ASSERT(0); //TODO not yet implemented
            break;

        case AsyncRequestType_t::PlaceOrder:
        {
            PlaceOrderResult result;

            if (status == AsyncRequestStatus_e::SUCCESS) {
                result = PlaceOrderResult(doc.object());
            }

            emit receivedAsyncPlaceOrder(requestID, status, result);

            break;
        }

        case AsyncRequestType_t::CancelOrder:
        {
            CancelOrderResult result;

            if (status == AsyncRequestStatus_e::SUCCESS) {
                result = CancelOrderResult(doc.object());
            }

            emit receivedAsyncCancelOrder(requestID, status, result);

            break;
        }

        case AsyncRequestType_t::GetRefreshAccessToken:
        {
            AuthToken token;
            if (status == AsyncRequestStatus_e::SUCCESS) {
                token = AuthToken::receiveAuthToken(obj);
            }
            // No emit on purpose, this is calling a private function of this class
            processAsyncRefreshTokenFinished(requestID, status, token);
            break;
        }

    default:
        Q_UNREACHABLE();
        break;
    }
}
