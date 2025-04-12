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
    QDateTime initDate;
    QDateTime fromDate;
    QDateTime toDate;

    {
        QDateTime now = QDateTime::currentDateTime();

        // Get the date for the previous day
        QDate previousDay = now.date().addDays(-1);

        // Create a QDateTime for previous day at 4:00 PM in New York time zone
        QTimeZone newYorkTimeZone("America/New_York");

        // Define 4:00 PM (16:00)
        const QTime noon(12, 0, 0);


        initDate = QDateTime(previousDay, noon, newYorkTimeZone);
        fromDate = initDate;
        toDate = fromDate;
    }


    QVector<Bar> results;

    int nbars;

    qInfo() << "";
    qInfo() << "Warm the cache with 4 bars";
    {
        nbars = 4;
        toDate = fromDate.addSecs(60 * (nbars-1));
        results = cache->getBars(fromDate, toDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache->getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Test asking for the same 4 bars hits the cache";
    {
        results = cache->getBars(fromDate, toDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache->getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past triggers a miss";
    {
        nbars = 10;
        fromDate = toDate.addSecs(60);
        toDate = fromDate.addSecs(60 * (nbars-1));

        results = cache->getBars(fromDate, toDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache->getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 14 is a hit";
    {
        results = cache->getBars(initDate, toDate);

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache->getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), 14);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar past is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache->getBars(initDate, toDate.addSecs(60 * nbars));

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 15);
        QCOMPARE(cache->getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache->getDuplicateStoreCount(), 0);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar before is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache->getBars(initDate.addSecs(-60 * nbars), toDate.addSecs(60 * nbars));

        QCOMPARE(cache->getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 16);
        QCOMPARE(cache->getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache->getDuplicateStoreCount(), 0);
    }
}
