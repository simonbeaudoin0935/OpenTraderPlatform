#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#include "CONSTANTS.h"

class TradeStationOptInGuardTests : public QObject
{
    Q_OBJECT

  private slots:
    void rejectsBeforeFixtureConstruction_data()
    {
        QTest::addColumn<QString>("apiOptIn");
        QTest::addColumn<QString>("orderOptIn");
        QTest::addColumn<QString>("account");
        QTest::addColumn<QString>("symbol");
        QTest::addColumn<QString>("price");
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("exitCode");
        QTest::newRow("api-disabled") << "" << "" << "SIM2956555M" << "SPY" << "1.00" << "discoversSimulationAccount"
                                      << 77;
        QTest::newRow("api-nonexact-optin")
            << "true" << "1" << "SIM2956555M" << "SPY" << "1.00" << "paperOrderLifecycle" << 77;
        QTest::newRow("order-disabled") << "1" << "" << "SIM2956555M" << "SPY" << "1.00" << "paperOrderLifecycle" << 77;
        QTest::newRow("wrong-account") << "1" << "1" << "SIM123456" << "SPY" << "1.00" << "paperOrderLifecycle" << 1;
        QTest::newRow("missing-symbol") << "1" << "1" << "SIM2956555M" << "" << "1.00" << "paperOrderLifecycle" << 1;
        QTest::newRow("missing-price") << "1" << "1" << "SIM2956555M" << "SPY" << "" << "paperOrderLifecycle" << 1;
        QTest::newRow("nonfinite-price") << "1" << "1" << "SIM2956555M" << "SPY" << "nan" << "paperOrderLifecycle" << 1;
    }

    void rejectsBeforeFixtureConstruction()
    {
        QFETCH(QString, apiOptIn);
        QFETCH(QString, orderOptIn);
        QFETCH(QString, account);
        QFETCH(QString, symbol);
        QFETCH(QString, price);
        QFETCH(QString, scenario);
        QFETCH(int, exitCode);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(TradeStationApiTestConstants::OPT_IN_ENV, apiOptIn);
        environment.insert(TradeStationApiTestConstants::ORDER_OPT_IN_ENV, orderOptIn);
        environment.insert(TradeStationApiTestConstants::ACCOUNT_ENV, account);
        environment.insert(TradeStationApiTestConstants::ORDER_SYMBOL_ENV, symbol);
        environment.insert(TradeStationApiTestConstants::ORDER_PRICE_ENV, price);
        environment.insert("XDG_CONFIG_HOME", directory.path());
        environment.insert("DBUS_SESSION_BUS_ADDRESS", "unix:path=" + directory.path() + "/no-keyring-bus");
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(OTP_TRADESTATION_TEST_EXECUTABLE, {scenario});
        QVERIFY(process.waitForStarted(5000));
        if (!process.waitForFinished(5000))
        {
            process.kill();
            QVERIFY(process.waitForFinished(5000));
            QFAIL("Opt-in guard did not exit before fixture/authentication startup.");
        }
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), exitCode);
        const QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
        QVERIFY2(!output.contains("Start testing of"), "Guard unexpectedly entered the authenticated Qt Test fixture.");
    }
};

QTEST_GUILESS_MAIN(TradeStationOptInGuardTests)
#include "TradeStationOptInGuardTests.moc"
