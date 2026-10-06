#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QScopeGuard>
#include <cmath>
#include <memory>

#include "AuthToken.h"
#include "ClientToken.h"
#include "MainApp.h"
#include "SecureStorage.h"
#include "Settings.h"
#include "TSClient.h"

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

    void cleanupTestCase()
    {
        if (m_clientCreated)
        {
            TSClient::destroyInstance();
        }
        appStateSettings = nullptr;
    }

  private:
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
};

int main(int p_argc, char** p_argv)
{
    QCoreApplication application(p_argc, p_argv);
    if (qEnvironmentVariable(TradeStationApiTestConstants::OPT_IN_ENV) != "1")
    {
        qInfo("TradeStation API smoke test skipped: explicit opt-in is required.");
        return 77;
    }
    TradeStationApiSmokeTests tests;
    return QTest::qExec(&tests, p_argc, p_argv);
}

#include "TradeStationApiSmokeTests.moc"
