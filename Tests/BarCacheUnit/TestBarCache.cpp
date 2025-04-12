#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestBarCache.h"
#include "TSClient.h"
#include "BarCache.h"

static TSClient* client;

static BarCache *cache;

// will be called to create a global test data table.
void TestBarCache::initTestCase_data()
{
}

// will be called before the first test function is executed.
void TestBarCache::initTestCase() {
    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    cache = new BarCache("AAPL", false);

    bool triggered;

    QSignalSpy authStateSpy(client, &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

    QVERIFY(client->isCleanedUp());
    QVERIFY(!client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // ****** This will kickstart the refresh logic
    client->start();

    // Wait 1ms, just to make sure the client thread has started and
    QTest::qWait(1);

    // Verify the refresh has started
    QVERIFY(!client->isAuthenticated());
    QVERIFY(client->isAuthInProgress()); // The client will immediately initiate a refresh


    // Wait for authStateChanged signal to be fired
    // 2 second is generous for one refresh request round trip
    // When debugging, 2 seconds wasnt enough. Bumbed it to 4.
    // This is because the start of the suite is slow as the debugger sets up
    // the maaaany runtime thangs the app links to
    triggered = authStateSpy.wait(4000);

    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(authStateSpy.count(), 1);

    // Authentication completed
    bool success   = authStateSpy.first().at(0).toBool();
    QString reason = authStateSpy.first().at(1).toString();

    qInfo() << "Refresh reply : " << reason;
    authStateSpy.removeFirst();

    QVERIFY(success);

    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Wait just a little bit, for some reason this test thread outruns the housekeeping done in the TSClient that cleans the serviced
    // replies. Otherwise, the following isCleanedUp() triggers because the refreshTokenReply is not flushed from the map<>
    QTest::qWait(100);

    // Verify no resources were leaked
    QVERIFY(client->isCleanedUp());
}

// Will be called before each test function is executed.
void TestBarCache::init()
{

}

// Will be called after every test function.
void TestBarCache::cleanup() {

}

void TestBarCache::testGetAfterHourBars()
{
    // Define 4:00 PM (16:00)
    const QTime fourPM(16, 0, 0);
    // Define 4:04 PM (16:04)
    const QTime fourPM4(16, 4, 0);

    QDateTime firstDate;
    QDateTime lastDate;

    {
        QDateTime now = QDateTime::currentDateTime();

        // Get the date for the previous day
        QDate previousDay = now.date().addDays(-1);

        // Create a QDateTime for previous day at 4:00 PM in New York time zone
        QTimeZone newYorkTimeZone("America/New_York");

        firstDate = QDateTime(previousDay, fourPM, newYorkTimeZone);
        lastDate = QDateTime(previousDay, fourPM4, newYorkTimeZone);
    }


    QVector<Bar> results;

    qInfo() << "";
    qInfo() << "Warm the cache with 4 bars";
    {
        results = cache->getBars(firstDate, lastDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache->getLastNumberFetchedBars(), 4);
        QCOMPARE_GE(results.size(), 4);
    }

    qInfo() << "";
    qInfo() << "Test asking for the same 4 bars hits the cache";
    {
        results = cache->getBars(firstDate, lastDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache->getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), 4);
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past triggers a miss";
    QDateTime secondLastDate = lastDate.addSecs(10*60);
    {
        results = cache->getBars(lastDate, secondLastDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache->getLastNumberFetchedBars(), 10);
        QCOMPARE_GE(results.size(), 10);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 14 is a hit";
    {
        results = cache->getBars(firstDate, secondLastDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache->getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), 14);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar past is a miss and only that one is fetched";
    {
        results = cache->getBars(firstDate, secondLastDate.addSecs(60));

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 15);
        QCOMPARE(cache->getLastNumberFetchedBars(), 1);
        QCOMPARE(cache->getDuplicateStoreCount(), 0);
    }

}
