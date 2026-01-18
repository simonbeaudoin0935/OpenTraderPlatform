#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>
#include <QTimeZone>

#include "TestBarCache.h"
#include "TSClient.h"
#include "BarCache.h"
#include "Logging.h"

static TSClient* client;
static QVector<QString> companies;

// will be called to create a global test data table.
void TestBarCache::initTestCase_data()
{
    companies.push_back("AVTE");
    companies.push_back("ELTX");
    companies.push_back("GNLX");
    companies.push_back("MAZE");
    companies.push_back("FDMT");
    companies.push_back("SEER");
    companies.push_back("QNRX");
    companies.push_back("ELAB");
    companies.push_back("REVB");
    companies.push_back("GLTO");
    companies.push_back("ADTX");
    companies.push_back("SXTP");
    companies.push_back("CELZ");
    companies.push_back("TCRT");
    companies.push_back("HCWB");
    companies.push_back("CYCN");
    companies.push_back("BCDA");
    companies.push_back("APLM");
    companies.push_back("QLGN");
}

// will be called before the first test function is executed.
void TestBarCache::initTestCase()
{

    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");


    // manually expire the token to force a refresh on startup
    {
        AuthToken savedAuthToken = AuthToken::loadFromSettings();

        QString tokenStr = savedAuthToken.toString();
        QString obfuscated = tokenStr.size() > 10 ? tokenStr.left(10) + "***" : tokenStr + "***";
        qDebug() << "Token details:" << obfuscated;

        // This test suite neet a valid token to be present in the settings.
        QVERIFY(savedAuthToken.isValid());

        // The first thing we are going to test is that the client is able to
        // start with an expired token and refresh it.
        // If the present token is not expired, we will artificially set it to expired
        if (!savedAuthToken.isExpired())
        {
            qInfo() << "Token is not expired, setting it to expired";

            AuthToken expiredToken = AuthToken(savedAuthToken.getAccessToken(),
                                               savedAuthToken.getRefreshToken(),
                                               savedAuthToken.getIdToken(),
                                               savedAuthToken.getTokenType(),
                                               savedAuthToken.getScope(),
                                               savedAuthToken.getExpiresIn(),
                                               QDateTime::currentDateTime().addSecs(-3600));

            QVERIFY(AuthToken::storeToSettings(expiredToken));
        }
        else
        {
            qInfo() << "Token is expired, no need to set it to expired";
        }
    }


    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    bool triggered;

    QSignalSpy authStateSpy(client,
                            &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

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
    bool success = authStateSpy.first().at(0).toBool();
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
    // QTest installs its own message handler which overrides our colored logging.
    // Reinstall our custom colored message handler before each test to ensure
    // proper colored output during test execution.
    reinstallColoredMessageHandler();
}

// Will be called after every test function.
void TestBarCache::cleanup() {}

void TestBarCache::testGetBars()
{
    BarCache cache("AAPL");
    cache.clearDatabase();

    QDateTime initDate;
    QDateTime fromDate;
    QDateTime toDate;

    {
        //QDateTime now = QDateTime::currentDateTime();

        // Get the date for the previous day
        QDate previousDay(2025, 4, 11);

        // Create a QDateTime for previous day at 4:00 PM in New York time zone
        QTimeZone newYorkTimeZone("America/New_York");

        // Define 4:00 PM (16:00)
        const QTime noon(12, 0, 0);


        initDate = QDateTime(previousDay, noon, newYorkTimeZone);
        fromDate = initDate;
        toDate = fromDate;

        qInfo() << "Init Date : " << initDate;
    }


    QVector<Bar> results;

    qsizetype nbars;

    qInfo() << "";
    qInfo() << "Warm the cache with 4 bars";
    {
        nbars = 4;
        toDate = fromDate.addSecs(60 * (nbars - 1));
        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Test asking for the same 4 bars hits the cache";
    {
        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past triggers a miss";
    {
        nbars = 10;
        fromDate = toDate.addSecs(60);
        toDate = fromDate.addSecs(60 * (nbars - 1));

        results = cache.getBars(fromDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE_GE(results.size(), nbars);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 14 is a hit";
    {
        results = cache.getBars(initDate, toDate);

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE_GE(results.size(), 14);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar past is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache.getBars(initDate, toDate.addSecs(60 * nbars));

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 15);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache.getDuplicateStoreCount(), 0);
    }

    qInfo() << "";
    qInfo() << "Testing asking for the 1 bar before is a miss and only that one is fetched";
    {
        nbars = 1;
        results = cache.getBars(initDate.addSecs(-60 * nbars), toDate.addSecs(60 * nbars));

        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE_GE(results.size(), 16);
        QCOMPARE(cache.getLastNumberFetchedBars(), nbars);
        QCOMPARE(cache.getDuplicateStoreCount(), 0);
    }
}

// Testing asking a range where we know there are only void bars
void TestBarCache::testGetBarsOnlyHoles()
{
    BarCache cache("TIVC");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");

    //  Test against wednesday April 2 shortly after 17h where there is a bar holes from 17h-17h39
    QDate date(2025, 04, 02);
    const QTime _17PM10(17, 10, 0);
    const QTime _17PM20(17, 19, 0);

    QDateTime fromDate = QDateTime(date, _17PM10, newYorkTimeZone);
    QDateTime toDate = QDateTime(date, _17PM20, newYorkTimeZone);


    qInfo() << "";
    qInfo() << "Testing asking for 10 bars not existing in the API database correctly return 10 void bars";
    {
        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(cache.getNumberOfBars(), (unsigned)10);

        for (auto& bar: result)
        {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Null);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for the same 10 void bars return a HIT";
    {
        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned)10);

        for (auto& bar: result)
        {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Null);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for 10 bars past the 10 we just asked is a partial HIT";
    {
        toDate = toDate.addSecs(10 * 60);

        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned)20);

        for (auto& bar: result)
        {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Null);
        }
    }

    qInfo() << "";
    qInfo() << "Testing asking for 5 bars before the 20 we just asked is a partial HIT";
    {
        fromDate = fromDate.addSecs(-5 * 60);

        QVector<Bar> result = cache.getBars(fromDate, toDate);

        // We know that these 10 bars dont exist in the API database.
        QCOMPARE(cache.getLastNumberFetchedBars(), 0);
        QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
        QCOMPARE(cache.getNumberOfBars(), (unsigned)25);

        for (auto& bar: result)
        {
            QVERIFY(bar.getBarStatus() == Bar::BarStatus::Null);
        }
    }
}

