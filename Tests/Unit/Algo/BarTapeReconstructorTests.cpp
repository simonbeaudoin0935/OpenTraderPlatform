#include <QTest>

#include "BarTapeReconstructor.h"
#include "CONSTANTS.h"

class BarTapeReconstructorTests : public QObject
{
    Q_OBJECT

    static QDateTime minute(const int p_minute)
    {
        return QDateTime(QDate(2026, 10, 9), QTime(10, p_minute), TradingHours::MARKET_TIMEZONE);
    }

    static Bar bar(const int p_minute,
                   const float p_close,
                   const quint64 p_volume,
                   const Bar::BarStatus p_status = Bar::BarStatus::Open)
    {
        return Bar(minute(p_minute), p_close, p_close, p_close, p_close, p_volume, p_status);
    }

    static const inline BarTapeReconstructor::Bbo s_bbo{0.1756, 0.1759};

  private slots:
    void firstUpdateIsBaselineOnly()
    {
        BarTapeReconstructor tape;
        QVERIFY(!tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8)).has_value());
    }

    void volumeDeltaBecomesPrint()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));

        const auto print = tape.onBar("SXTC", bar(8, 0.1759f, 1600), s_bbo, minute(8));
        QVERIFY(print.has_value());
        QCOMPARE(print->m_symbol, QString("SXTC"));
        QCOMPARE(print->m_size, 600);
        QCOMPARE(print->m_price, 0.1759);
        QCOMPARE(print->m_side, TradeSide::Bid);
    }

    void unchangedOrShrinkingVolumeIsIgnored()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));
        QVERIFY(!tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8)).has_value());
        QVERIFY(!tape.onBar("SXTC", bar(8, 0.1758f, 900), s_bbo, minute(8)).has_value());

        const auto print = tape.onBar("SXTC", bar(8, 0.1756f, 1100), s_bbo, minute(8));
        QVERIFY(print.has_value());
        QCOMPARE(print->m_size, 100);
        QCOMPARE(print->m_side, TradeSide::Ask);
    }

    void closedUpdateCountsRemainingVolume()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));

        const auto print = tape.onBar("SXTC", bar(8, 0.1757f, 1250, Bar::BarStatus::Closed), s_bbo, minute(9));
        QVERIFY(print.has_value());
        QCOMPARE(print->m_size, 250);
        QCOMPARE(print->m_side, TradeSide::None);
    }

    void newMinutePrintsWholeBarVolume()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));

        const auto print = tape.onBar("SXTC", bar(9, 0.1759f, 300), s_bbo, minute(9));
        QVERIFY(print.has_value());
        QCOMPARE(print->m_size, 300);

        const auto next = tape.onBar("SXTC", bar(9, 0.1759f, 450), s_bbo, minute(9));
        QVERIFY(next.has_value());
        QCOMPARE(next->m_size, 150);
    }

    void staleOlderMinuteIsIgnored()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));
        (void)tape.onBar("SXTC", bar(9, 0.1759f, 300), s_bbo, minute(9));

        QVERIFY(!tape.onBar("SXTC", bar(8, 0.1757f, 5000), s_bbo, minute(9)).has_value());

        const auto print = tape.onBar("SXTC", bar(9, 0.1759f, 400), s_bbo, minute(9));
        QVERIFY(print.has_value());
        QCOMPARE(print->m_size, 100);
    }

    void resetRestoresBaselineBehaviour()
    {
        BarTapeReconstructor tape;
        (void)tape.onBar("SXTC", bar(8, 0.1759f, 1000), s_bbo, minute(8));
        tape.reset();
        QVERIFY(!tape.onBar("SXTC", bar(8, 0.1759f, 5000), s_bbo, minute(8)).has_value());
    }

    void classifyAgainstBbo()
    {
        QCOMPARE(BarTapeReconstructor::classify(0.1759, s_bbo), TradeSide::Bid);
        QCOMPARE(BarTapeReconstructor::classify(0.1760, s_bbo), TradeSide::Bid);
        QCOMPARE(BarTapeReconstructor::classify(0.1756, s_bbo), TradeSide::Ask);
        QCOMPARE(BarTapeReconstructor::classify(0.1750, s_bbo), TradeSide::Ask);
        QCOMPARE(BarTapeReconstructor::classify(0.1757, s_bbo), TradeSide::None);
        QCOMPARE(BarTapeReconstructor::classify(0.1759, std::nullopt), TradeSide::None);
        QCOMPARE(BarTapeReconstructor::classify(0.1759, BarTapeReconstructor::Bbo{0.0, 0.1759}), TradeSide::None);
    }
};

QTEST_GUILESS_MAIN(BarTapeReconstructorTests)
#include "BarTapeReconstructorTests.moc"
