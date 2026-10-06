#include <QTest>
#include <QTimeZone>

#include "BarHistoryBackfill.h"

class BarHistoryBackfillTests : public QObject
{
    Q_OBJECT

    const QDateTime m_dayStart{QDate(2026, 10, 5), QTime(4, 0), QTimeZone("America/New_York")};

    Bar closed(int p_minutes) const
    {
        return Bar(m_dayStart.addSecs(p_minutes * 60), 10, 11, 9, 10, 100);
    }

  private slots:
    void replacementStatusMatrix()
    {
        const QList<Bar::BarStatus> statuses{Bar::BarStatus::Uninitialized,
                                             Bar::BarStatus::Null,
                                             Bar::BarStatus::Open,
                                             Bar::BarStatus::Closed};
        const bool expected[4][4] = {{true, true, true, true},
                                     {true, true, true, true},
                                     {false, false, false, true},
                                     {false, false, false, true}};
        for (qsizetype existing = 0; existing < statuses.size(); ++existing)
        {
            for (qsizetype incoming = 0; incoming < statuses.size(); ++incoming)
            {
                Bar oldBar = closed(0);
                Bar newBar = closed(0);
                oldBar.setBarStatus(statuses[existing]);
                newBar.setBarStatus(statuses[incoming]);
                QCOMPARE(BarHistoryBackfill::shouldReplace(oldBar, newBar), expected[existing][incoming]);
            }
        }
    }

    void unsortedSnapshotSelectsLatestClosed()
    {
        const auto start =
            BarHistoryBackfill::start({closed(100), closed(360), closed(200)}, m_dayStart, closed(399).getTimeStamp());
        QVERIFY(start.has_value());
        QCOMPARE(*start, closed(360).getTimeStamp());
    }

    void emptyCacheStartsAtSessionOpen()
    {
        const auto start = BarHistoryBackfill::start({}, m_dayStart, m_dayStart.addSecs(60 * 400));
        QVERIFY(start.has_value());
        QCOMPARE(*start, m_dayStart);
    }

    void staleCacheStartsAtLastSavedCandle()
    {
        const QVector<Bar> saved{closed(0), closed(350), closed(360), Bar::nullBar(m_dayStart.addSecs(60 * 400))};
        const auto start = BarHistoryBackfill::start(saved, m_dayStart, m_dayStart.addSecs(60 * 399));
        QVERIFY(start.has_value());
        QCOMPARE(*start, closed(360).getTimeStamp());
    }

    void freshCacheNeedsNoFetch()
    {
        QVERIFY(!BarHistoryBackfill::start({closed(399)}, m_dayStart, closed(399).getTimeStamp()).has_value());
    }

    void liveArrivalDoesNotAdvanceCapturedAnchor()
    {
        QVector<Bar> saved{closed(360)};
        const auto start = BarHistoryBackfill::start(saved, m_dayStart, closed(399).getTimeStamp());
        saved.append(closed(399));
        QCOMPARE(*start, closed(360).getTimeStamp());
    }

    void formingCandleDoesNotProveHistoryCoverage()
    {
        Bar forming = closed(400);
        forming.setBarStatus(Bar::BarStatus::Open);
        const auto start = BarHistoryBackfill::start({closed(360), forming}, m_dayStart, closed(399).getTimeStamp());
        QVERIFY(start.has_value());
        QCOMPARE(*start, closed(360).getTimeStamp());
    }

    void successfulEmptyTailDoesNotRefetch()
    {
        const auto verified = closed(399).getTimeStamp();
        QVERIFY(!BarHistoryBackfill::start({closed(360)}, m_dayStart, verified, verified).has_value());
        const auto start = BarHistoryBackfill::start({closed(360)}, m_dayStart, closed(400).getTimeStamp(), verified);
        QCOMPARE(*start, verified);
    }

    void beforeSessionOpenNeedsNoFetch()
    {
        QVERIFY(!BarHistoryBackfill::start({}, m_dayStart, m_dayStart.addSecs(-60)).has_value());
    }

    void mergePreservesLiveBarsAndRestoresDowntime()
    {
        const Bar live = closed(400);
        const Bar placeholder = Bar::nullBar(live.getTimeStamp());
        QVERIFY(!BarHistoryBackfill::shouldReplace(live, placeholder));
        QVERIFY(!BarHistoryBackfill::shouldReplace(live, Bar()));
        Bar forming = live;
        forming.setBarStatus(Bar::BarStatus::Open);
        QVERIFY(!BarHistoryBackfill::shouldReplace(live, forming));
        QVERIFY(!BarHistoryBackfill::shouldReplace(forming, forming));
        QVERIFY(BarHistoryBackfill::shouldReplace(forming, live));
        QVERIFY(BarHistoryBackfill::shouldReplace(placeholder, live));

        QVector<Bar> cached{closed(360),
                            Bar::nullBar(closed(361).getTimeStamp()),
                            Bar::nullBar(closed(362).getTimeStamp()),
                            closed(363)};
        const QVector<Bar> fetched{closed(360), closed(361), closed(362), Bar::nullBar(closed(363).getTimeStamp())};
        for (qsizetype i = 0; i < cached.size(); ++i)
        {
            if (BarHistoryBackfill::shouldReplace(cached[i], fetched[i]))
            {
                cached[i] = fetched[i];
            }
        }
        for (qsizetype i = 0; i < cached.size(); ++i)
        {
            QCOMPARE(cached[i].getBarStatus(), Bar::BarStatus::Closed);
        }
        QCOMPARE(cached.last(), closed(363));
    }
};

QTEST_GUILESS_MAIN(BarHistoryBackfillTests)
#include "BarHistoryBackfillTests.moc"
