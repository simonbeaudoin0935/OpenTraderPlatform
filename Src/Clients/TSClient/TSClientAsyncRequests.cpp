#include <QJsonObject>
#include <QJsonArray>
#include <memory>

#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "DBClient.h"
#include "OrderEmulator.h"
#include "Stream/MockNetworkAccessManager.h"
#include "MarketData/Bars/TSBarTimestampNormalizer.h"

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
                             TSClient::BarSessionTemplate p_sessionTemplate,
                             const QDateTime& p_firstDate,
                             const QDateTime& p_lastDate)
    {
        QUrlQuery query;
        query.addQueryItem("interval", QString::number(p_interval));
        query.addQueryItem("unit", barUnitToString(p_unit));
        query.addQueryItem("sessiontemplate", barSessionTemplateToString(p_sessionTemplate));

        const bool hasDateRange = p_firstDate.isValid() && p_lastDate.isValid();
        if (hasDateRange)
        {
            query.addQueryItem("firstdate", p_firstDate.toString(Qt::ISODate));
            query.addQueryItem("lastdate", p_lastDate.toString(Qt::ISODate));
        }
        else
        {
            query.addQueryItem("barsback", QString::number(p_barsback));
        }

        return query;
    }

    double parseDouble(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isDouble())
        {
            return value.toDouble();
        }
        if (value.isString())
        {
            bool ok = false;
            const double parsed = value.toString().toDouble(&ok);
            return ok ? parsed : 0.0;
        }
        return 0.0;
    }

    quint64 parseUInt64(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isDouble())
        {
            return static_cast<quint64>(qMax<qint64>(0, value.toVariant().toLongLong()));
        }
        if (value.isString())
        {
            bool ok = false;
            const quint64 parsed = value.toString().toULongLong(&ok);
            return ok ? parsed : 0ULL;
        }
        return 0ULL;
    }

    QDateTime parseTimestamp(const QJsonObject& p_json, const int p_minuteIntervalSeconds)
    {
        return TSBarTimestampNormalizer::normalizeToCanonicalBarTimestamp(p_json.value("TimeStamp").toString(),
                                                                          p_minuteIntervalSeconds);
    }

    Bar::BarStatus parseBarStatus(const QJsonObject& p_json)
    {
        const QString status = p_json.value("BarStatus").toString();
        if (status == "Open")
        {
            return Bar::BarStatus::Open;
        }
        if (status == "Null")
        {
            return Bar::BarStatus::Null;
        }
        return Bar::BarStatus::Closed;
    }

    std::optional<Bar> parseBar(const QJsonObject& p_json, const int p_minuteIntervalSeconds)
    {
        const QDateTime ts = parseTimestamp(p_json, p_minuteIntervalSeconds);
        if (!ts.isValid())
        {
            return std::nullopt;
        }

        Bar bar(ts,
                static_cast<float>(parseDouble(p_json, "Open")),
                static_cast<float>(parseDouble(p_json, "High")),
                static_cast<float>(parseDouble(p_json, "Low")),
                static_cast<float>(parseDouble(p_json, "Close")),
                parseUInt64(p_json, "TotalVolume"),
                parseBarStatus(p_json));

        if (!bar.isValid())
        {
            return std::nullopt;
        }

        return bar;
    }
} // namespace