void TestBarCache::testGetBarsWithHoles()
{
    QSKIP("yoo");

    BarCache cache("TIVC");

    //  Test against wednesday April 2
    QDate date(2025, 04, 02);

    QTimeZone newYorkTimeZone("America/New_York");

    const QTime _4PM(16, 0, 0);
    const QTime _8PM(19, 59, 0);

    QDateTime fromDate = QDateTime(date, _4PM, newYorkTimeZone);
    QDateTime toDate = QDateTime(date, _8PM, newYorkTimeZone);


    QVector<Bar> result = cache.getBars(fromDate, toDate);

    // We know there are only 26 bars in the after hour session of that date
    QCOMPARE(cache.getLastNumberFetchedBars(), 26);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // We need to have received 4 hours of 60 bars each
    QCOMPARE(result.size(), 4 * 60);
}

// Cache hierarchy tests
void TestBarCache::testDatabasePersistence()
{
    qInfo() << "Testing database persistence across cache instances";

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime fromDate = QDateTime(date, noon, newYorkTimeZone);
    QDateTime toDate = fromDate.addSecs(60 * 9); // 10 bars

    // First cache instance - fetch bars and store in DB
    {
        BarCache cache1("AAPL");
        cache1.clearDatabase();

        QVector<Bar> results = cache1.getBars(fromDate, toDate);

        QCOMPARE(cache1.getLastHitType(), BarCache::HitType::Miss);
        QCOMPARE(results.size(), 10);
        QCOMPARE_GE(cache1.getLastNumberFetchedBars(), 1);
    } // cache1 destroyed here, only DB should persist

    // Second cache instance - should load from database
    {
        BarCache cache2("AAPL");

        // Memory cache is empty, but DB has the bars
        QVector<Bar> results = cache2.getBars(fromDate, toDate);

        // Should be a Hit because bars are loaded from database into memory
        QCOMPARE(cache2.getLastHitType(), BarCache::HitType::Hit);
        QCOMPARE(results.size(), 10);
        // No API fetch should happen
        QCOMPARE(cache2.getLastNumberFetchedBars(), 0);

        cache2.clearDatabase();
    }
}

