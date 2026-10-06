#include <QSignalSpy>
#include <QTest>

#include "BarAggregator.h"
#include "CONSTANTS.h"

class BarAggregatorTests : public QObject
{
    Q_OBJECT

    static QDateTime timestamp(const QDate& p_date, const QTime& p_time)
    {
        return QDateTime(p_date, p_time, TradingHours::MARKET_TIMEZONE);
    }

    static QVector<Bar> barsFor(const QSignalSpy& p_spy, TimeFrame p_timeFrame)
    {
        QVector<Bar> bars;
        for (const auto& arguments: p_spy)
        {
            if (qvariant_cast<TimeFrame>(arguments.at(0)) == p_timeFrame)
            {
                bars.append(qvariant_cast<Bar>(arguments.at(1)));
            }
        }
        return bars;
    }

  private slots:
    void intradayAggregation_data()
    {
        QTest::addColumn<TimeFrame>("timeFrame");
        QTest::addColumn<int>("minutes");
        QTest::newRow("5m") << TimeFrame::FIVE_MINUTES << 5;
        QTest::newRow("15m") << TimeFrame::FIFTEEN_MINUTES << 15;
        QTest::newRow("30m") << TimeFrame::THIRTY_MINUTES << 30;
        QTest::newRow("1h") << TimeFrame::ONE_HOUR << 60;
        QTest::newRow("4h") << TimeFrame::FOUR_HOURS << 240;
    }

    void intradayAggregation()
    {
        QFETCH(TimeFrame, timeFrame);
        QFETCH(int, minutes);
        BarAggregator aggregator;
        QSignalSpy updated(&aggregator, &BarAggregator::barUpdated);
        QSignalSpy closed(&aggregator, &BarAggregator::barClosed);
        QVERIFY(updated.isValid());
        QVERIFY(closed.isValid());
        const auto start = timestamp(QDate(2026, 10, 5), QTime(4, 0));

        for (int minute = 0; minute < minutes; ++minute)
        {
            const float price = 10.0f + static_cast<float>(minute);
            aggregator.onNewBar("TEST", Bar(start.addSecs(minute * 60), price, price + 2, price - 1, price + 1, 100));
            QCOMPARE(barsFor(closed, timeFrame).size(), minute == minutes - 1 ? 1 : 0);
        }

        const Bar expected(start,
                           10,
                           static_cast<float>(minutes + 11),
                           9,
                           static_cast<float>(minutes + 10),
                           static_cast<quint64>(minutes) * 100);
        const auto completed = barsFor(closed, timeFrame);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first(), expected);
        auto expectedUpdate = expected;
        expectedUpdate.setBarStatus(Bar::BarStatus::Open);
        const auto updates = barsFor(updated, timeFrame);
        QCOMPARE(updates.size(), minutes);
        QCOMPARE(updates.last(), expectedUpdate);
        QVERIFY(barsFor(updated, TimeFrame::ONE_MINUTE).isEmpty());
        QVERIFY(barsFor(closed, TimeFrame::ONE_MINUTE).isEmpty());

