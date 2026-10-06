#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
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
    void discoversSimulationAccount()
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
        bool ready = false;
        bool rejected = false;
        const auto connection = connect(
            client,
            &TSClient::authStateChanged,
            this,
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
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
    bool m_clientCreated = false;
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