void TestBarCache::testDatabaseOnlyHit()
{
    qInfo() << "Testing database-only hit scenario";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime fromDate = QDateTime(date, noon, newYorkTimeZone);
    QDateTime toDate = fromDate.addSecs(60 * 4); // 5 bars

    // First request - populate both memory and database
    QVector<Bar> results1 = cache.getBars(fromDate, toDate);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
    QCOMPARE(results1.size(), 5);

    // Clear memory cache by directly accessing the private member
    // Since we can't access private members, we'll create a new cache instance
    BarCache cache2("AAPL");

    // Request same range - should load from database
    QVector<Bar> results2 = cache2.getBars(fromDate, toDate);
    QCOMPARE(cache2.getLastHitType(), BarCache::HitType::Hit);
    QCOMPARE(results2.size(), 5);
    QCOMPARE(cache2.getLastNumberFetchedBars(), 0);

    cache2.clearDatabase();
}

void TestBarCache::testMixedCacheStates()
{
    qInfo() << "Testing mixed cache states (memory + DB + API)";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime start1 = QDateTime(date, noon, newYorkTimeZone);
    QDateTime end1 = start1.addSecs(60 * 4); // 5 bars

    // Fetch first range (will be in memory and DB)
    QVector<Bar> results1 = cache.getBars(start1, end1);
    QCOMPARE(results1.size(), 5);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // Create new cache to clear memory but keep DB
    BarCache cache2("AAPL");

    // Fetch overlapping but extended range
    // This should: load 5 from DB, fetch 5 from API
    QDateTime start2 = start1;
    QDateTime end2 = end1.addSecs(60 * 5); // 10 bars total

    start2.setTimeZone(newYorkTimeZone);
    end2.setTimeZone(newYorkTimeZone);

    QVector<Bar> results2 = cache2.getBars(start2, end2);
    QCOMPARE(results2.size(), 10);
    // Should be PartialHit because we had some bars in DB
    QCOMPARE(cache2.getLastHitType(), BarCache::HitType::PartialHit);
    QCOMPARE(cache2.getLastNumberFetchedBars(), 5);

    cache2.clearDatabase();
}

void TestBarCache::testCacheClearingAndRepopulation()
{
    qInfo() << "Testing cache clearing and repopulation";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime fromDate = QDateTime(date, noon, newYorkTimeZone);
    QDateTime toDate = fromDate.addSecs(60 * 9); // 10 bars

    // Initial population
    QVector<Bar> results1 = cache.getBars(fromDate, toDate);
    QCOMPARE(results1.size(), 10);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // Clear database
    cache.clearDatabase();

    // New cache instance (fresh memory, empty DB)
    BarCache cache2("AAPL");

    // Should fetch from API again
    QVector<Bar> results2 = cache2.getBars(fromDate, toDate);
    QCOMPARE(results2.size(), 10);
    QCOMPARE(cache2.getLastHitType(), BarCache::HitType::Miss);
    QCOMPARE_GE(cache2.getLastNumberFetchedBars(), 1);

    cache2.clearDatabase();
}

