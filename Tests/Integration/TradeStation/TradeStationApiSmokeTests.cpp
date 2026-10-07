#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QScopeGuard>
#include <QElapsedTimer>
#include <QSet>
#include <cmath>
#include <memory>

#include "AuthToken.h"
#include "ClientToken.h"
#include "MainApp.h"
#include "SecureStorage.h"
#include "Settings.h"
#include "TSClient.h"
#include "TradeStationOrderTestPolicy.h"

class TradeStationApiSmokeTests : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        const QString account = qEnvironmentVariable(TradeStationApiTestConstants::ACCOUNT_ENV);
        QVERIFY2(account.startsWith("SIM") && account.size() > 3,
                 "Set OTP_TEST_TRADESTATION_ACCOUNT to the dedicated Simulation account.");
        QVERIFY(m_directory.isValid());

        QCoreApplication::setOrganizationName("OpenTraderPlatform");
        QCoreApplication::setApplicationName("TradeStationApiSmokeTests");
        // Native settings hold the isolated backend preference; Ini settings retain token metadata.
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
        QVERIFY2(SecureStorage::configureBackend(SecureStorage::Backend::OSKeyring),
                 "Could not configure the isolated OS-keyring test preference.");
        QCOMPARE(SecureStorage::activeBackend(), SecureStorage::Backend::OSKeyring);

        m_settings = std::make_unique<QSettings>(m_directory.path() + "/AppState.ini", QSettings::IniFormat);
        appStateSettings = m_settings.get();
        MainApp::setTradingMode(TradingMode::Sim);
        QCOMPARE(MainApp::getTradingMode(), TradingMode::Sim);

        QVERIFY2(ClientToken::loadFromSettings().isValid(),
                 "Valid TradeStation client credentials are missing from the OS keyring.");
        QVERIFY2(
            AuthToken::loadFromSettings().isValid(),
            "Valid stored TradeStation tokens or OS-keyring token metadata are missing. Log in using OS keyring first.");

        auto* client = TSClient::getInstance();
        m_clientCreated = true;
        QCOMPARE(client->getApiBaseUrl().host(), QString(TSClientHosts::SIM_HOST));
        QCOMPARE(client->getMode(), TSClient::Mode::Live);
        bool ready = false;
        bool rejected = false;
        QObject authContext;
        const auto connection = connect(
            client,
            &TSClient::authStateChanged,
            &authContext,
            [&](bool p_authenticated, TSClient::AuthStateReason p_reason, const QString&)
            {
                ready = p_authenticated;
                rejected = p_reason == TSClient::AuthStateReason::TokenExpired ||
                           p_reason == TSClient::AuthStateReason::AuthFailed;
            },
            Qt::QueuedConnection);
        QVERIFY(connection);
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(ready || rejected, TradeStationApiTestConstants::AUTH_TIMEOUT_MS);
        QVERIFY2(!rejected, "TradeStation authentication/refresh was rejected or token persistence failed.");
        QVERIFY(ready);

        const auto accounts = client->getAccounts();
        QTRY_VERIFY_WITH_TIMEOUT(accounts.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        const auto result = accounts.result();
        QVERIFY2(result.has_value(), "Simulation account discovery failed; inspect TSClient's error category.");
        bool found = false;
        for (const auto& returnedAccount: result.value())
        {
            if (returnedAccount.getAccountId() == account)
            {
                found = true;
                QVERIFY(returnedAccount.isValid());
            }
        }
        QVERIFY2(found, "The dedicated paper account was not returned by the Simulation API.");
        m_account = account;
    }

    void discoversSimulationAccount()
    {
        QVERIFY(!m_account.isEmpty());
    }

    void readsAccountBalances()
    {
        auto future = TSClient::getInstance()->getBalances({m_account});
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        const auto result = future.result();
        QVERIFY2(result.has_value(), "Simulation balance request failed.");
        QCOMPARE(result->size(), qsizetype(1));
        const Balance& balance = result->first();
        QCOMPARE(balance.getAccountID(), m_account);
        QVERIFY(std::isfinite(balance.getBuyingPower()));
        QVERIFY(std::isfinite(balance.getCashBalance()));
        QVERIFY(std::isfinite(balance.getEquity()));
    }

    void readsQuoteSnapshot()
    {
        auto future = TSClient::getInstance()->getQuoteSnapshots({"SPY"});
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        const auto result = future.result();
        QVERIFY2(result.has_value(), "Simulation quote snapshot request failed.");
        QCOMPARE(result->size(), qsizetype(1));
        const Quote& quote = result->first();
        QCOMPARE(quote.getSymbol(), QString("SPY"));
        QVERIFY(quote.isValid());
        QVERIFY(quote.getError().isEmpty());
        QVERIFY(std::isfinite(quote.getLast()) && quote.getLast() > 0.0);
        QVERIFY(std::isfinite(quote.getBid()) && quote.getBid() >= 0.0);
        QVERIFY(std::isfinite(quote.getAsk()) && quote.getAsk() >= 0.0);
    }

    void readsHistoricalRange()
    {
        auto* client = TSClient::getInstance();
        // Select a completed trading date from daily data rather than guessing holidays.
        auto daily = client->getBars("SPY", 1, TSClient::BarUnit::Daily, 5);
        QTRY_VERIFY_WITH_TIMEOUT(daily.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(daily.result().has_value());
        const auto days = daily.result().value();
        QVERIFY(days && !days->isEmpty());
        QDate tradingDay;
        const QDate today = QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE).date();
        for (const Bar& bar: *days)
        {
            if (bar.getBarStatus() == Bar::BarStatus::Closed && bar.getTimeStamp().date() < today &&
                (!tradingDay.isValid() || tradingDay < bar.getTimeStamp().date()))
                tradingDay = bar.getTimeStamp().date();
        }
        QVERIFY(tradingDay.isValid());
        const QDateTime first(tradingDay, QTime(9, 30), TradingHours::MARKET_TIMEZONE);
        const QDateTime last(tradingDay, QTime(10, 0), TradingHours::MARKET_TIMEZONE);
        auto future = client->getBars("SPY",
                                      1,
                                      TSClient::BarUnit::Minute,
                                      1,
                                      TSClient::BarSessionTemplate::USEQ24Hour,
                                      first,
                                      last);
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(future.result().has_value());
        const auto bars = future.result().value();
        QVERIFY(bars && !bars->isEmpty());
        QDateTime previous;
        for (const Bar& bar: *bars)
        {
            QVERIFY(bar.getTimeStamp() >= first && bar.getTimeStamp() <= last);
            QVERIFY(!previous.isValid() || previous < bar.getTimeStamp());
            QCOMPARE(bar.getTimeStamp().timeZone(), TradingHours::MARKET_TIMEZONE);
            QCOMPARE(bar.getBarStatus(), Bar::BarStatus::Closed);
            QVERIFY(std::isfinite(bar.getOpen()) && std::isfinite(bar.getHigh()) && std::isfinite(bar.getLow()) &&
                    std::isfinite(bar.getClose()));
            QVERIFY(bar.getLow() <= bar.getOpen() && bar.getOpen() <= bar.getHigh());
            QVERIFY(bar.getLow() <= bar.getClose() && bar.getClose() <= bar.getHigh());
            previous = bar.getTimeStamp();
        }
    }

    void reportsInvalidSymbol()
    {
        auto future = TSClient::getInstance()->getBars("OTP_INVALID_SYMBOL_987654321");
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        const auto result = future.result();
        QVERIFY2(!result.has_value(), "An invalid symbol must surface an error, not success-shaped empty bars.");
        QCOMPARE(result.error(), TSClient::Error::RejectedByValidator);
    }

    void receivesPositionSnapshotAndCloses()
    {
        auto* client = TSClient::getInstance();
        QPointer<StreamPositions> stream;
        bool snapshotComplete = false;
        bool closed = false;
        bool destroyed = false;
        QVector<Position> positions;
        QObject context;
        QVERIFY(QMetaObject::invokeMethod(
            client,
            [&]()
            {
                stream = client->openStreamPositions(m_account);
                if (stream.isNull())
                    return;
                connect(
                    stream,
                    &Stream::endSnapshotReceived,
                    &context,
                    [&]() { snapshotComplete = true; },
                    Qt::QueuedConnection);
                connect(stream, &Stream::streamClosed, &context, [&]() { closed = true; }, Qt::QueuedConnection);
                connect(
                    stream,
                    &StreamPositions::newPositionReceived,
                    &context,
                    [&](const Position& p_position) { positions.append(p_position); },
                    Qt::QueuedConnection);
                connect(stream, &QObject::destroyed, &context, [&]() { destroyed = true; }, Qt::QueuedConnection);
            },
            Qt::BlockingQueuedConnection));
        const auto cleanup = qScopeGuard([&]() { closeOwnedStream(client, stream); });
        QVERIFY(stream);
        QTRY_VERIFY_WITH_TIMEOUT(snapshotComplete || closed, TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY2(snapshotComplete && !closed, "Positions stream closed before completing its snapshot.");
        // An empty position snapshot is a valid flat account.
        for (const Position& position: positions)
        {
            QCOMPARE(position.getAccountID(), m_account);
            QVERIFY(position.isValid());
        }
        closeOwnedStream(client, stream);
        QTRY_VERIFY_WITH_TIMEOUT(destroyed, 5000);
    }

    void receivesQuoteStreamAndCloses()
    {
        auto* client = TSClient::getInstance();
        QPointer<StreamQuote> stream;
        bool closed = false;
        bool destroyed = false;
        QVector<Quote> quotes;
        QObject context;
        QVERIFY(QMetaObject::invokeMethod(
            client,
            [&]()
            {
                stream = client->openStreamQuote({"SPY"});
                if (stream.isNull())
                    return;
                connect(
                    stream,
                    &StreamQuote::newQuoteReceived,
                    &context,
                    [&](const Quote& p_quote) { quotes.append(p_quote); },
                    Qt::QueuedConnection);
                connect(stream, &Stream::streamClosed, &context, [&]() { closed = true; }, Qt::QueuedConnection);
                connect(stream, &QObject::destroyed, &context, [&]() { destroyed = true; }, Qt::QueuedConnection);
            },
            Qt::BlockingQueuedConnection));
        const auto cleanup = qScopeGuard([&]() { closeOwnedStream(client, stream); });
        QVERIFY(stream);
        QTRY_VERIFY_WITH_TIMEOUT(!quotes.isEmpty() || closed, TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY2(!closed && !quotes.isEmpty(), "Quote stream failed to deliver an initial quote.");
        QCOMPARE(quotes.first().getSymbol(), QString("SPY"));
        QVERIFY(quotes.first().isValid());
        closeOwnedStream(client, stream);
        QTRY_VERIFY_WITH_TIMEOUT(destroyed, 5000);
    }

    void paperOrderLifecycle()
    {
        if (!QCoreApplication::arguments().contains("paperOrderLifecycle") ||
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_OPT_IN_ENV) != "1")
            QSKIP("Select paperOrderLifecycle explicitly with the separate order opt-in.");
        const auto configuration = orderConfiguration();
        QVERIFY2(configuration.has_value(), configuration.has_value() ? "" : qPrintable(configuration.error()));
        auto* client = TSClient::getInstance();
        QCOMPARE(MainApp::getTradingMode(), TradingMode::Sim);
        QCOMPARE(client->getMode(), TSClient::Mode::Live);
        QVERIFY(TradeStationOrderTestPolicy::isSimulationEndpoint(client->getApiBaseUrl()));
        QCOMPARE(m_account, configuration->account);

        auto quoteFuture = client->getQuoteSnapshots({configuration->symbol});
        QTRY_VERIFY_WITH_TIMEOUT(quoteFuture.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(quoteFuture.result().has_value());
        const auto quotes = quoteFuture.result().value();
        QCOMPARE(quotes.size(), qsizetype(1));
        QCOMPARE(quotes.first().getSymbol(), configuration->symbol);
        QVERIFY(std::isfinite(quotes.first().getBid()) && quotes.first().getBid() > 0.0);
        QVERIFY2(configuration->limitPrice < quotes.first().getBid(),
                 "The explicit buy limit must be below the current bid; fills are still possible.");

        QVERIFY(QMetaObject::invokeMethod(
            client,
            [&]()
            {
                m_orderStream = client->openStreamOrders(m_account);
                if (!m_orderStream)
                    return;
                connect(
                    m_orderStream,
                    &Stream::endSnapshotReceived,
                    this,
                    [this]() { m_orderSnapshotComplete = true; },
                    Qt::QueuedConnection);
                connect(
                    m_orderStream,
                    &Stream::streamClosed,
                    this,
                    [this]() { m_orderStreamClosed = true; },
                    Qt::QueuedConnection);
                connect(
                    m_orderStream,
                    &StreamOrders::newOrderReceived,
                    this,
                    [this](const Order& p_order)
                    {
                        m_orderUpdates.insert(p_order.getOrderID(), p_order);
                        if (p_order.getOrderStatus() == Order::Status::FLL ||
                            p_order.getOrderStatus() == Order::Status::FPR ||
                            p_order.getOrderStatus() == Order::Status::FLP)
                            m_filledOrders.insert(p_order.getOrderID());
                    },
                    Qt::QueuedConnection);
            },
            Qt::BlockingQueuedConnection));
        QVERIFY(m_orderStream);
        QTRY_VERIFY_WITH_TIMEOUT(m_orderSnapshotComplete || m_orderStreamClosed,
                                 TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(m_orderSnapshotComplete && !m_orderStreamClosed);

        PlaceOrderRequest request;
        request.setAccountID(configuration->account);
        request.setSymbol(configuration->symbol);
        request.setQuantity(1);
        request.setTradeAction(TradeAction::Buy);
        request.setOrderType(OrderType::Type::Limit);
        request.setLimitPrice(configuration->limitPrice);
        request.setTimeInForce(TimeInForce(OrderDuration::Day));
        QVERIFY(request.isValid());
        // Recheck the account/endpoint immediately before the only placement.
        QCOMPARE(MainApp::getTradingMode(), TradingMode::Sim);
        QVERIFY(TradeStationOrderTestPolicy::isSimulationEndpoint(client->getApiBaseUrl()));
        m_placement = client->placeOrder(request);
        QTRY_VERIFY_WITH_TIMEOUT(m_placement->isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        rememberPlacedOrderIDs();
        QVERIFY2(m_placement->result().has_value(), "Placement failed or is ambiguous; do not retry automatically.");
        const auto result = m_placement->result().value();
        QVERIFY(!result.hasErrors());
        QCOMPARE(result.getOrders().size(), qsizetype(1));
        QVERIFY(!result.getOrders().first().isError());
        QCOMPARE(m_createdOrderIDs.size(), qsizetype(1));
        const QString id = *m_createdOrderIDs.cbegin();
        QTRY_VERIFY_WITH_TIMEOUT(m_orderUpdates.contains(id) || m_orderStreamClosed,
                                 TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(!m_orderStreamClosed);
        QVERIFY(m_orderUpdates.contains(id));
        const Order observed = m_orderUpdates.constFind(id).value();
        QCOMPARE(observed.getAccountID(), configuration->account);
        QCOMPARE(observed.getSymbol(), configuration->symbol);
        QVERIFY2(!isTerminal(observed.getOrderStatus()), "Test order was already terminal before cancellation.");
        auto cancel = client->cancelOrder(id);
        QTRY_VERIFY_WITH_TIMEOUT(cancel.isFinished(), TradeStationApiTestConstants::REQUEST_TIMEOUT_MS);
        QVERIFY(cancel.result().has_value());
        QVERIFY(!cancel.result()->isError());
        QCOMPARE(cancel.result()->getOrderID(), id);
        QTRY_VERIFY_WITH_TIMEOUT(isCreatedOrderTerminal(id) || m_orderStreamClosed,
                                 TradeStationApiTestConstants::ORDER_CLEANUP_TIMEOUT_MS);
        QVERIFY(isCreatedOrderTerminal(id));
        QVERIFY2(!m_filledOrders.contains(id),
                 "The paper order filled; existing positions are not liquidated by tests.");
        const auto status = m_orderUpdates.constFind(id)->getOrderStatus();
        QVERIFY(status == Order::Status::CAN || status == Order::Status::OUT || status == Order::Status::TSC);
    }

    void cleanupTestCase()
    {
        if (m_clientCreated)
        {
            cleanupCreatedOrders();
            closeOwnedStream(TSClient::getInstance(), m_orderStream);
            TSClient::destroyInstance();
        }
        appStateSettings = nullptr;
    }

  private:
    static std::expected<TradeStationOrderTestPolicy::Configuration, QString> orderConfiguration()
    {
        return TradeStationOrderTestPolicy::configuration(
            qEnvironmentVariable(TradeStationApiTestConstants::OPT_IN_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_OPT_IN_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ACCOUNT_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_SYMBOL_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_PRICE_ENV));
    }

    static bool isTerminal(Order::Status p_status)
    {
        return p_status == Order::Status::CAN || p_status == Order::Status::EXP || p_status == Order::Status::FLL ||
               p_status == Order::Status::OUT || p_status == Order::Status::REJ || p_status == Order::Status::TSC ||
               p_status == Order::Status::BRO || p_status == Order::Status::FLP;
    }

    bool isCreatedOrderTerminal(const QString& p_id) const
    {
        const auto it = m_orderUpdates.constFind(p_id);
        return it != m_orderUpdates.cend() && isTerminal(it->getOrderStatus());
    }

    void rememberPlacedOrderIDs()
    {
        if (!m_placement.has_value() || !m_placement->isFinished() || !m_placement->result().has_value())
            return;
        const auto result = m_placement->result();
        for (const auto& order: result->getOrders())
        {
            if (!order.getOrderID().isEmpty())
                m_createdOrderIDs.insert(order.getOrderID());
        }
    }

    void cleanupCreatedOrders()
    {
        if (!m_placement)
            return;
        QElapsedTimer timer;
        timer.start();
        while (!m_placement->isFinished() && timer.elapsed() < TradeStationApiTestConstants::ORDER_CLEANUP_TIMEOUT_MS)
            QTest::qWait(10);
        rememberPlacedOrderIDs();
        if (m_createdOrderIDs.isEmpty())
        {
            QTest::qFail("No test-created order ID was recovered. Placement may be ambiguous: inspect the dedicated "
                         "paper account manually; the test will not guess IDs or repeat placement.",
                         __FILE__,
                         __LINE__);
            return;
        }
        for (const QString& id: m_createdOrderIDs)
        {
            if (!isCreatedOrderTerminal(id))
            {
                auto future = TSClient::getInstance()->cancelOrder(id);
                timer.restart();
                while (!future.isFinished() && timer.elapsed() < TradeStationApiTestConstants::ORDER_CLEANUP_TIMEOUT_MS)
                    QTest::qWait(10);
                if (!future.isFinished() || !future.result().has_value() || future.result()->isError())
                    QTest::qFail(qPrintable(QString("Cleanup cancellation failed for test-created order %1. Inspect "
                                                    "the dedicated paper account manually.")
                                                .arg(id)),
                                 __FILE__,
                                 __LINE__);
                timer.restart();
                while (!isCreatedOrderTerminal(id) &&
                       timer.elapsed() < TradeStationApiTestConstants::ORDER_CLEANUP_TIMEOUT_MS)
                    QTest::qWait(10);
            }
            if (!isCreatedOrderTerminal(id))
                QTest::qFail(qPrintable(QString("No terminal update for test-created order %1. Manual inspection "
                                                "is required.")
                                            .arg(id)),
                             __FILE__,
                             __LINE__);
            if (m_filledOrders.contains(id))
                QTest::qFail("A test-created paper order filled. Tests never liquidate existing positions; "
                             "inspect the paper position manually.",
                             __FILE__,
                             __LINE__);
        }
    }

    template<typename StreamType> static void closeOwnedStream(TSClient* p_client, QPointer<StreamType> p_stream)
    {
        if (!QMetaObject::invokeMethod(
                p_client,
                [p_client, p_stream]()
                {
                    if (p_stream)
                        p_client->closeStream(p_stream);
                },
                Qt::BlockingQueuedConnection))
            qFatal("Could not schedule API fixture stream cleanup");
    }

    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
    bool m_clientCreated = false;
    QString m_account;
    QPointer<StreamOrders> m_orderStream;
    bool m_orderSnapshotComplete = false;
    bool m_orderStreamClosed = false;
    QMap<QString, Order> m_orderUpdates;
    QSet<QString> m_createdOrderIDs;
    QSet<QString> m_filledOrders;
    std::optional<QFuture<std::expected<PlaceOrderResult, TSClient::Error>>> m_placement;
};

int main(int p_argc, char** p_argv)
{
    QCoreApplication application(p_argc, p_argv);
    if (qEnvironmentVariable(TradeStationApiTestConstants::OPT_IN_ENV) != "1")
    {
        qInfo("TradeStation API smoke test skipped: explicit opt-in is required.");
        return 77;
    }
    const bool selectsOrders = application.arguments().contains("paperOrderLifecycle");
    if (selectsOrders)
    {
        if (qEnvironmentVariable(TradeStationApiTestConstants::ORDER_OPT_IN_ENV) != "1")
        {
            qInfo("Paper-order test skipped: separate order opt-in is required.");
            return 77;
        }
        const auto configuration = TradeStationOrderTestPolicy::configuration(
            qEnvironmentVariable(TradeStationApiTestConstants::OPT_IN_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_OPT_IN_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ACCOUNT_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_SYMBOL_ENV),
            qEnvironmentVariable(TradeStationApiTestConstants::ORDER_PRICE_ENV));
        if (!configuration.has_value())
        {
            qCritical().noquote() << configuration.error();
            return 1;
        }
    }
    TradeStationApiSmokeTests tests;
    return QTest::qExec(&tests, p_argc, p_argv);
}

#include "TradeStationApiSmokeTests.moc"
