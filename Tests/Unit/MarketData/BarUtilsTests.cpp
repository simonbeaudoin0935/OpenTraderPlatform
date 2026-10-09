#include <QSignalSpy>
#include <QTest>

#include "BarAggregator.h"
#include "BarUtils.h"

class BarUtilsTests : public QObject
{
    Q_OBJECT

  private slots:
    void slotArithmetic_data()
    {
        QTest::addColumn<TimeFrame>("timeFrame");
        QTest::addColumn<int>("slotCount");
        QTest::newRow("1s") << TimeFrame::ONE_SECOND << 57600;
        QTest::newRow("1m") << TimeFrame::ONE_MINUTE << 960;
        QTest::newRow("5m") << TimeFrame::FIVE_MINUTES << 192;
        QTest::newRow("15m") << TimeFrame::FIFTEEN_MINUTES << 64;
        QTest::newRow("30m") << TimeFrame::THIRTY_MINUTES << 32;
        QTest::newRow("1h") << TimeFrame::ONE_HOUR << 16;
        QTest::newRow("4h") << TimeFrame::FOUR_HOURS << 4;
        QTest::newRow("1d") << TimeFrame::ONE_DAY << 1;
        QTest::newRow("1w") << TimeFrame::ONE_WEEK << 1;
        QTest::newRow("1M") << TimeFrame::ONE_MONTH << 1;
    }

    void slotArithmetic()
    {
        QFETCH(TimeFrame, timeFrame);
        QFETCH(int, slotCount);
        QCOMPARE(BarUtils::barsPerDay(timeFrame), slotCount);
        QCOMPARE(BarUtils::secondsPerBar(timeFrame), static_cast<int>(timeFrame));
        for (int index = 0; index < slotCount; ++index)
        {
            const auto time = BarUtils::indexToBarTime(timeFrame, index);
            QCOMPARE(BarUtils::barIndex(timeFrame, time), index);
        }
        QCOMPARE(BarUtils::barIndex(timeFrame, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION), 0);
        const int lastMinuteIndex =
            BarUtils::isIntradayTimeFrame(timeFrame) ? 57540 / BarUtils::secondsPerBar(timeFrame) : 0;
        QCOMPARE(BarUtils::barIndex(timeFrame, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION), lastMinuteIndex);
    }

    void sourceSelection()
    {
        for (auto tf: {TimeFrame::FIVE_MINUTES, TimeFrame::FIFTEEN_MINUTES, TimeFrame::THIRTY_MINUTES})
            QCOMPARE(BarUtils::aggregateSourceTimeFrame(tf), TimeFrame::ONE_MINUTE);
        QCOMPARE(BarUtils::aggregateSourceTimeFrame(TimeFrame::FOUR_HOURS), TimeFrame::ONE_HOUR);
        for (auto tf: {TimeFrame::ONE_WEEK, TimeFrame::ONE_MONTH})
            QCOMPARE(BarUtils::aggregateSourceTimeFrame(tf), TimeFrame::ONE_DAY);
        for (auto tf: {TimeFrame::ONE_SECOND, TimeFrame::ONE_MINUTE, TimeFrame::ONE_HOUR, TimeFrame::ONE_DAY})
        {
            QVERIFY(BarUtils::isNativeTimeFrame(tf));
            QCOMPARE(BarUtils::aggregateSourceTimeFrame(tf), tf);
        }
    }

    void calendarAlignment()
    {
        const QDateTime time(QDate(2026, 10, 9), QTime(9, 37, 29), TradingHours::MARKET_TIMEZONE);
        QCOMPARE(BarUtils::targetBarOpen(time, TimeFrame::FIVE_MINUTES),
                 QDateTime(time.date(), QTime(9, 35), time.timeZone()));
        QCOMPARE(BarUtils::targetBarOpen(time, TimeFrame::ONE_WEEK),
                 QDateTime(QDate(2026, 10, 5), QTime(4, 0), time.timeZone()));
        QCOMPARE(BarUtils::targetBarOpen(time, TimeFrame::ONE_MONTH),
                 QDateTime(QDate(2026, 10, 1), QTime(4, 0), time.timeZone()));
        QCOMPARE(BarUtils::targetBarOpen(time, TimeFrame::ONE_DAY),
                 QDateTime(time.date(), QTime(4, 0), time.timeZone()));
    }

    void sparseAggregation()
    {
        const QDateTime start(QDate(2026, 10, 5), QTime(4, 0), TradingHours::MARKET_TIMEZONE);
        const QVector<Bar> source{Bar(),
                                  Bar::nullBar(start),
                                  Bar(start.addSecs(60), 10, 12, 9, 11, 100),
                                  Bar(start.addSecs(180), 11, 14, 8, 13, 200),
                                  Bar(start.addSecs(300), 20, 22, 19, 21, 50)};
        const auto result = BarUtils::aggregateBars(source, TimeFrame::FIVE_MINUTES);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0], Bar(start, 10, 14, 8, 13, 300));
        QCOMPARE(result[1], source.last());
        QVERIFY(BarUtils::aggregateBars({}, TimeFrame::FIVE_MINUTES).isEmpty());
        QVERIFY(BarUtils::aggregateBars({Bar(), Bar::nullBar(start)}, TimeFrame::FIVE_MINUTES).isEmpty());
    }

    void historicalAndLiveAggregationAgree_data()
    {
        QTest::addColumn<TimeFrame>("timeFrame");
        QTest::addColumn<int>("minutes");
        QTest::newRow("5m") << TimeFrame::FIVE_MINUTES << 5;
        QTest::newRow("15m") << TimeFrame::FIFTEEN_MINUTES << 15;
        QTest::newRow("30m") << TimeFrame::THIRTY_MINUTES << 30;
        QTest::newRow("1h") << TimeFrame::ONE_HOUR << 60;
        QTest::newRow("4h") << TimeFrame::FOUR_HOURS << 240;
    }

    void historicalAndLiveAggregationAgree()
    {
        QFETCH(TimeFrame, timeFrame);
        QFETCH(int, minutes);
        BarAggregator aggregator;
        QSignalSpy closed(&aggregator, &BarAggregator::barClosed);
        QVERIFY(closed.isValid());
        QVector<Bar> source;
        const QDateTime start(QDate(2026, 10, 5), QTime(4, 0), TradingHours::MARKET_TIMEZONE);
        for (int index = 0; index < minutes * 2; ++index)
        {
            const float price = 10.0f + static_cast<float>(index % 7);
            source.append(Bar(start.addSecs(index * 60), price, price + 2, price - 1, price + 1, index + 1));
            aggregator.onNewBar("TEST", source.last());
        }
        QVector<Bar> live;
        for (const auto& arguments: closed)
            if (qvariant_cast<TimeFrame>(arguments[0]) == timeFrame)
                live.append(qvariant_cast<Bar>(arguments[1]));
        QCOMPARE(live.size(), 2);
        QCOMPARE(live, BarUtils::aggregateBars(source, timeFrame));
    }
};

QTEST_GUILESS_MAIN(BarUtilsTests)
#include "BarUtilsTests.moc"
