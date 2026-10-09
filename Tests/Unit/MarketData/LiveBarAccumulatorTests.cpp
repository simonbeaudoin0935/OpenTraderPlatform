#include <QSignalSpy>
#include <QTest>

#include "LiveBarAccumulator.h"
#include "CONSTANTS.h"

class LiveBarAccumulatorTests : public QObject
{
    Q_OBJECT

    static Trade trade(const QString& p_symbol, const QDateTime& p_time, double p_price, int p_size)
    {
        return Trade{p_symbol, p_time, p_price, p_size, TradeSide::None};
    }

  private slots:
    void intervalRollover_data()
    {
        QTest::addColumn<int>("interval");
        QTest::newRow("1m") << 60;
    }

    void intervalRollover()
    {
        QFETCH(int, interval);
        LiveBarAccumulator accumulator(nullptr, interval);
        QSignalSpy updated(&accumulator, &LiveBarAccumulator::barUpdated);
        QSignalSpy closed(&accumulator, &LiveBarAccumulator::barClosed);
        QVERIFY(updated.isValid());
        QVERIFY(closed.isValid());
        const QDateTime start(QDate(2026, 10, 5), QTime(9, 30), TradingHours::MARKET_TIMEZONE);
        QVERIFY(!accumulator.getFormingBar("TEST").has_value());
        accumulator.onNewTrade("TEST", trade("TEST", start.addMSecs(1234).toUTC(), 10, 2));
        accumulator.onNewTrade("TEST", trade("TEST", start.addSecs(3), 12, 3));
        accumulator.onNewTrade("TEST", trade("TEST", start.addSecs(interval - 1), 9, 4));
        QVERIFY(closed.isEmpty());
        QCOMPARE(updated.size(), 3);
        const auto forming = accumulator.getFormingBar("TEST");
        QVERIFY(forming.has_value());
        QCOMPARE(*forming, Bar(start, 10, 12, 9, 9, 9, Bar::BarStatus::Open));
        accumulator.onNewTrade("TEST", trade("TEST", start.addSecs(interval), 20, 5));
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed[0][0].toString(), QString("TEST"));
        QCOMPARE(qvariant_cast<Bar>(closed[0][1]), Bar(start, 10, 12, 9, 9, 9));
        const auto next = accumulator.getFormingBar("TEST");
        QVERIFY(next.has_value());
        QCOMPARE(*next, Bar(start.addSecs(interval), 20, 20, 20, 20, 5, Bar::BarStatus::Open));
    }

    void symbolsAreIndependentAndGapsDoNotInventTrades()
    {
        LiveBarAccumulator accumulator;
        QSignalSpy closed(&accumulator, &LiveBarAccumulator::barClosed);
        const QDateTime start(QDate(2026, 10, 5), QTime(9, 30), TradingHours::MARKET_TIMEZONE);
        accumulator.onNewTrade("AAA", trade("AAA", start, 10, 2));
        accumulator.onNewTrade("BBB", trade("BBB", start, 50, 7));
        accumulator.onNewTrade("AAA", trade("AAA", start.addSecs(300), 12, 3));
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed[0][0].toString(), QString("AAA"));
        const auto bbb = accumulator.getFormingBar("BBB");
        QVERIFY(bbb.has_value());
        QCOMPARE(*bbb, Bar(start, 50, 50, 50, 50, 7, Bar::BarStatus::Open));
        const auto aaa = accumulator.getFormingBar("AAA");
        QVERIFY(aaa.has_value());
        QCOMPARE(aaa->getTimeStamp(), start.addSecs(300));
    }

    void nextSessionStartsNewBar()
    {
        LiveBarAccumulator accumulator;
        QSignalSpy closed(&accumulator, &LiveBarAccumulator::barClosed);
        const QDateTime end(QDate(2026, 10, 5), QTime(19, 59), TradingHours::MARKET_TIMEZONE);
        const QDateTime next(QDate(2026, 10, 6), QTime(4, 0), TradingHours::MARKET_TIMEZONE);
        accumulator.onNewTrade("TEST", trade("TEST", end, 10, 1));
        accumulator.onNewTrade("TEST", trade("TEST", next, 20, 2));
        QCOMPARE(closed.size(), 1);
        QCOMPARE(qvariant_cast<Bar>(closed[0][1]), Bar(end, 10, 10, 10, 10, 1));
        const auto forming = accumulator.getFormingBar("TEST");
        QVERIFY(forming.has_value());
        QCOMPARE(*forming, Bar(next, 20, 20, 20, 20, 2, Bar::BarStatus::Open));
    }
};

QTEST_GUILESS_MAIN(LiveBarAccumulatorTests)
#include "LiveBarAccumulatorTests.moc"