QFuture<std::expected<QVector<Quote>, TSClient::Error>> TSClient::getQuoteSnapshots(const QStringList& symbols)
{
    OBJ_ASSUME_FALSE(symbols.isEmpty());
    OBJ_ASSUME_TRUE(symbols.size() == 1); // current call-sites are single-symbol

    const QString symbol = symbols.at(0);

    DEBUG << "Fetching quote snapshot for symbol:" << symbol;

    QPromise<std::expected<QVector<Quote>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, symbol, promise = std::move(promise)]() mutable
        {
            QNetworkReply* reply =
                m_networkManager->get(buildNetworkRequest(QString(TSClientEndpoints::GET_QUOTE_SNAPSHOTS).arg(symbol)));
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        const QByteArray rawData = reply->readAll();
                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            const QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse quote snapshot JSON:" << parseError.errorString();
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            QJsonArray quotesArray;
                            if (doc.isObject() && doc.object().contains("Quotes"))
                            {
                                quotesArray = doc.object().value("Quotes").toArray();
                            }
                            else if (doc.isArray())
                            {
                                quotesArray = doc.array();
                            }

                            QVector<Quote> quotes;
                            for (const QJsonValue& value: quotesArray)
                            {
                                if (!value.isObject())
                                {
                                    continue;
                                }

                                Quote quote(value.toObject());
                                if (quote.isValid())
                                {
                                    quotes.push_back(quote);
                                }
                            }

                            promise.addResult(quotes);
                            break;
                        }

                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                            CRITICAL << "getQuoteSnapshots timeout:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;

                        default:
                            CRITICAL << "getQuoteSnapshots failed:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }

                        promise.finish();
                        reply->deleteLater();
                    });
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>
TSClient::getBars(const QString& symbol,
                  unsigned int interval,
                  BarUnit unit,
                  unsigned int barsback,
                  BarSessionTemplate sessionTemplate,
                  QDateTime firstDate,
                  QDateTime lastDate)
{
    OBJ_ASSUME_FALSE(symbol.isEmpty());
    OBJ_ASSUME_GTE(interval, 1u);
    OBJ_ASSUME_LTE(barsback, 57600u);

    if (firstDate.isValid() || lastDate.isValid())
    {
        OBJ_ASSUME_TRUE(firstDate.isValid());
        OBJ_ASSUME_TRUE(lastDate.isValid());
        OBJ_ASSUME_LT(firstDate, lastDate);
        barsback = 0;
    }
    const int minuteIntervalSeconds = (unit == BarUnit::Minute) ? static_cast<int>(interval) * 60 : 0;

    QUrlQuery query = buildBarsQuery(interval, unit, barsback, sessionTemplate, firstDate, lastDate);
    QNetworkRequest request = buildNetworkRequest(QString(TSClientEndpoints::GET_BARS).arg(symbol), query);

    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, request = std::move(request), promise = std::move(promise), minuteIntervalSeconds]() mutable
        {
            QNetworkReply* reply = m_networkManager->get(request);
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise), minuteIntervalSeconds]() mutable
                    {
                        const QByteArray rawData = reply->readAll();
                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            const QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse bars JSON:" << parseError.errorString();
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            QJsonArray barsArray;
                            if (doc.isObject() && doc.object().contains("Bars"))
                            {
                                barsArray = doc.object().value("Bars").toArray();
                            }
                            else if (doc.isArray())
                            {
                                barsArray = doc.array();
                            }
                            else
                            {
                                CRITICAL << "Unexpected bars payload shape";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            auto bars = std::make_shared<QVector<Bar>>();
                            bars->reserve(barsArray.size());
                            for (const QJsonValue& value: barsArray)
                            {
                                if (!value.isObject())
                                {
                                    continue;
                                }
                                std::optional<Bar> parsed = parseBar(value.toObject(), minuteIntervalSeconds);
                                if (parsed.has_value())
                                {
                                    bars->push_back(parsed.value());
                                }
                            }

                            promise.addResult(bars);
                            break;
                        }

                        case QNetworkReply::ContentNotFoundError:
                            promise.addResult(std::make_shared<QVector<Bar>>());
                            break;

                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                            CRITICAL << "getBars timeout:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;

                        case QNetworkReply::ProtocolInvalidOperationError:
                        {
                            QJsonParseError parseError;
                            const QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "getBars failed (protocol invalid op) and body is not JSON:"
                                         << parseError.errorString() << "raw=" << rawData;
                                promise.addResult(std::unexpected(Error::Other));
                                break;
                            }

                            const QJsonObject obj = doc.object();
                            const QString error = obj.value("Error").toString();
                            const QString message = obj.value("Message").toString();
                            const QString normalizedMessage = message.trimmed().toLower();
                            const bool isInvalidSymbol = error.compare("BadRequest", Qt::CaseInsensitive) == 0 &&
                                                         (normalizedMessage.contains("invalid symbol") ||
                                                          normalizedMessage.contains("symbol cannot be found"));

                            if (isInvalidSymbol)
                            {
                                WARNING << "getBars rejected symbol as invalid:" << message;
                                promise.addResult(std::unexpected(Error::RejectedByValidator));
                                break;
                            }

                            CRITICAL << "getBars failed (protocol invalid op):" << reply->errorString()
                                     << reply->error() << "raw=" << rawData;
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }

                        default:
                            CRITICAL << "getBars failed:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }

                        promise.finish();
                        reply->deleteLater();
                    });
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<QVector<Account>, TSClient::Error>> TSClient::getAccounts()
{
    if (m_shuttingDown.load(std::memory_order_acquire))
    {
        WARNING << "Dropping getAccounts() request because TSClient is shutting down";
        QPromise<std::expected<QVector<Account>, TSClient::Error>> promise;
        auto future = promise.future();
        promise.start();
        promise.addResult(std::unexpected(Error::Other));
        promise.finish();
        return future;
    }

    DEBUG << "Fetching accounts";

    QPromise<std::expected<QVector<Account>, TSClient::Error>> promise;
    auto future = promise.future();

    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, promise = std::move(promise)]() mutable
        {
            if (m_shuttingDown.load(std::memory_order_acquire))
            {
                DEBUG << "Dropping getAccounts() invoke because TSClient is shutting down";
                promise.addResult(std::unexpected(Error::Other));
                promise.finish();
                return;
            }

            // Use mock network manager in replay mode for simulated account
            QNetworkAccessManager* manager =
                (m_mode == Mode::Replay && m_mockNetworkManager) ? m_mockNetworkManager : m_networkManager;
            if (manager == nullptr)
            {
                CRITICAL << "getAccounts(): Network manager unavailable";
                promise.addResult(std::unexpected(Error::Other));
                promise.finish();
                return;
            }

            QNetworkReply* reply = manager->get(buildNetworkRequest(TSClientEndpoints::GET_ACCOUNTS));
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        if (m_shuttingDown.load(std::memory_order_acquire))
                        {
                            DEBUG << "Dropping getAccounts() reply because TSClient is shutting down";
                            promise.addResult(std::unexpected(Error::Other));
                            promise.finish();
                            reply->deleteLater();
                            return;
                        }

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

                            // Credentials are likely invalid/expired.
                            // Transition to unauthenticated state and let UI trigger re-authentication dialog.
                            const bool wasAuthenticated = m_authenticated;
                            m_authenticated = false;
                            if (wasAuthenticated)
                            {
                                emit authStateChanged(false,
                                                      AuthStateReason::TokenExpired,
                                                      "TradeStation authentication required");
                            }

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
    if (m_mode == Mode::Replay)
    {
        account = OrderEmulator::getSimulatedAccountID();
    }

    QMetaObject::invokeMethod(
        this,
        [this, account, promise = std::move(promise)]() mutable
        {
            // Use mock network manager in replay mode for simulated balance
            QNetworkAccessManager* manager =
                (m_mode == Mode::Replay && m_mockNetworkManager) ? m_mockNetworkManager : m_networkManager;

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

QFuture<std::expected<QVector<OrderRoute>, TSClient::Error>> TSClient::getOrderRoutes()
{
    if (m_shuttingDown.load(std::memory_order_acquire))
    {
        WARNING << "Dropping getOrderRoutes() request because TSClient is shutting down";
        QPromise<std::expected<QVector<OrderRoute>, TSClient::Error>> promise;
        auto future = promise.future();
        promise.start();
        promise.addResult(std::unexpected(Error::Other));
        promise.finish();
        return future;
    }

    QPromise<std::expected<QVector<OrderRoute>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, promise = std::move(promise)]() mutable
        {
            if (m_shuttingDown.load(std::memory_order_acquire))
            {
                DEBUG << "Dropping getOrderRoutes() invoke because TSClient is shutting down";
                promise.addResult(std::unexpected(Error::Other));
                promise.finish();
                return;
            }

            QNetworkAccessManager* manager =
                (m_mode == Mode::Replay && m_mockNetworkManager) ? m_mockNetworkManager : m_networkManager;
            if (manager == nullptr)
            {
                CRITICAL << "getOrderRoutes(): Network manager unavailable";
                promise.addResult(std::unexpected(Error::Other));
                promise.finish();
                return;
            }

            QNetworkReply* reply = manager->get(buildNetworkRequest(TSClientEndpoints::GET_ORDER_ROUTES));
            Q_CHECK_PTR(reply);

            connect(reply,
                    &QNetworkReply::finished,
                    this,
                    [this, reply, promise = std::move(promise)]() mutable
                    {
                        if (m_shuttingDown.load(std::memory_order_acquire))
                        {
                            DEBUG << "Dropping getOrderRoutes() reply because TSClient is shutting down";
                            promise.addResult(std::unexpected(Error::Other));
                            promise.finish();
                            reply->deleteLater();
                            return;
                        }

                        const QByteArray rawData = reply->readAll();
                        processNewAmountOfDataReceived(rawData.size());

                        switch (reply->error())
                        {
                        case QNetworkReply::NoError:
                        {
                            QJsonParseError parseError;
                            const QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
                            if (parseError.error != QJsonParseError::NoError)
                            {
                                CRITICAL << "Failed to parse order routes JSON:" << parseError.errorString();
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            QJsonArray routesArray;
                            if (doc.isObject() && doc.object().contains("Routes"))
                            {
                                routesArray = doc.object().value("Routes").toArray();
                            }
                            else if (doc.isArray())
                            {
                                routesArray = doc.array();
                            }
                            else
                            {
                                CRITICAL << "Unexpected order routes payload shape";
                                promise.addResult(std::unexpected(Error::JSONError));
                                break;
                            }

                            QVector<OrderRoute> routes;
                            routes.reserve(routesArray.size());
                            for (const QJsonValue& value: routesArray)
                            {
                                if (!value.isObject())
                                {
                                    continue;
                                }

                                const OrderRoute route(value.toObject());
                                if (route.isValid())
                                {
                                    routes.push_back(route);
                                }
                            }

                            INFO << "getOrderRoutes() succeeded with" << routes.size() << "routes";
                            for (const OrderRoute& route: routes)
                            {
                                INFO << "Route:" << "id=" << route.getId() << "name=" << route.getName()
                                     << "assetTypes=" << route.getAssetTypes();
                            }

                            promise.addResult(routes);
                            break;
                        }

                        case QNetworkReply::HostNotFoundError:
                        case QNetworkReply::UnknownNetworkError:
                            CRITICAL << "getOrderRoutes() timeout:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Timeout));
                            break;

                        default:
                            CRITICAL << "getOrderRoutes() failed:" << reply->errorString() << reply->error();
                            promise.addResult(std::unexpected(Error::Other));
                            break;
                        }

                        promise.finish();
                        reply->deleteLater();
                    });
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>> TSClient::placeOrder(const PlaceOrderRequest& order)
{
    DEBUG << "Placing order async";

    OBJ_ASSUME_TRUE(order.isValid());

    if (m_mode == Mode::Replay && DBClient::getInstance()->getPlaybackState() != Playback::State::Playing)
    {
        WARNING << "Rejecting replay order because playback is not running:" << order.getSymbol();
        QPromise<std::expected<PlaceOrderResult, TSClient::Error>> promise;
        auto future = promise.future();
        promise.start();
        promise.addResult(std::unexpected(Error::RejectedByValidator));
        promise.finish();
        return future;
    }

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
            QNetworkAccessManager* manager =
                (m_mode == Mode::Replay && m_mockNetworkManager) ? m_mockNetworkManager : m_networkManager;

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
                            break;
                        }

                        promise.addResult(std::unexpected(Error::Other));
                        break;
                    }

                    case QNetworkReply::ContentAccessDenied:
                    case QNetworkReply::AuthenticationRequiredError:
                    {
                        CRITICAL << ": placeOrder(): Access denied or authentication error with reply: "
                                 << reply->errorString() << " : " << reply->error() << " : " << rawData;
                        promise.addResult(std::unexpected(Error::Other));
                        break;
                    }

                    // other errors
                    default:
                    {
                        CRITICAL << ": placeOrder(): Error with reply: " << reply->errorString() << " : "
                                 << reply->error() << " : " << rawData;
                        promise.addResult(std::unexpected(Error::Other));

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
            QNetworkAccessManager* manager =
                (m_mode == Mode::Replay && m_mockNetworkManager) ? m_mockNetworkManager : m_networkManager;

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
