#include <QTest>
#include <QTimer>
#include <QSignalSpy>

#include "CONSTANTS.h"
#include "StreamReceiver.h"

class TestStreamReceiver : public StreamReceiver
{
  public:
    using StreamReceiver::beginRecovery;
    using StreamReceiver::completeRecovery;
    using StreamReceiver::stopRecovery;

    QTimer* recoveryTimer()
    {
        const auto timers = findChildren<QTimer*>();
        for (QTimer* timer: timers)
        {
            if (timer->interval() == StreamConstants::BROKERAGE_RECOVERY_TIMEOUT_MS)
            {
                return timer;
            }
        }
        return nullptr;
    }

  protected:
    QPointer<Stream> getStreamBase() const override
    {
        return nullptr;
    }
};

class StreamRecoveryTests : public QObject
{
    Q_OBJECT

  private slots:
    void deadlineIsNotRestarted()
    {
        TestStreamReceiver receiver;
        receiver.beginRecovery("Positions", "test");
        QTimer* timer = receiver.recoveryTimer();
        QVERIFY(timer != nullptr);
        QCOMPARE(timer->interval(), 15000);
        QTest::qWait(30);
        const int remaining = timer->remainingTime();
        receiver.beginRecovery("Positions", "test");
        QVERIFY(timer->remainingTime() <= remaining);
    }

    void completedSnapshotCancelsDeadline()
    {
        TestStreamReceiver receiver;
        receiver.beginRecovery("Positions", "test");
        QTest::ignoreMessage(QtInfoMsg, "\"Positions\" stream recovered for account \"test\" - snapshot complete");
        receiver.completeRecovery();
        QVERIFY(!receiver.recoveryTimer()->isActive());
        receiver.beginRecovery("Positions", "test");
        QVERIFY(receiver.recoveryTimer()->isActive());
    }

    void timeoutReportsCriticalOnce()
    {
        TestStreamReceiver receiver;
        QTimer* timer = receiver.recoveryTimer();
        QVERIFY(timer != nullptr);
        QSignalSpy timeout(timer, &QTimer::timeout);
        receiver.beginRecovery("Positions", "test");
        QTest::ignoreMessage(
            QtCriticalMsg,
            "\"Positions\" stream recovery failed for account \"test\" - no completed snapshot within 15000 ms; reconnect attempts continue");
        timer->start(1);
        QTRY_COMPARE(timeout.count(), 1);
        QVERIFY(!timer->isActive());
        receiver.beginRecovery("Positions", "test");
        QVERIFY(!timer->isActive());
    }

    void intentionalStopCancelsDeadline()
    {
        TestStreamReceiver receiver;
        receiver.beginRecovery("Orders", "test");
        receiver.stopRecovery();
        QVERIFY(!receiver.recoveryTimer()->isActive());
    }
};

QTEST_GUILESS_MAIN(StreamRecoveryTests)
#include "StreamRecoveryTests.moc"