// Edge case tests
void TestBarCache::testSingleBarRequest()
{
    qInfo() << "Testing single bar request";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime barTime = QDateTime(date, noon, newYorkTimeZone);

    // Request a single bar (start == end)
    QVector<Bar> results = cache.getBars(barTime, barTime);

    QCOMPARE(results.size(), 1);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);
    QCOMPARE_GE(cache.getLastNumberFetchedBars(), 1);

    // Request same bar again - should hit
    QVector<Bar> results2 = cache.getBars(barTime, barTime);
    QCOMPARE(results2.size(), 1);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
    QCOMPARE(cache.getLastNumberFetchedBars(), 0);

    cache.clearDatabase();
}

void TestBarCache::testLargeRangeRequest()
{
    qInfo() << "Testing large range request (multiple hours)";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime time10AM(10, 0, 0);
    const QTime time2PM(14, 0, 0);

    QDateTime fromDate = QDateTime(date, time10AM, newYorkTimeZone);
    QDateTime toDate = QDateTime(date, time2PM, newYorkTimeZone);

    // Request 4 hours of bars (240 bars)
    QVector<Bar> results = cache.getBars(fromDate, toDate);

    qint64 expectedBars = fromDate.secsTo(toDate) / 60 + 1;
    QCOMPARE(results.size(), expectedBars);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // Verify bars are in chronological order
    for (int i = 1; i < results.size(); ++i)
    {
        QVERIFY(results[i - 1].getTimeStamp() < results[i].getTimeStamp());
        // Each bar should be exactly 60 seconds apart
        QCOMPARE(results[i - 1].getTimeStamp().secsTo(results[i].getTimeStamp()), 60);
    }

    cache.clearDatabase();
}

void TestBarCache::testOverlappingRequests()
{
    qInfo() << "Testing overlapping requests with different ranges";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime baseTime = QDateTime(date, noon, newYorkTimeZone);

    // Request 1: bars 0-9
    QVector<Bar> results1 = cache.getBars(baseTime, baseTime.addSecs(60 * 9));
    QCOMPARE(results1.size(), 10);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // Request 2: bars 5-14 (overlaps with 0-9)
    QVector<Bar> results2 = cache.getBars(baseTime.addSecs(60 * 5), baseTime.addSecs(60 * 14));
    QCOMPARE(results2.size(), 10);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);
    // Should only fetch bars 10-14 (5 bars)
    QCOMPARE(cache.getLastNumberFetchedBars(), 5);

    // Request 3: bars 0-14 (fully contained in cache now)
    QVector<Bar> results3 = cache.getBars(baseTime, baseTime.addSecs(60 * 14));
    QCOMPARE(results3.size(), 15);
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);
    QCOMPARE(cache.getLastNumberFetchedBars(), 0);

    cache.clearDatabase();
}

void TestBarCache::testBoundaryConditions()
{
    qInfo() << "Testing boundary conditions at trading hours";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);

    // Test at 6:00 AM (start of trading data)
    const QTime time6AM(6, 0, 0);
    QDateTime start6AM = QDateTime(date, time6AM, newYorkTimeZone);

    // Request first few bars of the day
    QVector<Bar> results = cache.getBars(start6AM, start6AM.addSecs(60 * 4));
    QCOMPARE(results.size(), 5);

    // Verify first bar timestamp (should be closing time of first minute bar)
    QCOMPARE(results.first().getTimeStamp(), start6AM.addSecs(60));

    cache.clearDatabase();
}