        const auto nextStart = start.addSecs(minutes * 60);
        const Bar next(nextStart, 50, 52, 49, 51, 7);
        aggregator.onNewBar("TEST", next);
        auto nextUpdate = next;
        nextUpdate.setBarStatus(Bar::BarStatus::Open);
        QCOMPARE(barsFor(updated, timeFrame).last(), nextUpdate);
        QCOMPARE(barsFor(closed, timeFrame).size(), 1);
    }

    void calendarCloseBoundaries_data()
    {
        QTest::addColumn<TimeFrame>("timeFrame");
        QTest::addColumn<QDate>("date");
        QTest::addColumn<bool>("closes");
        QTest::newRow("daily") << TimeFrame::ONE_DAY << QDate(2026, 10, 5) << true;
        QTest::newRow("weekly-thursday") << TimeFrame::ONE_WEEK << QDate(2026, 10, 8) << false;
        QTest::newRow("weekly-friday") << TimeFrame::ONE_WEEK << QDate(2026, 10, 9) << true;
        QTest::newRow("monthly-mid-month") << TimeFrame::ONE_MONTH << QDate(2026, 10, 9) << false;
        QTest::newRow("monthly-before-weekend") << TimeFrame::ONE_MONTH << QDate(2026, 10, 30) << true;
        QTest::newRow("monthly-weekday-end") << TimeFrame::ONE_MONTH << QDate(2026, 9, 30) << true;
        QTest::newRow("monthly-year-end") << TimeFrame::ONE_MONTH << QDate(2026, 12, 31) << true;
    }

    void calendarCloseBoundaries()
    {
        QFETCH(TimeFrame, timeFrame);
        QFETCH(QDate, date);
        QFETCH(bool, closes);
        BarAggregator aggregator;
        QSignalSpy closed(&aggregator, &BarAggregator::barClosed);
        const auto end = timestamp(date, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);
        aggregator.onNewBar("TEST", Bar(end.addSecs(-60), 10, 12, 9, 11, 100));
        QVERIFY(barsFor(closed, timeFrame).isEmpty());
        aggregator.onNewBar("TEST", Bar(end, 11, 14, 8, 13, 200));
        const auto completed = barsFor(closed, timeFrame);
        QCOMPARE(completed.size(), closes ? 1 : 0);
        if (closes)
        {
            QCOMPARE(completed.first(), Bar(end.addSecs(-60), 10, 14, 8, 13, 300));
        }
    }

    void liveUpdatesDoNotDoubleCountVolume()
    {
        BarAggregator aggregator;
        QSignalSpy updated(&aggregator, &BarAggregator::barUpdated);
        QSignalSpy closed(&aggregator, &BarAggregator::barClosed);
        const auto start = timestamp(QDate(2026, 10, 5), QTime(4, 0));
        aggregator.onBarUpdated("TEST", Bar(start, 10, 12, 9, 11, 50, Bar::BarStatus::Open));
        aggregator.onBarUpdated("TEST", Bar(start, 10, 14, 8, 13, 100, Bar::BarStatus::Open));
        QCOMPARE(barsFor(updated, TimeFrame::FIVE_MINUTES).last(), Bar(start, 10, 14, 8, 13, 0, Bar::BarStatus::Open));
        aggregator.onNewBar("TEST", Bar(start, 10, 14, 8, 13, 100));
        aggregator.onBarUpdated("TEST", Bar(start.addSecs(60), 13, 16, 12, 15, 999, Bar::BarStatus::Open));
        QCOMPARE(barsFor(updated, TimeFrame::FIVE_MINUTES).last(),
                 Bar(start, 10, 16, 8, 15, 100, Bar::BarStatus::Open));
        QVERIFY(closed.isEmpty());
        for (int minute = 1; minute < 5; ++minute)
        {
            aggregator.onNewBar("TEST", Bar(start.addSecs(minute * 60), 13, 16, 12, 15, 200));
        }
        const auto completed = barsFor(closed, TimeFrame::FIVE_MINUTES);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed.first(), Bar(start, 10, 16, 8, 15, 900));
    }

    void ignoresPlaceholderBarsAndNonOpenTickUpdates()
    {
        BarAggregator aggregator;
        QSignalSpy updated(&aggregator, &BarAggregator::barUpdated);
        QSignalSpy closed(&aggregator, &BarAggregator::barClosed);
        const auto start = timestamp(QDate(2026, 10, 5), QTime(4, 0));
        aggregator.onNewBar("TEST", Bar());
        aggregator.onNewBar("TEST", Bar::nullBar(start));
        aggregator.onBarUpdated("TEST", Bar());
        aggregator.onBarUpdated("TEST", Bar::nullBar(start));
        aggregator.onBarUpdated("TEST", Bar(start, 10, 12, 9, 11, 100));
        QVERIFY(updated.isEmpty());
        QVERIFY(closed.isEmpty());
        aggregator.onNewBar("TEST", Bar(start, 10, 12, 9, 11, 100));
        QCOMPARE(updated.size(), 8);
        QCOMPARE(barsFor(updated, TimeFrame::FIVE_MINUTES).first(),
                 Bar(start, 10, 12, 9, 11, 100, Bar::BarStatus::Open));
    }
};

QTEST_GUILESS_MAIN(BarAggregatorTests)
#include "BarAggregatorTests.moc"
