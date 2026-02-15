#include <QJsonObject>
#include <QJsonArray>
#include <memory>

#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "Stream/MockNetworkAccessManager.h"

#define LOGGING_CATEGORY TSClientLog

QFuture<std::expected<QVector<Account>, TSClient::Error>> TSClient::getAccounts()
{
    DEBUG << "Fetching accounts";

    QPromise<std::expected<QVector<Account>, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, promise = std::move(promise)]() mutable
        {
            // Use mock network manager in replay mode for simulated account
            QNetworkAccessManager* manager = (m_mode == Mode::Replay && m_mockNetworkManager)
                                                  ? m_mockNetworkManager
                                                  : m_networkManager;

            QNetworkReply* reply = manager->get(buildNetworkRequest(TSClientEndpoints::GET_ACCOUNTS));
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        QByteArray rawData = reply->readAll();

                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        // Happy path
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse JSON:" << parseError.errorString();
                                CRITICAL << "Content of the bad data : " << rawData;
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonValue val = doc["Accounts"];

                            if (val == QJsonValue::Undefined)
                            {
                                CRITICAL << " : 'Accounts' field is missing in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonArray accountsArray = val.toArray();

                            if (accountsArray.isEmpty())
                            {
                                CRITICAL << " : 'Accounts' array is empty in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            // Happiest path
                            QVector<Account> results;
                            for (const QJsonValue& json: accountsArray)
                            {
                                results.push_back(Account(json.toObject()));
                            }

                            DEBUG << "Fetched" << results.size() << "accounts";
                            promise.addResult(results);
                            break;
                        }

                        // IMPORTANT EDGE CASE: In getBars API, its possible to receive this when theres just no bars in the range, its not really an error
                        case QNetworkReply::ContentNotFoundError:
                        {
                            QVector<Account> results; // empty vector
                            promise.addResult(results);
                            break;
                        }

                        // timeout
                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                        {
                            CRITICAL << ": getAccounts(): Timeout with the reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;
                        }

                        case QNetworkReply::AuthenticationRequiredError:
                        {
                            CRITICAL << ": getAccounts(): Authentication error with the reply: " << reply->errorString()
                                     << " : " << reply->error();

                            // Here the credentials are likely invalid or expired.
                            // Need to pop a dialog to the user to re-authenticate.


                            Q_UNREACHABLE();

                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }

                        // other errors
                        default:
                        {
                            CRITICAL << ": getAccounts(): Error with reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }
                        };

                        promise.finish(); // always finish exactly once
                        reply->deleteLater();
                    });

            DEBUG << "Sent getAccounts() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<QVector<Balance>, TSClient::Error>> TSClient::getBalances(const QStringList& accounts)
{
    //DEBUG << "Fetching Balances";

    OBJ_ASSUME_FALSE(accounts.isEmpty());
    OBJ_ASSUME_TRUE(accounts.size() == 1); // FIXME For now only single account is supported

    QPromise<std::expected<QVector<Balance>, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QString account = accounts.at(0);

    QMetaObject::invokeMethod(
        this,
        [this, account, promise = std::move(promise)]() mutable
        {
            // Use mock network manager in replay mode for simulated balance
            QNetworkAccessManager* manager = (m_mode == Mode::Replay && m_mockNetworkManager)
                                                  ? m_mockNetworkManager
                                                  : m_networkManager;

            QNetworkReply* reply =
                manager->get(buildNetworkRequest(QString(TSClientEndpoints::GET_BALANCES).arg(account)));
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        QByteArray rawData = reply->readAll();

                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        // Happy path
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                            if (parseError.error != QJsonParseError::NoError)
                            {
                                qCCritical(TSClientLog) << "Failed to parse JSON:" << parseError.errorString();
                                qCCritical(TSClientLog) << "Content of the bad data : " << rawData;
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonValue val = doc["Balances"];

                            if (val == QJsonValue::Undefined)
                            {
                                qCCritical(TSClientLog) << " : 'Balances' field is missing in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonArray balancesArray = val.toArray();

                            if (balancesArray.isEmpty())
                            {
                                qCCritical(TSClientLog) << " : 'Balances' array is empty in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            // Happiest path
                            QVector<Balance> results;
                            for (const QJsonValue& json: balancesArray)
                            {
                                results.push_back(Balance(json.toObject()));
                            }

                            //qCDebug(TSClientLog) << "Fetched" << results.size() << "account balances";
                            promise.addResult(results);
                            break;
                        }

                        // timeout
                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                        {
                            qCCritical(TSClientLog)
                                << ": getBalances(): Timeout with the reply: " << reply->errorString() << " : "
                                << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;
                        }

                        // other errors
                        default:
                        {
                            qCCritical(TSClientLog) << ": getBalances(): Error with reply: " << reply->errorString()
                                                    << " : " << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }
                        };

                        promise.finish(); // always finish exactly once
                        reply->deleteLater();
                    });

            //DEBUG << "Sent getBalances() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>
TSClient::getBars(const QString& symbol,
                  unsigned int interval,
                  Bar::BarUnit unit,
                  unsigned int barsback,
                  Bar::BarSessionTemplate sessionTemplate,
                  QDateTime firstDate,
                  QDateTime lastDate)
{
    DEBUG << "Fetching Bars for symbols : " << symbol;

    OBJ_ASSUME_FALSE(symbol.isEmpty());
    // Interval that each bar will consist of - for minute bars, the number of minutes aggregated in a single bar. For bar units other than minute, value must be 1.
    if (unit == Bar::BarUnit::Minute)
    {
        OBJ_ASSUME_TRUE(interval >= 1);
    }
    else
    {
        OBJ_ASSUME_TRUE(interval == 1);
    }
    OBJ_ASSUME_TRUE(barsback <= 57600);
    if (barsback > 0)
        OBJ_ASSUME_TRUE(firstDate == QDateTime());

    QUrlQuery query = Bar::buildUrlQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);

    QNetworkRequest request = buildNetworkRequest(QString(TSClientEndpoints::GET_BARS).arg(symbol), query);

    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, request = std::move(request), promise = std::move(promise)]() mutable
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        QByteArray rawData = reply->readAll();

                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        // Happy path
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse JSON:" << parseError.errorString();
                                CRITICAL << "Content of the bad data : " << rawData;
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonValue val = doc["Bars"];

                            if (val == QJsonValue::Undefined)
                            {
                                CRITICAL << " : 'Bars' field is missing in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonArray barsArray = val.toArray();
                            if (barsArray.isEmpty())
                            {
                                CRITICAL << " : 'Bars' array is empty in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            // Happiest path
                            std::shared_ptr<QVector<Bar>> results = std::make_shared<QVector<Bar>>();
                            results->reserve(barsArray.size());

                            for (const QJsonValue& json: barsArray)
                            {
                                results->push_back(Bar(json.toObject()));
                            }

                            promise.addResult(results);
                            break;
                        }

                        // timeout
                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                        {
                            CRITICAL << ": getBars(): Timeout with the reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;
                        }

                        case QNetworkReply::ContentNotFoundError:
                        {
                            // This is not really an error, it just means there are no bars in the requested range
                            // empty vector
                            std::shared_ptr<QVector<Bar>> results = std::make_shared<QVector<Bar>>();

                            promise.addResult(results);
                            break;
                        }

                        // other errors
                        default:
                        {
                            CRITICAL << ": getBars(): Error with reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }
                        };

                        promise.finish(); // always finish exactly once
                        reply->deleteLater();
                    });

            DEBUG << "Sent getBars() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<QVector<Quote>, TSClient::Error>> TSClient::getQuoteSnapshots(const QStringList& symbols)
{
    OBJ_ASSUME_FALSE(symbols.isEmpty());
    OBJ_ASSUME_TRUE(symbols.size() == 1); // For now only single account is supported
    QString symbol = symbols.at(0);

    DEBUG << "Fetching quotes for symbols : " << symbols;

    QNetworkRequest request = buildNetworkRequest(QString(TSClientEndpoints::GET_QUOTE_SNAPSHOTS).arg(symbol));

    QPromise<std::expected<QVector<Quote>, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, request = std::move(request), promise = std::move(promise)]() mutable
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        QByteArray rawData = reply->readAll();

                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        // Happy path
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse JSON:" << parseError.errorString();
                                CRITICAL << "Content of the bad data : " << rawData;
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            if (!doc.isArray())
                            {
                                CRITICAL << " : JSON is not an array";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            if (doc.array().isEmpty())
                            {
                                CRITICAL << " : JSON is an empty array";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonValue val = doc["Quotes"];

                            if (val == QJsonValue::Undefined)
                            {
                                CRITICAL << " : 'Quotes' field is missing in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            const QJsonArray quotesArray = val.toArray();
                            if (quotesArray.isEmpty())
                            {
                                CRITICAL << " : 'Quotes' array is empty in the response";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            // Happiest path
                            QVector<Quote> results;
                            for (const QJsonValue& json: quotesArray)
                            {
                                results.push_back(Quote(json.toObject()));
                            }

                            promise.addResult(results);
                            break;
                        }

                        // timeout
                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                        {
                            CRITICAL << ": getQuoteSnapshots(): Timeout with the reply: " << reply->errorString()
                                     << " : " << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;
                        }

                        // other errors
                        default:
                        {
                            CRITICAL << ": getQuoteSnapshots(): Error with reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }
                        };

                        promise.finish(); // always finish exactly once
                        reply->deleteLater();
                    });

            DEBUG << "Sent getBars() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>> TSClient::placeOrder(const PlaceOrderRequest& order)
{
    DEBUG << "Placing order async";

    OBJ_ASSUME_TRUE(order.isValid());

    QNetworkRequest request = buildNetworkRequest(TSClientEndpoints::PLACE_ORDER);

    QByteArray postData = QJsonDocument(order.toJson()).toJson(QJsonDocument::Compact);

    QPromise<std::expected<PlaceOrderResult, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, request = std::move(request), postData = std::move(postData), promise = std::move(promise)]() mutable
        {
            // Use mock network manager in replay mode for order emulation
            QNetworkAccessManager* manager = (m_mode == Mode::Replay && m_mockNetworkManager)
                                                  ? m_mockNetworkManager
                                                  : m_networkManager;

            QNetworkReply* reply = manager->post(request, postData);
            Q_CHECK_PTR(reply);

            connect(
                reply,
                &QNetworkReply::finished,
                this,
                [this, reply, promise = std::move(promise)]() mutable
                {
                    QByteArray rawData = reply->readAll();

                    processNewAmountOfDataReceived(rawData.size());

                    switch (reply->error())
                    {
                    // Happy path
                    case QNetworkReply::NoError:
                    {
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError)
                        {
                            CRITICAL << "Failed to parse JSON:" << parseError.errorString();
                            CRITICAL << "Content of the bad data : " << rawData;
                            promise.addResult(std::unexpected(Error::JSONError));
                            break;
                        }

                        if (!doc.isObject())
                        {
                            CRITICAL << " : JSON is not an object";
                            promise.addResult(std::unexpected(Error::JSONError));
                            break;
                        }

                        const PlaceOrderResult results = PlaceOrderResult(doc.object());

                        promise.addResult(results);
                        break;
                    }

                    // timeout
                    case QNetworkReply::HostNotFoundError:
                    case QNetworkReply::UnknownNetworkError:
                    {
                        CRITICAL << ": placeOrder(): Timeout with the reply: " << reply->errorString() << " : "
                                 << reply->error();
                        promise.addResult(std::unexpected(Error::Timeout));
                        break;
                    }

                    case QNetworkReply::ProtocolInvalidOperationError:
                    {
                        // This error code is returned by TSClient when the order request is syntactically correct but semantically invalid (e.g. trying to buy a stock that doesn't exist, or missing required fields, etc). In this case, TSClient's response body should contain details about what exactly was wrong with the order request, so we should treat this as a successful response and parse the error details from the JSON instead of treating it as a generic error.
                        QJsonParseError parseError;
                        QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                        if (parseError.error != QJsonParseError::NoError)
                        {
                            CRITICAL << "Failed to parse JSON for order validation error response:"
                                     << parseError.errorString();
                            CRITICAL << "Content of the bad data : " << rawData;
                            promise.addResult(std::unexpected(Error::JSONError));
                            break;
                        }

                        const QJsonObject obj = doc.object();

                        const QString error = obj["Error"].toString();
                        const QString message = obj["Message"].toString();

                        CRITICAL << "Order validation error - Error: " << error << ", Message: " << message;

                        if (error == "BadRequest")
                        {
                            // This means the order request was malformed in some way that it couldn't be processed at all, and we should treat this as a generic error instead of trying to parse error details from the JSON because we can't rely on the structure of the JSON in this case
                            CRITICAL
                                << "Bad request error indicates a malformed order request that couldn't be processed at all, treating as generic error";
                            promise.addResult(std::unexpected(Error::Other));

                            Q_UNREACHABLE();
                            break;
                        }
                        break;
                    }

                    // other errors
                    default:
                    {
                        CRITICAL << ": placeOrder(): Error with reply: " << reply->errorString() << " : "
                                 << reply->error() << " : " << rawData;
                        promise.addResult(std::unexpected(Error::Other));

                        // Means our request is malformed
                        Q_UNREACHABLE(); // We should never hit this because placeOrder should return error details in the JSON response even in cases of order validation errors, so TSClient should never treat any response as an outright failure. If we do hit this, it means we got an unexpected error code that we haven't accounted for, and we should investigate and update our code to handle it properly instead of just treating it as a generic error.

                        break;
                    }
                    };

                    promise.finish(); // always finish exactly once
                    reply->deleteLater();
                });

            DEBUG << "Sent placeOrder() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<CancelOrderResult, TSClient::Error>> TSClient::cancelOrder(const QString& orderID)
{
    DEBUG << "Cancel order for orderID : " << orderID;

    OBJ_ASSUME_FALSE(orderID.isEmpty());
    OBJ_ASSUME_TRUE(QRegularExpression("^[0-9]+$").match(orderID).hasMatch());

    QNetworkRequest request = buildNetworkRequest(QString(TSClientEndpoints::CANCEL_ORDER).arg(orderID));

    QPromise<std::expected<CancelOrderResult, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, request = std::move(request), promise = std::move(promise)]() mutable
        {
            // Use mock network manager in replay mode for order emulation
            QNetworkAccessManager* manager = (m_mode == Mode::Replay && m_mockNetworkManager)
                                                  ? m_mockNetworkManager
                                                  : m_networkManager;

            QNetworkReply* reply = manager->deleteResource(request);
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        QByteArray rawData = reply->readAll();

                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        // Happy path
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse JSON:" << parseError.errorString();
                                CRITICAL << "Content of the bad data : " << rawData;
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            if (!doc.isObject())
                            {
                                CRITICAL << " : JSON is not an object";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            promise.addResult(CancelOrderResult(doc.object()));
                            break;
                        }

                        // timeout
                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                        {
                            CRITICAL << ": cancelOrder(): Timeout with the reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;
                        }

                        // other errors
                        default:
                        {
                            CRITICAL << ": cancelOrder(): Error with reply: " << reply->errorString() << " : "
                                     << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }
                        };

                        promise.finish(); // always finish exactly once
                        reply->deleteLater();
                    });

            DEBUG << "Sent cancelOrder() to Network Manager";
        },
        Qt::QueuedConnection);

    return future;
}