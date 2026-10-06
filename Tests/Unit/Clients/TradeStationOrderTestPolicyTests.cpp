#include <QTest>

#include "../../Integration/TradeStation/TradeStationOrderTestPolicy.h"

namespace Policy = TradeStationOrderTestPolicy;

class TradeStationOrderTestPolicyTests : public QObject
{
    Q_OBJECT

  private slots:
    void requiresBothExplicitOptIns()
    {
        for (const QString& api: {QString(), QString("0"), QString("true"), QString("1")})
        {
            for (const QString& orders: {QString(), QString("0"), QString("true"), QString("1")})
            {
                const auto result = Policy::configuration(api, orders, "SIM2956555M", "SPY", "1.00");
                QCOMPARE(result.has_value(), api == "1" && orders == "1");
            }
        }
    }

    void rejectsWrongAccountsAndMissingOrMalformedInputs()
    {
        for (const QString& account:
             {QString(), QString("SIM123456"), QString("2956555M"), QString("SIM2956555M "), QString("LIVE2956555M")})
            QVERIFY(!Policy::configuration("1", "1", account, "SPY", "1.00").has_value());
        for (const QString& symbol:
             {QString(), QString("spy"), QString("/ES"), QString("SPY,QQQ"), QString("SPY\n"), QString("SPY OPTION")})
            QVERIFY(!Policy::configuration("1", "1", "SIM2956555M", symbol, "1.00").has_value());
        for (const QString& price: {QString(),
                                    QString("0"),
                                    QString("-1"),
                                    QString("nan"),
                                    QString("inf"),
                                    QString("1e309"),
                                    QString("1.0bad")})
            QVERIFY(!Policy::configuration("1", "1", "SIM2956555M", "SPY", price).has_value());
        const auto valid = Policy::configuration("1", "1", "SIM2956555M", "BRK.B", "12.34");
        QVERIFY(valid.has_value());
        QCOMPARE(valid->symbol, QString("BRK.B"));
        QCOMPARE(valid->limitPrice, 12.34);
    }

    void rejectsNonSimulationEndpoints()
    {
        QVERIFY(Policy::isSimulationEndpoint(QUrl("https://sim-api.tradestation.com/v3/")));
        for (const QString& url: {QString("https://api.tradestation.com/v3"),
                                  QString("http://sim-api.tradestation.com/v3"),
                                  QString("https://sim-api.tradestation.com.evil.example/v3"),
                                  QString("https://sim-api.tradestation.com/v2"),
                                  QString("https://user@sim-api.tradestation.com/v3"),
                                  QString("https://sim-api.tradestation.com:8443/v3")})
            QVERIFY(!Policy::isSimulationEndpoint(QUrl(url)));
    }
};

QTEST_GUILESS_MAIN(TradeStationOrderTestPolicyTests)
#include "TradeStationOrderTestPolicyTests.moc"