void TestBarCache::testNullBarsMixedWithRealBars()
{
    qInfo() << "Testing null bars mixed with real bars";

    BarCache cache("TIVC"); // Stock known to have null bars
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 04, 02);
    const QTime time4PM(16, 0, 0);
    const QTime time5PM(16, 59, 0);

    QDateTime fromDate = QDateTime(date, time4PM, newYorkTimeZone);
    QDateTime toDate = QDateTime(date, time5PM, newYorkTimeZone);

    // Request range that includes both null and real bars
    QVector<Bar> results = cache.getBars(fromDate, toDate);

    qint64 expectedBars = fromDate.secsTo(toDate) / 60 + 1;
    QCOMPARE(results.size(), expectedBars);

    // Count null vs real bars
    int nullCount = 0;
    int realCount = 0;
    for (const Bar& bar: results)
    {
        if (bar.getBarStatus() == Bar::BarStatus::Null)
        {
            nullCount++;
        }
        else
        {
            realCount++;
        }
    }

    qInfo() << "Found" << nullCount << "null bars and" << realCount << "real bars";

    // Verify consistency - requesting again should give same results
    QVector<Bar> results2 = cache.getBars(fromDate, toDate);
    QCOMPARE(results2.size(), results.size());
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);

    cache.clearDatabase();
}

void TestBarCache::testDuplicateStoreTracking()
{
    qInfo() << "Testing duplicate store count tracking";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime fromDate = QDateTime(date, noon, newYorkTimeZone);
    QDateTime toDate = fromDate.addSecs(60 * 4);

    // Initial fetch
    cache.getBars(fromDate, toDate);
    qsizetype initialDuplicates = cache.getDuplicateStoreCount();

    // Fetch same range - should be cache hit, no duplicates
    cache.getBars(fromDate, toDate);
    QCOMPARE(cache.getDuplicateStoreCount(), initialDuplicates);

    // The duplicate count should remain 0 for well-behaved cache operations
    QCOMPARE(cache.getDuplicateStoreCount(), qsizetype(0));

    cache.clearDatabase();
}

// Metrics validation tests
void TestBarCache::testHitTypeAccuracy()
{
    qInfo() << "Testing HitType accuracy in various scenarios";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime start = QDateTime(date, noon, newYorkTimeZone);

    // Scenario 1: Complete miss
    cache.getBars(start, start.addSecs(60 * 4));
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Miss);

    // Scenario 2: Complete hit
    cache.getBars(start, start.addSecs(60 * 4));
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);

    // Scenario 3: Partial hit (extend range)
    cache.getBars(start, start.addSecs(60 * 9));
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::PartialHit);

    // Scenario 4: Hit after partial hit
    cache.getBars(start, start.addSecs(60 * 9));
    QCOMPARE(cache.getLastHitType(), BarCache::HitType::Hit);

    cache.clearDatabase();
}

void TestBarCache::testMetricsTracking()
{
    qInfo() << "Testing comprehensive metrics tracking";

    BarCache cache("AAPL");
    cache.clearDatabase();

    QTimeZone newYorkTimeZone("America/New_York");
    QDate date(2025, 4, 11);
    const QTime noon(12, 0, 0);
    QDateTime start = QDateTime(date, noon, newYorkTimeZone);

    // Test 1: Fetch 10 bars
    cache.getBars(start, start.addSecs(60 * 9));
    QCOMPARE_GE(cache.getLastNumberFetchedBars(), qsizetype(1));
    QCOMPARE(cache.getNumberOfBars(), (unsigned)10);

    // Test 2: Hit should have 0 fetched
    cache.getBars(start, start.addSecs(60 * 9));
    QCOMPARE(cache.getLastNumberFetchedBars(), qsizetype(0));

    // Test 3: Extend by 5 bars
    cache.getBars(start, start.addSecs(60 * 14));
    QCOMPARE(cache.getLastNumberFetchedBars(), qsizetype(5));
    QCOMPARE(cache.getNumberOfBars(), (unsigned)15);

    cache.clearDatabase();
}

void TestBarCache::testBarStreaming()
{
    QSKIP("yo");

    QVector<BarCache*> caches;
    for (auto& company: companies)
    {
        BarCache* cache = new BarCache(company, true);
        cache->clearDatabase();
        caches.push_back(cache);
    }

    //    BarCache cache("AAPL", true);

    QTest::qWait(120000);

    for (auto& cache: caches)
    {
        qInfo() << "Number of bars for : " << cache->getSymbol() << " : " << cache->getNumberOfBars();
    }


    for (auto& cache: caches)
    {
        delete cache;
    }
}
