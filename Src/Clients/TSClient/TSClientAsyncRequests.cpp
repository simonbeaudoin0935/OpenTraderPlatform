#include <QJsonObject>
#include <QJsonArray>

#include "TSClient.h"


TSClient::GetAccountsFuture_t TSClient::getAccounts()
{
    qCDebug(TSClientLog) << "Fetching accounts";

    QNetworkRequest request = buildNetworkRequest(ENDPOINT_GET_ACCOUNTS);

    QPromise<GetAccountsResult_t> promise;
    GetAccountsFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->get(request);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isArray()) {
                            qCCritical(TSClientLog) << " : JSON is not an array";
                            promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (doc.array().isEmpty()) {
                            qCCritical(TSClientLog) << " : JSON is an empty array";
                            promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonValue val = doc["Accounts"];

                        if(val == QJsonValue::Undefined) {
                            qCCritical(TSClientLog) << " : 'Accounts' field is missing in the response";
                            promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonArray accountsArray = val.toArray();

                        if (accountsArray.isEmpty()) {
                            qCCritical(TSClientLog) << " : 'Accounts' array is empty in the response";
                            promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        // Happiest path
                        QVector<Account> results;
                        for (const QJsonValue &json: accountsArray) {
                            results.push_back(Account(json.toObject()));
                        }

                        promise.addResult(GetAccountsResult_t(results));
                        break;
                    }

                    // IMPORTANT EDGE CASE: In getBars API, its possible to receive this when theres just no bars in the range, its not really an error
                    case QNetworkReply::ContentNotFoundError:
                    {
                        QVector<Account> results; // empty vector
                        promise.addResult(GetAccountsResult_t(results));
                        break;
                    }
        
                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": getAccounts(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetAccountsResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": getAccounts(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetAccountsResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent getAccounts() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}

TSClient::GetBalancesFuture_t TSClient::getBalances(const QStringList &accounts)
{
    Q_ASSERT(!accounts.isEmpty());
    Q_ASSERT(accounts.size() == 1); // For now only single account is supported
    QString account = accounts.at(0);

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_BALANCES).arg(account));

    qCDebug(TSClientLog) << "Fetching Balances";
    
    QPromise<GetBalancesResult_t> promise;
    GetBalancesFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->get(request);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isArray()) {
                            qCCritical(TSClientLog) << " : JSON is not an array";
                            promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (doc.array().isEmpty()) {
                            qCCritical(TSClientLog) << " : JSON is an empty array";
                            promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonValue val = doc["Balances"];

                        if(val == QJsonValue::Undefined) {
                            qCCritical(TSClientLog) << " : 'Balances' field is missing in the response";
                            promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonArray balancesArray = val.toArray();

                        if (balancesArray.isEmpty()) {
                            qCCritical(TSClientLog) << " : 'Balances' array is empty in the response";
                            promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        // Happiest path
                        QVector<Balance> results;
                        for (const QJsonValue &json: balancesArray) {
                            results.push_back(Balance(json.toObject()));
                        }

                        promise.addResult(GetBalancesResult_t(results));
                        break;
                    }
        
                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": getBalances(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetBalancesResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": getBalances(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetBalancesResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent getBalances() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}

TSClient::GetBarsFuture_t TSClient::getBars(const QString &symbol,
                                            unsigned int interval,
                                            Bar::BarUnit unit,
                                            unsigned int barsback,
                                            Bar::BarSessionTemplate sessionTemplate,
                                            QDateTime firstDate,
                                            QDateTime lastDate)
{
    Q_ASSERT(!symbol.isEmpty());
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute) {Q_ASSERT(interval >= 1);}
    else { Q_ASSERT(interval == 1);}
    Q_ASSERT(barsback <= 57600);
    if (barsback > 0) Q_ASSERT(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_BARS).arg(symbol), query);

    qCDebug(TSClientLog) << "Fetching Bars for symbols : " << symbol;

    QPromise<GetBarsResult_t> promise;
    GetBarsFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->get(request);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isArray()) {
                            qCCritical(TSClientLog) << " : JSON is not an array";
                            promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (doc.array().isEmpty()) {
                            qCCritical(TSClientLog) << " : JSON is an empty array";
                            promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonValue val = doc["Bars"];

                        if(val == QJsonValue::Undefined) {
                            qCCritical(TSClientLog) << " : 'Bars' field is missing in the response";
                            promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonArray barsArray = val.toArray();
                        if (barsArray.isEmpty()) {
                            qCCritical(TSClientLog) << " : 'Bars' array is empty in the response";
                            promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        // Happiest path
                        QVector<Bar> results;
                        for (const QJsonValue &json: barsArray) {
                            results.push_back(Bar(json.toObject()));
                        }

                        promise.addResult(GetBarsResult_t(results));
                        break;
                    }
        
                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": getBars(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetBarsResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": getBars(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetBarsResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent getBars() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}


TSClient::GetQuoteSnapshotFuture_t TSClient::getQuoteSnapshots(const QStringList &symbols)
{
    Q_ASSERT(!symbols.isEmpty());
    Q_ASSERT(symbols.size() == 1); // For now only single account is supported
    QString symbol = symbols.at(0);

    qCDebug(TSClientLog) << "Fetching quotes for symbols : " << symbols;

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_GET_QUOTE_SNAPSHOTS).arg(symbol));

    QPromise<GetQuoteSnapshotResult_t> promise;
    GetQuoteSnapshotFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->get(request);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isArray()) {
                            qCCritical(TSClientLog) << " : JSON is not an array";
                            promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (doc.array().isEmpty()) {
                            qCCritical(TSClientLog) << " : JSON is an empty array";
                            promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonValue val = doc["Quotes"];

                        if(val == QJsonValue::Undefined) {
                            qCCritical(TSClientLog) << " : 'Quotes' field is missing in the response";
                            promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const QJsonArray quotesArray = val.toArray();
                        if (quotesArray.isEmpty()) {
                            qCCritical(TSClientLog) << " : 'Quotes' array is empty in the response";
                            promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        // Happiest path
                        QVector<Quote> results;
                        for (const QJsonValue &json: quotesArray) {
                            results.push_back(Quote(json.toObject()));
                        }

                        promise.addResult(GetQuoteSnapshotResult_t(results));
                        break;
                    }
        
                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": getQuoteSnapshots(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": getQuoteSnapshots(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(GetQuoteSnapshotResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent getBars() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}

TSClient::PlaceOrderFuture_t TSClient::placeOrder(const PlaceOrderRequest &order)
{
    Q_ASSERT(order.isValid());

    QNetworkRequest request = buildNetworkRequest(ENDPOINT_PLACE_ORDER);

    QByteArray postData = QJsonDocument(order.toJson()).toJson(QJsonDocument::Compact);

    qCDebug(TSClientLog) << "Placing order async";

    QPromise<PlaceOrderResult_t> promise;
    PlaceOrderFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, postData, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->post(request, postData);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(PlaceOrderResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isObject()) {
                            qCCritical(TSClientLog) << " : JSON is not an object";
                            promise.addResult(PlaceOrderResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        const PlaceOrderResult results = PlaceOrderResult(doc.object());

                        promise.addResult(PlaceOrderResult_t(results));
                        break;
                    }

                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": placeOrder(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(PlaceOrderResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": placeOrder(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(PlaceOrderResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent placeOrder() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}

TSClient::CancelOrderFuture_t TSClient::cancelOrder(const QString &orderID)
{
    Q_ASSERT(!orderID.isEmpty());
    Q_ASSERT(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QNetworkRequest request = buildNetworkRequest(QString(ENDPOINT_CANCEL_ORDER).arg(orderID));

    qCDebug(TSClientLog) << "Cancel order for orderID : " << orderID;

    QPromise<CancelOrderResult_t> promise;
    CancelOrderFuture_t future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(this, [this, request, promise = std::move(promise)]() mutable {

        QNetworkReply *reply = m_networkManager->deleteResource(request);
        Q_CHECK_PTR(reply);

        auto b = connect(reply, &QNetworkReply::finished, this,
            [this, reply, promise = std::move(promise)]() mutable{

                QByteArray rawData = reply->readAll();
    
                processNewAmountOfDataReceived(rawData.size());

                switch (reply->error())
                {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError) {
                            qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                            qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                            promise.addResult(CancelOrderResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        if (!doc.isObject()) {
                            qCCritical(TSClientLog) << " : JSON is not an object";
                            promise.addResult(CancelOrderResult_t(AsyncRequestError_e::ERROR));
                            break;
                        }

                        CancelOrderResult results = CancelOrderResult(doc.object());
                        
                        promise.addResult(CancelOrderResult_t(results));
                        break;
                    }
        
                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        qCCritical(TSClientLog) << ": cancelOrder(): Timeout with the reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(CancelOrderResult_t(AsyncRequestError_e::TIMEOUT));
                        break;
                    }

                    // other errors
                    default:
                    {
                        qCCritical(TSClientLog) << ": cancelOrder(): Error with reply: " << reply->errorString() << " : " << reply->error();
                        promise.addResult(CancelOrderResult_t(AsyncRequestError_e::ERROR));
                        break;
                    }
                };
    
                promise.finish();   // always finish exactly once
                reply->deleteLater();
            });
        Q_ASSERT(b);

        qCDebug(TSClientLog) << "Sent getAccounts() to Network Manager";

    }, Qt::QueuedConnection);

    return future;
}



void TSClient::demuxReceivedAsyncRequestReply(AsyncRequestType_t type, const QJsonDocument &doc, AsyncRequestID_t requestID, AsyncRequestStatus_e status)
{

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
